#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Observe original old-product Windows alerts and one reviewed portable XPI test.

The helper lives outside the authenticated f7ef checkout. It never changes that
checkout's tracked files or the packaged application. A fresh Windows Job owns
each runner and its descendants; a stdin barrier prevents child work before job
assignment. Native browser sandboxing and the original tests remain intact.
"""

import argparse
import ctypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys
import time
import zipfile


sys.dont_write_bytecode = True

EXPECTED_HEAD = "f7ef0b832d79ed793b16421914d579307f76271c"
EXPECTED_BUILD = "20261019183619"
EXPECTED_SOURCE = "7dd619742b25567e479eeda97ae071d10542ad0b"
EXPECTED_ARTIFACT = {
    "id": 12215177649,
    "name": "chrome-native-windows",
    "size_in_bytes": 155576376,
    "digest": "sha256:bc3784d574e2c0d5d6b13ac75eb3b521feec94a215d767add8716ad280bdb243",
}
EXPECTED_PACKAGE = {
    "name": "floorp-158.0.1.en-US.win64.zip", "bytes": 157122650,
    "sha256": "2bc04c17443e31eeef58498a9073874c47ddc2306f3698645f2c798242250ffd",
}
EXPECTED_BINARY = "7b582571d38f981f0983162b98b686df59ab86480b0169bfa8be670efcd3faa3"
EXPECTED_INVENTORY = "d2fd6d4a3b0a00c90f1aade7b18d6ef5dadeca3ebf94f7677da332202736ea61"
EXPECTED_GRAPH = "4a0387fe93ac8634b714998ea01f86717dd521a5534ffa8e72200b16ffd6682e"
EXPECTED_OBSERVER = "aa4da4e360459d540fe3516c7e56b64e34d43c19626a4d4b9ee5049aef055997"
TEST_PREFIX = "browser-features/modules/modules/chrome-web-store/test/"
NOTIFICATION_TEST = TEST_PREFIX + "ChromeNotificationsCloseNative.test.mts"
EXPECTED_NOTIFICATION = "6a86399fba5653672f6d2bdd99adbc1cbebb5a7d5bbcaa9b0871a5eed3f06936"


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def file_record(path):
    path = path.resolve(strict=True)
    return {"path": str(path), "sha256": sha(path), "bytes": path.stat().st_size}


def validate_artifact(metadata):
    if any(metadata.get(key) != value for key, value in EXPECTED_ARTIFACT.items()):
        raise RuntimeError("Not the exact original Windows artifact")
    workflow = metadata.get("workflow_run", {})
    if (metadata.get("expired") is not False or workflow.get("id") != 39303630826 or
            workflow.get("head_sha") != "1158af71726169ca6b07fbcc11612c70506e5b66" or
            workflow.get("head_branch") != "codex/chrome-extension-runtime-v2" or
            workflow.get("repository_id") != 437505831 or workflow.get("head_repository_id") != 437505831):
        raise RuntimeError("Original artifact run/branch/repository identity differs")


def clean_source(root):
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    status = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root, text=True)
    if head != EXPECTED_HEAD or status:
        raise RuntimeError("Expected exact clean original production source")
    return {"head": head, "trackedStatus": status}


def safe_zip_members(archive):
    members = archive.infolist()
    if not members or len(members) > 100000:
        raise ValueError("Unexpected original package entry count")
    seen, total = set(), 0
    for item in members:
        name = item.filename
        path = PurePosixPath(name)
        if (not name or "\\" in name or ":" in name or path.is_absolute() or
                ".." in path.parts or any(part.rstrip(" .") != part for part in path.parts)):
            raise ValueError("Unsafe original Windows package path")
        for part in path.parts:
            device = part.split(".", 1)[0].upper()
            if device in {"CON", "PRN", "AUX", "NUL", *[f"COM{i}" for i in range(1, 10)],
                          *[f"LPT{i}" for i in range(1, 10)]}:
                raise ValueError("Windows device path in original package")
        key = path.as_posix().casefold()
        if key in seen:
            raise ValueError("Duplicate Windows package path")
        seen.add(key)
        mode = stat.S_IFMT(item.external_attr >> 16)
        if mode not in (0, stat.S_IFREG, stat.S_IFDIR) or (mode == stat.S_IFDIR and not item.is_dir()):
            raise ValueError("Unsupported Windows package object")
        total += item.file_size
        if total > 2 * 1024 ** 3 or item.flag_bits & 1:
            raise ValueError("Unexpected encrypted/oversized original package")
    return members


def stage_fixture(root, diagnostic_root, graph_file):
    if sha(graph_file) != EXPECTED_GRAPH:
        raise RuntimeError("Reviewed source graph metadata changed")
    graph = json.loads(graph_file.read_text())
    if graph["candidateHead"] != EXPECTED_HEAD or len(graph["files"]) != 17:
        raise RuntimeError("Not the reviewed seventeen-file graph")
    destination = root / "_dist/chrome-runtime-tests/windows-old-fixture/source"
    destination.mkdir(parents=True, exist_ok=False)
    records, changed = [], 0
    for row in graph["files"]:
        relative = PurePosixPath(row["relative"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Source graph path escapes checkout")
        original = root / Path(relative)
        if original.is_symlink() or not original.is_file():
            raise RuntimeError("Candidate source input is not a physical regular file")
        if sha(original) != row["preimageSha256"] or original.stat().st_size != row["preimageBytes"]:
            raise RuntimeError(f"Original f7ef fixture input differs: {relative}")
        selected = diagnostic_root / Path(relative) if "postimageSha256" in row else original
        expected_hash = row.get("postimageSha256", row["preimageSha256"])
        expected_bytes = row.get("postimageBytes", row["preimageBytes"])
        if selected.is_symlink() or sha(selected) != expected_hash or selected.stat().st_size != expected_bytes:
            raise RuntimeError(f"Reviewed portable fixture input differs: {relative}")
        target = destination / Path(relative)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(selected.read_bytes())
        if sha(target) != expected_hash:
            raise RuntimeError("Physical fixture copy differs")
        changed += int("postimageSha256" in row)
        records.append({"original": file_record(original), "selected": file_record(selected),
                        "copied": file_record(target), "changed": "postimageSha256" in row})
    if changed != 2:
        raise RuntimeError("Exactly the reviewed test and types must change")
    return destination / TEST_PREFIX / "ChromeExtensionUpdateScheduler.test.mts", records


# Fixed Windows LLP64 layouts, also inspectable by source-only host tests.
DWORD, BOOL, HANDLE = ctypes.c_uint32, ctypes.c_int32, ctypes.c_void_p
SIZE_T = ctypes.c_size_t


class BasicLimits(ctypes.Structure):
    _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64), ("PerJobUserTimeLimit", ctypes.c_int64),
                ("LimitFlags", DWORD), ("MinimumWorkingSetSize", SIZE_T),
                ("MaximumWorkingSetSize", SIZE_T), ("ActiveProcessLimit", DWORD),
                ("Affinity", SIZE_T), ("PriorityClass", DWORD), ("SchedulingClass", DWORD)]


class IoCounters(ctypes.Structure):
    _fields_ = [(name, ctypes.c_uint64) for name in
                ("ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                 "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]


class ExtendedLimits(ctypes.Structure):
    _fields_ = [("BasicLimitInformation", BasicLimits), ("IoInfo", IoCounters),
                ("ProcessMemoryLimit", SIZE_T), ("JobMemoryLimit", SIZE_T),
                ("PeakProcessMemoryUsed", SIZE_T), ("PeakJobMemoryUsed", SIZE_T)]


class Accounting(ctypes.Structure):
    _fields_ = [(name, ctypes.c_int64) for name in
                ("TotalUserTime", "TotalKernelTime", "ThisPeriodTotalUserTime", "ThisPeriodTotalKernelTime")]
    _fields_ += [(name, DWORD) for name in
                 ("TotalPageFaultCount", "TotalProcesses", "ActiveProcesses", "TotalTerminatedProcesses")]


def bounded_windows(argv, cwd, environment, output, seconds=420):
    if sys.platform != "win32":
        raise RuntimeError("Actual Windows is required for the owned Job")
    output.mkdir(parents=True, exist_ok=False)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    for name, arguments, result in [
        ("CreateJobObjectW", [ctypes.c_void_p, ctypes.c_wchar_p], HANDLE),
        ("SetInformationJobObject", [HANDLE, ctypes.c_int, ctypes.c_void_p, DWORD], BOOL),
        ("AssignProcessToJobObject", [HANDLE, HANDLE], BOOL),
        ("QueryInformationJobObject", [HANDLE, ctypes.c_int, ctypes.c_void_p, DWORD, ctypes.c_void_p], BOOL),
        ("TerminateJobObject", [HANDLE, DWORD], BOOL), ("CloseHandle", [HANDLE], BOOL),
    ]:
        function = getattr(kernel, name)
        function.argtypes, function.restype = arguments, result
    job = kernel.CreateJobObjectW(None, None)
    if not job:
        raise ctypes.WinError(ctypes.get_last_error())
    process = None
    result = {"argv": [str(x) for x in argv], "watchdogStopped": False,
              "ownJobAssignedBeforeWork": False, "remainingOwned": None}
    try:
        limits = ExtendedLimits()
        limits.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel.SetInformationJobObject(job, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
            raise ctypes.WinError(ctypes.get_last_error())
        bootstrap = ("import json,subprocess,sys; p=json.load(sys.stdin); "
                     "sys.exit(subprocess.run(p['argv'],cwd=p['cwd']).returncode)")
        with (output / "supervisor.log").open("wb") as log:
            process = subprocess.Popen([sys.executable, "-c", bootstrap], stdin=subprocess.PIPE,
                                       stdout=log, stderr=subprocess.STDOUT, cwd=cwd, env=environment,
                                       creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            # The bootstrap blocks on stdin until its exact process handle belongs
            # to our Job. It cannot launch the browser or other child work earlier.
            if not kernel.AssignProcessToJobObject(job, HANDLE(int(process._handle))):
                raise ctypes.WinError(ctypes.get_last_error())
            result["ownJobAssignedBeforeWork"] = True
            result["bootstrapPid"] = process.pid
            started = time.monotonic()
            try:
                process.communicate(json.dumps({"argv": [str(x) for x in argv], "cwd": str(cwd)}).encode(),
                                    timeout=seconds)
            except subprocess.TimeoutExpired:
                result["watchdogStopped"] = True
                if not kernel.TerminateJobObject(job, 124):
                    raise ctypes.WinError(ctypes.get_last_error())
                process.wait(timeout=15)
            result.update({"exitCode": process.returncode, "elapsedSeconds": time.monotonic() - started})
            accounting = Accounting()
            if not kernel.QueryInformationJobObject(job, 1, ctypes.byref(accounting), ctypes.sizeof(accounting), None):
                raise ctypes.WinError(ctypes.get_last_error())
            result["activeOwnedAtRunnerExit"] = accounting.ActiveProcesses
            if accounting.ActiveProcesses and not kernel.TerminateJobObject(job, 125):
                raise ctypes.WinError(ctypes.get_last_error())
            deadline = time.monotonic() + 15
            while True:
                if not kernel.QueryInformationJobObject(job, 1, ctypes.byref(accounting), ctypes.sizeof(accounting), None):
                    raise ctypes.WinError(ctypes.get_last_error())
                if not accounting.ActiveProcesses or time.monotonic() >= deadline:
                    break
                time.sleep(0.1)
            result["remainingOwned"] = accounting.ActiveProcesses
    finally:
        if process is not None and process.poll() is None:
            if result["ownJobAssignedBeforeWork"]:
                kernel.TerminateJobObject(job, 126)
            else:
                # No stdin payload was supplied, hence this unassigned bootstrap
                # cannot have launched any browser or descendants.
                process.terminate()
            process.wait(timeout=15)
        kernel.CloseHandle(job)
        save(output / "supervisor.json", result)
    if result["remainingOwned"]:
        raise RuntimeError("Owned native diagnostic processes remain; no next phase allowed")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", required=True, type=Path)
    parser.add_argument("--artifact", required=True, type=Path)
    parser.add_argument("--artifact-metadata", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Actual Windows is required; no simulated native success")
    root = args.source_root.resolve(strict=True)
    artifact = args.artifact.resolve(strict=True)
    output = args.output.resolve()
    helper = Path(__file__).resolve()
    diagnostic_root = helper.parents[2]
    if helper.is_relative_to(root) or output.is_relative_to(root) or artifact.is_relative_to(root):
        raise ValueError("Diagnostic helpers/artifacts/output must remain outside authenticated source")
    output.mkdir(parents=True, exist_ok=False)
    metadata = json.loads(args.artifact_metadata.read_text())
    validate_artifact(metadata)
    clean_source(root)
    observer_path = helper.with_name("observe-windows-alert-state.py")
    if sha(observer_path) != EXPECTED_OBSERVER:
        raise RuntimeError("Reviewed read-only state observer differs")
    observer = load("readonly_windows_state", observer_path)
    graph_file = helper.with_name("windows-portable-scheduler-inputs.json")
    test, fixture_records = stage_fixture(root, diagnostic_root, graph_file)
    original = root / NOTIFICATION_TEST
    if sha(original) != EXPECTED_NOTIFICATION:
        raise RuntimeError("Original unchanged notification test differs")
    ci = load("original_candidate_ci", root / "tools/chrome-extension-runtime/ci/test-candidate.py")
    manifests = list(artifact.rglob("candidate-manifest.json"))
    if len(manifests) != 1:
        raise RuntimeError("Expected one original candidate manifest")
    manifest_path = manifests[0]
    manifest = json.loads(manifest_path.read_text())
    if (manifest.get("floorpCommit") != EXPECTED_HEAD or manifest.get("buildId") != EXPECTED_BUILD or
            manifest.get("sourceCommit") != EXPECTED_SOURCE or manifest.get("platform") != "windows" or
            manifest.get("target") != "x86_64-pc-windows-msvc" or manifest.get("productionOverlay") is not True or
            manifest.get("nativeWebExtensionBindings") is not True):
        raise RuntimeError("Original native Windows artifact identity differs")
    ci._identity.validate_maintained_patches(manifest, root)
    package_row = ci._selection.select_native_package(manifest["packages"], "windows")
    if package_row != EXPECTED_PACKAGE:
        raise RuntimeError("Not the pinned old Windows package")
    packages = list(artifact.rglob(package_row["name"]))
    if (len(packages) != 1 or packages[0].is_symlink() or
            packages[0].stat().st_size != package_row["bytes"] or sha(packages[0]) != package_row["sha256"]):
        raise RuntimeError("Original native package size/checksum differs")
    application = output / "application"
    application.mkdir()
    with zipfile.ZipFile(packages[0]) as archive:
        for item in safe_zip_members(archive):
            archive.extract(item, application)
    binary = ci.binary_in(application, "windows")
    candidate = ci._identity.authenticate_candidate(manifest_path, binary, root)
    if (candidate["binarySha256"] != EXPECTED_BINARY or candidate["verifiedFileCount"] != 9283 or
            candidate["applicationInventorySha256"] != EXPECTED_INVENTORY):
        raise RuntimeError("Full original Windows application payload differs")
    summary = {"scope": "separate old Windows product diagnostic; zero qualification credit",
               "diagnosticCompleted": False, "candidate": candidate, "artifactMetadata": metadata,
               "originalSource": clean_source(root), "fixtureRecords": fixture_records,
               "producerFiles": [file_record(path) for path in (helper, observer_path, graph_file)],
               "originalNotificationTest": file_record(original), "phases": {}}
    save(output / "summary.json", summary)
    environment = dict(os.environ)
    for name in ("MOZ_HEADLESS", "MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_DISABLE_GPU_SANDBOX",
                 "MOZ_DISABLE_RDD_SANDBOX", "MOZ_DISABLE_GMP_SANDBOX", "FLOORP_TEST_RUNTIME_PATCH_OVERRIDE",
                 "FLOORP_TEST_ACTOR_DIRECTORY", "FLOORP_TEST_CANDIDATE_MANIFEST", "FLOORP_CANDIDATE_MANIFEST",
                 "FLOORP_TEST_NATIVE_CANDIDATE_IDENTITY", "FLOORP_TEST_REQUIRE_NATIVE_CORPUS"):
        environment.pop(name, None)

    def verify_sources():
        clean_source(root)
        for row in fixture_records:
            for record in (row["original"], row["selected"], row["copied"]):
                path = Path(record["path"])
                if sha(path) != record["sha256"] or path.stat().st_size != record["bytes"]:
                    raise RuntimeError("Physical fixture source changed")
        for record in summary["producerFiles"]:
            if sha(Path(record["path"])) != record["sha256"]:
                raise RuntimeError("Diagnostic producer changed")

    def phase(name, suite, headed, notification=False):
        verify_sources()
        if notification:
            save(output / f"{name}-state-before.json", observer.observe())
        reports_at = root / "_dist/chrome-runtime-tests/windows-old-diagnostic" / name
        command = [sys.executable, root / "tools/chrome-extension-runtime/run-browser-tests.py",
                   "--browser", binary, "--candidate-manifest", manifest_path,
                   "--output", reports_at, "--timeout", "300", "--test", suite]
        if headed:
            command.append("--headed")
        process = bounded_windows(command, root, environment, output / name)
        if notification:
            save(output / f"{name}-state-after.json", observer.observe())
        reports = list(reports_at.glob("run-*/report.json"))
        report = json.loads(reports[0].read_text()) if len(reports) == 1 else {}
        if report.get("runtime"):
            ci._identity.verify_runtime_identity(candidate, report["runtime"])
        if report.get("passed") and report.get("nativeCandidateIdentityMatches") is not True:
            raise RuntimeError("A passing phase must include actual native runtime/package binding")
        for module in report.get("modules", []):
            for record in module["bundleInputs"]:
                if not Path(record["path"]).resolve(strict=True).is_relative_to(root):
                    raise RuntimeError("Executed test input escapes candidate checkout")
        verify_sources()
        after = ci._identity._tree_inventory(binary.parent)
        digest = hashlib.sha256(json.dumps(sorted(after.items()), separators=(",", ":")).encode()).hexdigest()
        if digest != EXPECTED_INVENTORY:
            raise RuntimeError("Original application changed; no next phase allowed")
        result = {"supervisor": process, "reports": [file_record(path) for path in reports],
                  "runtime": report.get("runtime"), "nativeCandidateIdentityMatches": report.get("nativeCandidateIdentityMatches"),
                  "appInventoryUnchanged": True,
                  "passed": process["exitCode"] == 0 and report.get("passed") is True and not process["watchdogStopped"]}
        summary["phases"][name] = result
        save(output / "summary.json", summary)
        return result

    headless = phase("notifications-headless", original, False, notification=True)
    if not headless["passed"]:
        phase("notifications-headed", original, True, notification=True)
    else:
        summary["conditionalHeaded"] = "not run because original headless test passed"
    phase("scheduler-portable", test, False)
    summary["sourceAfter"] = clean_source(root)
    summary["diagnosticCompleted"] = True
    summary["allSelectedTestsPassed"] = all(row["passed"] for row in summary["phases"].values())
    save(output / "summary.json", summary)
    return 0 if summary["allSelectedTestsPassed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
