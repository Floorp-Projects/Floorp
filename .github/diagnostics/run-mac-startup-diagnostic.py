#!/usr/bin/env python3
"""Run one unchanged authenticated macOS startup case, then a separate crash backtrace."""
import argparse, hashlib, importlib.util, json, os, pathlib, plistlib, shutil, signal, socket, subprocess, sys, time

EXPECTED = "f7ef0b832d79ed793b16421914d579307f76271c"
EXPECTED_BUILD = "20261019183619"

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n")

def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()

def processes():
    data = subprocess.check_output(["/bin/ps", "-axo", "pid=,ppid=,pgid=,stat=,lstart=,comm="], text=True, timeout=10)
    result = {}
    for line in data.splitlines():
        parts = line.strip().split(None, 9)
        if len(parts) == 10:
            result[int(parts[0])] = {"pid": int(parts[0]), "parent": int(parts[1]),
                                     "group": int(parts[2]), "state": parts[3], "started": " ".join(parts[4:9]), "command": parts[9]}
    return result

def bounded(argv, cwd, output, timeout, env):
    output.mkdir(parents=True, exist_ok=False)
    observed, timed_out = {}, False
    with (output / "supervisor.log").open("wb") as log:
        process = subprocess.Popen([str(x) for x in argv], cwd=cwd, env=env,
                                   stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        deadline = time.monotonic() + timeout
        try:
            while process.poll() is None:
                table = processes()
                owned = {process.pid}
                changed = True
                while changed:
                    before = len(owned)
                    owned.update(pid for pid, row in table.items()
                                 if row["parent"] in owned or row["group"] == process.pid)
                    changed = len(owned) != before
                observed.update({pid: table[pid] for pid in owned if pid in table})
                if time.monotonic() >= deadline or (output / "supervisor.log").stat().st_size > 8 * 1024 * 1024:
                    timed_out = True
                    break
                time.sleep(0.2)
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            # Only this fresh session's surviving process group is eligible.
            table = processes()
            surviving = [row for row in table.values() if row["group"] == process.pid and not row["state"].startswith("Z")]
            if surviving:
                os.killpg(process.pid, signal.SIGKILL)
            for pid, row in observed.items():
                current = table.get(pid)
                if (current and current["started"] == row["started"] and current["command"] == row["command"] and
                        current["group"] != process.pid and pid != process.pid and not current["state"].startswith("Z")):
                    os.kill(pid, signal.SIGKILL)
            time.sleep(0.2)
            final = [row for pid, row in processes().items() if not row["state"].startswith("Z") and
                     (row["group"] == process.pid or
                      (pid in observed and row["started"] == observed[pid]["started"] and
                       row["command"] == observed[pid]["command"]))]
    result = {"argv": [str(x) for x in argv], "exitCode": process.returncode,
              "watchdogStopped": timed_out, "sessionLeader": process.pid,
              "observedOwnedProcesses": list(observed.values()), "survivingOwnedGroup": final}
    save(output / "supervisor.json", result)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--artifact", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    if sys.platform != "darwin":
        raise RuntimeError("This diagnostic requires genuine macOS")
    root = args.source_root.resolve(strict=True)
    artifact = args.artifact.resolve(strict=True)
    output = args.output.resolve()
    if output.is_relative_to(root):
        raise ValueError("Diagnostic helper/output must remain outside authenticated source")
    output.mkdir(parents=True, exist_ok=False)
    helper_path = pathlib.Path(__file__).resolve()
    if helper_path.is_relative_to(root):
        raise ValueError("Diagnostic helper must remain outside authenticated source")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    if commit != EXPECTED or subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root):
        raise RuntimeError("Expected exact clean immutable old production source")
    manifests = list(artifact.rglob("candidate-manifest.json"))
    if len(manifests) != 1:
        raise RuntimeError("Expected exactly one original artifact manifest")
    manifest_path = manifests[0]
    manifest = json.loads(manifest_path.read_text())
    if (manifest.get("floorpCommit") != EXPECTED or manifest.get("buildId") != EXPECTED_BUILD or
            manifest.get("platform") != "mac" or manifest.get("target") != "aarch64-apple-darwin"):
        raise RuntimeError("Original production artifact identity differs")
    ci = load("original_test_candidate", root / "tools/chrome-extension-runtime/ci/test-candidate.py")
    ci._identity.validate_maintained_patches(manifest, root)
    package_row = ci._selection.select_native_package(manifest["packages"], "mac")
    if (package_row["sha256"] != "a54ea072b9dbffd616fdb7f2cfcc18517db731fd410736efc2f7ba87918e5a6f" or
            package_row["bytes"] != 129148995):
        raise RuntimeError("Not the previously verified old production DMG")
    packages = list(artifact.rglob(package_row["name"]))
    if (len(packages) != 1 or packages[0].is_symlink() or
            packages[0].stat().st_size != package_row["bytes"] or sha(packages[0]) != package_row["sha256"]):
        raise RuntimeError("Original production DMG checksum/size differs")
    destination = root / "_dist/native-candidate-mac-crash-diagnostic"
    # Same readonly DMG/ditto extraction as maintained CI, with explicit process deadlines.
    destination.mkdir(parents=True, exist_ok=False)
    mount = destination / "mounted"
    mount.mkdir()
    attached = False
    try:
        subprocess.run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", str(mount), str(packages[0])], check=True, timeout=120)
        attached = True
        apps = list(mount.glob("*.app"))
        if len(apps) != 1 or apps[0].is_symlink():
            raise RuntimeError("Expected one regular original application bundle")
        subprocess.run(["ditto", str(apps[0]), str(destination/apps[0].name)], check=True, timeout=180)
    finally:
        detached = subprocess.run(["hdiutil", "detach", str(mount)], timeout=60)
        if attached and detached.returncode:
            raise RuntimeError("Original readonly DMG could not be detached")
    mount.rmdir()
    binary = ci.binary_in(destination, "mac")
    if sha(binary) != "4837f9a6bbb6f82db03c937e0d53fd9e451e59b30df286e8e725d1b0e29dc979":
        raise RuntimeError("Not the previously verified old production executable")
    app = next(path for path in binary.parents if path.suffix == ".app")
    first_output = root / "_dist/chrome-runtime-tests/mac-crash-diagnostic-original"
    started = time.time()
    environment = dict(os.environ)
    for name in ("MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_HEADLESS", "FLOORP_TEST_RUNTIME_PATCH_OVERRIDE"):
        environment.pop(name, None)
    original_test = root / "tools/chrome-extension-runtime/ci/ProductionOverlay.test.mts"
    argv = [sys.executable, root / "tools/chrome-extension-runtime/run-browser-tests.py",
            "--browser", binary, "--candidate-manifest", manifest_path,
            "--output", first_output, "--timeout", "300", "--test", original_test]
    first = bounded(argv, root, output / "original", 540, environment)
    reports = list(first_output.glob("run-*/report.json"))
    report = json.loads(reports[0].read_text()) if len(reports) == 1 else {}
    candidate = report.get("candidate", {})
    summary = {"diagnosticOnly": True, "qualificationCredit": 0, "sourceCommit": commit,
               "originalArtifactRun": 39303630826, "originalArtifactId": 12217158366,
               "manifestSha256": sha(manifest_path), "packageSha256": sha(packages[0]),
               "originalModule": str(original_test), "originalModuleSha256": sha(original_test),
               "original": first, "originalReports": [str(path) for path in reports]}
    expected_inventory = candidate.get("applicationInventorySha256")
    def app_inventory():
        tree = ci._identity._tree_inventory(app)
        digest = hashlib.sha256(json.dumps(sorted(tree.items()), separators=(",", ":")).encode()).hexdigest()
        return {"files": len(tree), "sha256": digest}
    if expected_inventory:
        summary["afterOriginalAppInventory"] = app_inventory()
        if (summary["afterOriginalAppInventory"]["sha256"] != expected_inventory or
                sha(binary) != candidate.get("binarySha256") or candidate.get("manifest") != manifest):
            raise RuntimeError("App identity changed after original startup; no second launch permitted")
    collector = load("darwin_crash_collector", helper_path.with_name("collect-darwin-crashes.py"))
    pids = [row["pid"] for row in first["observedOwnedProcesses"] if row["command"].lower().startswith("floorp") or str(app) in row["command"]]
    try:
        collector.collect(root, output / "after-original", started, [binary], pids)
    except Exception as error:
        summary.setdefault("collectionErrors", []).append({"phase":"after-original","error":str(error)})
    # Separate traced startup only after an actual, authenticated original SIGSEGV.
    if report.get("hostExitCode") == -11 and expected_inventory and not first["survivingOwnedGroup"]:
        lldb = shutil.which("lldb")
        if lldb:
            profile = output / "lldb-profile"
            profile.mkdir(mode=0o700)
            user_js = reports[0].parent / "profile/user.js"
            prefs = user_js.read_text()
            with socket.socket() as stream:
                stream.bind(("127.0.0.1", 0))
                port = stream.getsockname()[1]
            import re
            prefs, replacements = re.subn(r'user_pref\("marionette.port", \d+\);',
                                           f'user_pref("marionette.port", {port});', prefs)
            if replacements != 1:
                raise RuntimeError("Expected original runner's single Marionette port preference")
            (profile / "user.js").write_text(prefs)
            second_environment = {**environment, "MOZ_CRASHREPORTER_DISABLE": "1"}
            command = [lldb, "--no-lldbinit", "--batch", "-o", "settings set target.disable-aslr false",
                       "-o", "run", "-o", "thread backtrace all", "-o", "process status", "--",
                       binary, "--no-remote", "--profile", profile, "--marionette",
                       "--remote-allow-system-access", "--headless", "about:blank"]
            summary["lldbSecondStartup"] = bounded(command, root, output / "lldb-second", 180, second_environment)
        else:
            summary["lldbSecondStartup"] = {"skipped": "Normal Apple LLDB unavailable"}
    else:
        summary["lldbSecondStartup"] = {"skipped": "First original authenticated browser did not segfault, identity unavailable, or owned group not closed"}
    summary["afterAllAppInventory"] = app_inventory()
    summary["appInventoryUnchanged"] = summary["afterAllAppInventory"]["sha256"] == expected_inventory if expected_inventory else None
    summary["sourceHeadAfter"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    summary["trackedStatusAfter"] = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root, text=True)
    try:
        collector.collect(root, output / "after-all", started, [binary], pids)
    except Exception as error:
        summary.setdefault("collectionErrors", []).append({"phase":"after-all","error":str(error)})
    save(output / "summary.json", summary)
    if summary["appInventoryUnchanged"] is False or summary["sourceHeadAfter"] != EXPECTED or summary["trackedStatusAfter"]:
        raise RuntimeError("Diagnostic app/source integrity changed")
    if summary["appInventoryUnchanged"] is None:
        raise RuntimeError("Original candidate authentication unavailable; app integrity comparison is unavailable")
    if first["survivingOwnedGroup"] or summary.get("lldbSecondStartup", {}).get("survivingOwnedGroup"):
        raise RuntimeError("Owned diagnostic processes remain")

if __name__ == "__main__":
    main()

