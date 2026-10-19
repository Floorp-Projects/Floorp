#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Observe natural Windows notification readiness, then one unchanged headed test.

This separate diagnostic never changes Windows settings, native APIs, browser
security preferences, source files or the application. No browser starts unless
a real read-only observation returns S_OK and QUNS_ACCEPTS_NOTIFICATIONS.
"""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zipfile

sys.dont_write_bytecode = True

EXPECTED_DRIVER = "2908ab5a2368c1ac38b84031be502abe0fa16dbb7bec314fd8d3d893549526c0"
EXPECTED_OBSERVER = "aa4da4e360459d540fe3516c7e56b64e34d43c19626a4d4b9ee5049aef055997"
EXPECTED_HEAD = "b781ab6b56fb6e41bd87eeba91221230daebb579"
EXPECTED_PR_HEAD = "3bd78b645242c1fd45cd634109b1f1cb52d3e6d7"
EXPECTED_RUN = 39324371771
EXPECTED_SOURCE = "7dd619742b25567e479eeda97ae071d10542ad0b"
MAX_WAIT_SECONDS = 65 * 60
POLL_SECONDS = 15


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def accepts_notifications(snapshot):
    state = snapshot.get("notificationState", {})
    return (state.get("hresult") == "0x00000000" and
            state.get("querySucceeded") is True and state.get("value") == 5)


def poll_until_accepting(observe, retain, clock=time.monotonic, sleep=time.sleep):
    """The injected clock/observer support host-only deadline regression tests."""
    started = clock()
    deadline = started + MAX_WAIT_SECONDS
    sequence = 0
    while clock() < deadline:
        poll_started = clock()
        observation = observe(sequence, min(10, deadline - clock()))
        elapsed = clock() - started
        accepted = accepts_notifications(observation) and clock() <= deadline
        row = {"sequence": sequence, "elapsedSeconds": elapsed,
               "snapshot": observation, "acceptingAtObservation": accepted}
        retain(row)
        if accepted:
            return {"ready": True, "elapsedSeconds": elapsed,
                    "acceptedSequence": sequence, "observations": sequence + 1}
        sequence += 1
        # Each native subprocess is bounded; each wait is at most fifteen seconds.
        remaining = min(poll_started + POLL_SECONDS, deadline) - clock()
        if remaining > 0:
            sleep(remaining)
    return {"ready": False, "elapsedSeconds": clock() - started,
            "observations": sequence, "reason": "OS-notification-readiness-deadline"}


def clean_source(root):
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    status = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root, text=True)
    if head != EXPECTED_HEAD or status:
        raise RuntimeError("Expected the exact clean current CI compiled merge")
    return {"head": head, "trackedStatus": status}


def authenticate_current(old, root, artifact, application, output):
    # This function is called only after an actual S_OK/state5 observation.
    metadata_bytes = subprocess.check_output(
        ["gh", "api", f"repos/Floorp-Projects/Floorp/actions/runs/{EXPECTED_RUN}/artifacts"], timeout=30)
    metadata = json.loads(metadata_bytes)
    choices = [row for row in metadata["artifacts"] if row.get("name") == "chrome-native-windows"]
    if len(choices) != 1:
        raise RuntimeError("Current run has no unique completed Windows artifact; no browser attempt")
    row = choices[0]
    workflow = row.get("workflow_run", {})
    if (row.get("expired") is not False or workflow.get("id") != EXPECTED_RUN or
            workflow.get("head_sha") != EXPECTED_PR_HEAD or
            workflow.get("head_branch") != "codex/chrome-extension-runtime-v2" or
            workflow.get("repository_id") != 437505831 or workflow.get("head_repository_id") != 437505831 or
            not isinstance(row.get("size_in_bytes"), int) or not 0 < row["size_in_bytes"] <= 2 * 1024 ** 3 or
            not isinstance(row.get("digest"), str) or not row["digest"].startswith("sha256:")):
        raise RuntimeError("Current Windows artifact run/repository/branch/digest identity differs")
    save(output / "artifact-metadata.json", row)
    artifact.mkdir(parents=True, exist_ok=False)
    raw_zip = artifact / "github-artifact.zip"
    started = time.monotonic()
    with raw_zip.open("xb") as stream, (output / "artifact-download-stderr.log").open("wb") as error:
        subprocess.run(["gh", "api", f"repos/Floorp-Projects/Floorp/actions/artifacts/{row['id']}/zip"],
                       stdout=stream, stderr=error, check=True, timeout=300)
    if raw_zip.stat().st_size != row["size_in_bytes"] or sha(raw_zip) != row["digest"].removeprefix("sha256:"):
        raise RuntimeError("Exact current raw GitHub artifact size/digest differs")
    save(output / "artifact-download-receipt.json", {
        "artifactId": row["id"], "runId": EXPECTED_RUN, "bytes": raw_zip.stat().st_size,
        "sha256": sha(raw_zip), "elapsedSeconds": time.monotonic() - started})
    payload = artifact / "payload"
    payload.mkdir()
    with zipfile.ZipFile(raw_zip) as archive:
        for member in old.safe_zip_members(archive):
            archive.extract(member, payload)
    manifests = list(payload.rglob("candidate-manifest.json"))
    if len(manifests) != 1:
        raise RuntimeError("Expected exactly one current candidate manifest")
    manifest_path = manifests[0]
    manifest = json.loads(manifest_path.read_text())
    if (manifest.get("schemaVersion") != 1 or manifest.get("floorpCommit") != EXPECTED_HEAD or
            manifest.get("sourceCommit") != EXPECTED_SOURCE or manifest.get("platform") != "windows" or
            manifest.get("target") != "x86_64-pc-windows-msvc" or manifest.get("productionOverlay") is not True or
            manifest.get("nativeWebExtensionBindings") is not True):
        raise RuntimeError("Current compiled-merge Windows production identity differs")
    shutil.copyfile(manifest_path, output / "candidate-manifest.json")
    ci = old.load("current_candidate_ci", root / "tools/chrome-extension-runtime/ci/test-candidate.py")
    ci._identity.validate_maintained_patches(manifest, root)
    package = ci._selection.select_native_package(manifest["packages"], "windows")
    if Path(package["name"]).name != package["name"]:
        raise RuntimeError("Selected Windows package name is not a single filename")
    packages = list(payload.rglob(package["name"]))
    if (len(packages) != 1 or packages[0].is_symlink() or
            packages[0].stat().st_size != package["bytes"] or sha(packages[0]) != package["sha256"]):
        raise RuntimeError("Current package checksum/size differs")
    application.mkdir(parents=True, exist_ok=False)
    with zipfile.ZipFile(packages[0]) as archive:
        for member in old.safe_zip_members(archive):
            archive.extract(member, application)
    binary = ci.binary_in(application, "windows")
    candidate = ci._identity.authenticate_candidate(manifest_path, binary, root)
    clean_source(root)
    return ci, candidate, manifest_path, binary, row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "artifact", "application", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Actual Windows is required; no simulated native success")
    helper = Path(__file__).resolve(strict=True)
    driver_path = helper.with_name("run-windows-old-diagnostic.py")
    observer_path = helper.with_name("observe-windows-alert-state.py")
    if sha(driver_path) != EXPECTED_DRIVER or sha(observer_path) != EXPECTED_OBSERVER:
        raise RuntimeError("Reviewed immutable diagnostic helpers differ")
    old = load("reviewed_old_windows_diagnostic", driver_path)
    root = args.source_root.resolve(strict=True)
    artifact = args.artifact.resolve()
    application = args.application.resolve()
    output = args.output.resolve()
    selected = [helper, driver_path, observer_path, artifact, output, application]
    if any(path.is_relative_to(root) for path in selected):
        raise ValueError("Diagnostic helpers/artifacts/output/application must be outside authenticated source")
    if application == output or application.is_relative_to(output) or output.is_relative_to(application):
        raise ValueError("Application and evidence output must be disjoint")
    output.mkdir(parents=True, exist_ok=False)
    clean_source(root)
    original = root / old.NOTIFICATION_TEST
    if sha(original) != old.EXPECTED_NOTIFICATION:
        raise RuntimeError("Original notification test differs")
    producers = [old.file_record(path) for path in (helper, driver_path, observer_path)]
    for path in (helper, driver_path, observer_path):
        destination = output / "producer-source" / path.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        if sha(destination) != sha(path):
            raise RuntimeError("Retained diagnostic source differs")
    summary = {"scope": "natural-expiry current393243 Windows product diagnostic; zero qualification credit",
               "diagnosticCompleted": False, "browserAttempted": False, "expectedCompiledMerge": EXPECTED_HEAD,
               "expectedRun": EXPECTED_RUN, "originalSource": clean_source(root),
               "producerFiles": producers, "originalNotificationTest": old.file_record(original),
               "maximumReadinessWaitSeconds": MAX_WAIT_SECONDS, "pollIntervalSeconds": POLL_SECONDS,
               "settingsChanged": False, "historicalQuietRedRun": 39323813903,
               "historicalQuietRedArtifact": {"id": 12220874227,
                   "sha256": "14a8719d218b51c82f3007265bca5aa58e39fb0afe09f2c90021b25b4251a348"}}
    save(output / "summary.json", summary)
    environment = dict(os.environ)
    for name in ("MOZ_HEADLESS", "MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_DISABLE_GPU_SANDBOX",
                 "MOZ_DISABLE_RDD_SANDBOX", "MOZ_DISABLE_GMP_SANDBOX", "FLOORP_TEST_RUNTIME_PATCH_OVERRIDE",
                 "FLOORP_TEST_ACTOR_DIRECTORY", "FLOORP_TEST_CANDIDATE_MANIFEST", "FLOORP_CANDIDATE_MANIFEST",
                 "FLOORP_TEST_NATIVE_CANDIDATE_IDENTITY", "FLOORP_TEST_REQUIRE_NATIVE_CORPUS",
                 "GH_TOKEN", "GITHUB_TOKEN"):
        environment.pop(name, None)

    def verify_sources():
        clean_source(root)
        for record in producers + [summary["originalNotificationTest"]]:
            path = Path(record["path"])
            if sha(path) != record["sha256"] or path.stat().st_size != record["bytes"]:
                raise RuntimeError("Diagnostic or original source changed")

    def observe(sequence, remaining):
        verify_sources()
        directory = output / "readiness" / f"poll-{sequence:04d}"
        snapshot_path = directory / "snapshot.json"
        result = old.bounded_windows(
            [sys.executable, observer_path, "--output", snapshot_path], root,
            environment, directory, seconds=remaining)
        if (result["exitCode"] != 0 or result["watchdogStopped"] or
                result["remainingOwned"] != 0 or not result["ownJobAssignedBeforeWork"]):
            raise RuntimeError("Read-only native observer failed; no browser is allowed")
        return json.loads(snapshot_path.read_text())

    def retain(row):
        save(output / "readiness" / f"observation-{row['sequence']:04d}.json", row)
        summary["lastReadinessObservation"] = row
        save(output / "summary.json", summary)

    readiness = poll_until_accepting(observe, retain)
    summary["readiness"] = readiness
    verify_sources()
    if not readiness["ready"]:
        summary.update({"diagnosticCompleted": True, "osBlocked": True, "passed": False,
                        "sourceAfter": clean_source(root)})
        save(output / "summary.json", summary)
        return 2
    ci, candidate, manifest_path, binary, metadata = authenticate_current(
        old, root, artifact, application, output)
    summary["candidate"] = candidate
    summary["artifactMetadata"] = metadata
    verify_sources()
    pre_test = observe(readiness["acceptedSequence"] + 1, 10)
    save(output / "pre-test-state.json", pre_test)
    if not accepts_notifications(pre_test):
        summary.update({"diagnosticCompleted": True, "osBlocked": True, "passed": False,
                        "reason": "OS-readiness-changed-before-browser-attempt"})
        save(output / "summary.json", summary)
        return 2
    reports_at = root / "_dist/chrome-runtime-tests/windows-quiet-expiry"
    summary["browserAttempted"] = True
    save(output / "summary.json", summary)
    command = [sys.executable, root / "tools/chrome-extension-runtime/run-browser-tests.py",
               "--browser", binary, "--candidate-manifest", manifest_path,
               "--output", reports_at, "--timeout", "300", "--test", original, "--headed"]
    process = old.bounded_windows(command, root, environment, output / "notifications-headed")
    # Keep a real post-test native observation with the same bounded ownership.
    observe_directory = output / "post-test-state"
    observer_process = old.bounded_windows(
        [sys.executable, observer_path, "--output", observe_directory / "snapshot.json"],
        root, environment, observe_directory, seconds=10)
    summary["postTestObserver"] = observer_process
    reports = list(reports_at.glob("run-*/report.json"))
    report = json.loads(reports[0].read_text()) if len(reports) == 1 else {}
    if report.get("runtime"):
        ci._identity.verify_runtime_identity(candidate, report["runtime"])
        if report["runtime"].get("workerPolicy") is not False:
            raise RuntimeError("Global experimental Worker policy must remain disabled")
    copies = []
    for module in report.get("modules", []):
        for record in module["bundleInputs"]:
            source = Path(record["path"]).resolve(strict=True)
            if (not source.is_relative_to(root) or sha(source) != record["sha256"] or
                    source.stat().st_size != record["bytes"]):
                raise RuntimeError("Executed original input differs or escapes authenticated source")
            target = output / "test-input-source" / source.relative_to(root)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            if sha(target) != record["sha256"]:
                raise RuntimeError("Retained actual input differs")
            copies.append({"original": record, "retained": old.file_record(target)})
    verify_sources()
    after = ci._identity._tree_inventory(binary.parent)
    digest = hashlib.sha256(json.dumps(sorted(after.items()), separators=(",", ":")).encode()).hexdigest()
    if digest != candidate["applicationInventorySha256"]:
        raise RuntimeError("Current application changed")
    passed = (process["exitCode"] == 0 and not process["watchdogStopped"] and
              process["remainingOwned"] == 0 and report.get("passed") is True and
              report.get("nativeCandidateIdentityMatches") is True and
              observer_process["exitCode"] == 0 and observer_process["remainingOwned"] == 0)
    summary.update({"diagnosticCompleted": True, "osBlocked": False, "passed": passed,
                    "supervisor": process, "runtime": report.get("runtime"),
                    "nativeCandidateIdentityMatches": report.get("nativeCandidateIdentityMatches"),
                    "reports": [old.file_record(path) for path in reports],
                    "testInputCopies": copies, "appInventoryUnchanged": True,
                    "sourceAfter": clean_source(root)})
    save(output / "summary.json", summary)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
