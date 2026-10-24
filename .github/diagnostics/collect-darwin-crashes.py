#!/usr/bin/env python3
"""Retain existing Darwin crash evidence for exact owned browser processes only."""
import argparse, datetime, hashlib, json, os, pathlib, re, selectors, shutil, signal, subprocess, time

MAX_FILE = 16 * 1024 * 1024
MAX_TOTAL = 64 * 1024 * 1024

def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")

def command(argv, output, timeout=30, cap=2 * 1024 * 1024):
    """Run only readonly native tools, retaining bounded stdout/stderr."""
    output.parent.mkdir(parents=True, exist_ok=True)
    process = subprocess.Popen(argv, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
    deadline, retained, bounded = time.monotonic() + timeout, 0, False
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    try:
        with output.open("wb") as stream:
            while True:
                if time.monotonic() >= deadline:
                    bounded = True
                    break
                ready = selector.select(timeout=0.2)
                if ready:
                    data = process.stdout.read1(65536)
                    if not data:
                        break
                    allowed = data[:max(0, cap-retained)]
                    stream.write(allowed)
                    retained += len(allowed)
                    if len(allowed) != len(data):
                        bounded = True
                        break
                elif process.poll() is not None:
                    break
    finally:
        selector.close()
        cleanup_error = None
        try:
            # Reap completed tools first. Never signal their former process group.
            process.wait(timeout=0.2)
        except subprocess.TimeoutExpired:
            try:
                if process.poll() is None and os.getpgid(process.pid) == process.pid:
                    os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            except PermissionError as error:
                cleanup_error = str(error)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                cleanup_error = cleanup_error or "Owned native tool remains after bounded cleanup"
        process.stdout.close()
    return {"argv": argv, "exitCode": process.returncode, "boundedStop": bounded,
            "retainedBytes": retained, "output": str(output), "cleanupError": cleanup_error,
            "ownedPid": process.pid, "stillRunning": process.poll() is None}

def collect(root, output, since, binaries, pids=()):
    root, output = pathlib.Path(root).resolve(), pathlib.Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    binaries = {str(pathlib.Path(value).resolve(strict=True)) for value in binaries}
    for value in binaries:
        if not pathlib.Path(value).is_relative_to(root / "_dist"):
            raise ValueError("Diagnostic binary must be inside this exact candidate output")
    result = {"diagnosticOnly": True, "sinceEpoch": since, "ownedBinaryPaths": sorted(binaries),
              "ownedPids": sorted(set(pids)), "reports": [], "tools": [],
              "limitations": ["No inference from missing OS reports; Firefox crash reporter may be disabled",
                              "Read-only metadata, no resigning, quarantine removal, pref or security changes"]}
    commands = [["/usr/bin/sw_vers"], ["/usr/bin/uname", "-a"]]
    for binary in sorted(binaries):
        commands += [["/usr/bin/file", binary], ["/usr/bin/otool", "-hv", binary], ["/usr/bin/otool", "-L", binary],
                     ["/usr/bin/codesign", "-d", "--verbose=4", binary],
                     ["/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", binary]]
        app = next((p for p in pathlib.Path(binary).parents if p.suffix == ".app"), None)
        if app:
            commands += [["/usr/bin/codesign", "-d", "--entitlements", ":-", str(app)],
                         ["/usr/bin/codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app)],
                         ["/usr/bin/xattr", "-l", str(app)]]
        if shutil.which("dwarfdump"):
            commands.append(["dwarfdump", "--uuid", binary])
    for index, argv in enumerate(commands):
        try:
            result["tools"].append(command(argv, output / f"native-tool-{index}.log"))
        except Exception as error:
            result["tools"].append({"argv": argv, "readError": str(error)})
    total = 0
    directories = [pathlib.Path.home() / "Library/Logs/DiagnosticReports",
                   pathlib.Path("/Library/Logs/DiagnosticReports")]
    for directory_index, directory in enumerate(directories):
        if not directory.is_dir():
            continue
        try:
            candidates = sorted(directory.iterdir(), key=lambda p: p.stat().st_mtime if not p.is_symlink() else 0, reverse=True)[:256]
        except OSError as error:
            result.setdefault("directoryReadErrors", []).append({"directory": str(directory), "error": str(error)})
            continue
        for path in candidates:
            if (path.is_symlink() or not path.is_file() or path.suffix not in (".ips", ".crash") or
                    not path.name.lower().startswith("floorp") or path.stat().st_mtime < since - 2 or
                    path.stat().st_size > MAX_FILE):
                continue
            data = path.read_bytes()
            text = data.decode("utf-8", errors="replace")
            process_path = None
            if path.suffix == ".ips":
                decoder, rest = json.JSONDecoder(), text.lstrip()
                try:
                    while rest:
                        obj, offset = decoder.raw_decode(rest)
                        if isinstance(obj, dict) and isinstance(obj.get("procPath"), str):
                            process_path = obj["procPath"]
                            break
                        rest = rest[offset:].lstrip()
                except ValueError:
                    pass
            else:
                match = re.search(r"^Path:\s+(.+)$", text, re.MULTILINE)
                process_path = match.group(1).strip() if match else None
            if process_path not in binaries:
                continue
            if total + len(data) > MAX_TOTAL:
                result["retentionLimitReached"] = True
                break
            destination = output / "darwin-reports" / str(directory_index) / path.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(data)
            total += len(data)
            result["reports"].append({"source": str(path), "processPath": process_path,
                                      "path": str(destination), "bytes": len(data),
                                      "sha256": hashlib.sha256(data).hexdigest()})
    for path in sorted((root / "_dist/chrome-runtime-tests").glob("**/profile/minidumps/*"))[:64]:
        if (path.is_symlink() or not path.is_file() or path.suffix not in (".dmp", ".extra") or
                path.stat().st_mtime < since - 2 or path.stat().st_size > MAX_FILE or
                not path.resolve().is_relative_to(root / "_dist/chrome-runtime-tests")):
            continue
        data = path.read_bytes()
        if total + len(data) > MAX_TOTAL:
            break
        destination = output / "minidumps" / path.relative_to(root / "_dist/chrome-runtime-tests")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        total += len(data)
        result["reports"].append({"source": str(path), "path": str(destination),
                                  "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    owned = sorted(set(int(pid) for pid in pids if isinstance(pid, int) and pid > 1))[:32]
    if owned:
        predicate = " OR ".join(f"processID == {pid}" for pid in owned)
        stamp = datetime.datetime.fromtimestamp(since).strftime("%Y-%m-%d %H:%M:%S")
        try:
            result["tools"].append(command(
                ["/usr/bin/log", "show", "--style", "json", "--info", "--start", stamp,
                 "--predicate", predicate], output / "owned-process-unified-log.json", timeout=30))
        except Exception as error:
            result["unifiedLogReadError"] = str(error)
    result["retainedCrashBytes"] = total
    save(output / "collection.json", result)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    reports = list((root / "_dist/chrome-runtime-tests").glob("**/report.json"))
    binaries, starts, pids = set(), [], []
    for path in reports:
        if path.is_symlink() or path.stat().st_size > MAX_FILE:
            continue
        value = json.loads(path.read_text())
        binary = value.get("browser")
        candidate = value.get("candidate", {})
        if (isinstance(binary, str) and isinstance(candidate, dict) and
                candidate.get("binaryPath") == binary and
                pathlib.Path(binary).is_file() and pathlib.Path(binary).resolve().is_relative_to(root / "_dist")):
            binaries.add(binary)
            profile = path.parent / "profile/user.js"
            if profile.is_file():
                starts.append(profile.stat().st_mtime)
            if isinstance(value.get("hostPid"), int):
                pids.append(value["hostPid"])
    if not binaries or not starts:
        save(args.output / "collection.json", {"diagnosticOnly": True,
             "skipped": "No owned authenticated browser reports/profile start timestamp", "reports": []})
        return
    collect(root, args.output, min(starts), binaries, pids)

if __name__ == "__main__":
    main()

