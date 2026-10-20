#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""One immediate Windows alert/window-geometry diagnostic. No natural-expiry wait.

No Windows setting, application byte or tracked candidate source is changed.
The historical original RED remains authoritative; this run earns no credit.
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

sys.dont_write_bytecode = True

EXPECTED_HEAD = "b781ab6b56fb6e41bd87eeba91221230daebb579"
EXPECTED_ARTIFACT = {"id": 12223427866, "size_in_bytes": 151951140,
                     "digest": "sha256:1c7c5300599f516b9af51dddec24e88acd3e4199304e533b990d6a9098a20c4a"}
HELPERS = {
    "run-windows-old-diagnostic.py": "2908ab5a2368c1ac38b84031be502abe0fa16dbb7bec314fd8d3d893549526c0",
    "run-windows-quiet-expiry-diagnostic.py": "90514a52398f493563efa6679d1a60b47ec7852ae5b1eb530dfd5cb90027e3a4",
    "observe-windows-alert-state.py": "aa4da4e360459d540fe3516c7e56b64e34d43c19626a4d4b9ee5049aef055997",
    "windows-alert-late-snapshot.sys.mts": "376140bf26ee50c2ba6e23a8881c51d68aa05de7790b5f7dc233e7be63756fda",
    "windows-alert-geometry-snapshot.sys.mts": "060b5d4842af91063b5d740dd41a8f5e90b49b2629b207ad46348859767b9a79",
}
NOTIFICATION = "browser-features/modules/modules/chrome-web-store/test/ChromeNotificationsCloseNative.test.mts"
NOTIFICATION_SHA = "6a86399fba5653672f6d2bdd99adbc1cbebb5a7d5bbcaa9b0871a5eed3f06936"
WAIT_END = '      "real active native alert before clear",\n    );'
CATCH_END = '''      "real active native alert before clear",
    ).catch(async (originalError: unknown) => {
      try {
        await captureAlertGeometry("after-original-failure");
      } catch (geometryError) {
        console.error("FLOORP_ALERT_GEOMETRY_ERROR", String(geometryError));
      }
      try {
        await captureLateAlertFailure(title, originalError, state());
      } catch (snapshotError) {
        console.error("FLOORP_ALERT_LATE_SNAPSHOT_ERROR", String(snapshotError));
      }
      throw originalError;
    });'''


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def stage_test(original, snapshot, destination):
    source = original.read_text(encoding="utf-8")
    if sha(original) != NOTIFICATION_SHA or source.count(WAIT_END) != 1:
        raise RuntimeError("Exact original notification source or insertion point differs")
    relocated = source.replace('"./helpers/debugger-auxiliary-types.ts"',
        '"../../../../browser-features/modules/modules/chrome-web-store/test/helpers/debugger-auxiliary-types.ts"')
    relocated = relocated.replace('"../../../../chrome/test/utils/test_harness.ts"',
        '"../../../../browser-features/chrome/test/utils/test_harness.ts"')
    added_import = ('import { captureLateAlertFailure } from "./windows-alert-late-snapshot.sys.mts";\n'
                    'import { captureAlertGeometry } from "./windows-alert-geometry-snapshot.sys.mts";\n')
    before_old = 'export async function runAllTests(): Promise<void> {\n'
    before_new = before_old + ('  try { await captureAlertGeometry("before-original"); } '
                               'catch (error) { console.error("FLOORP_ALERT_GEOMETRY_ERROR",String(error)); }\n')
    if relocated.count(before_old) != 1:
        raise RuntimeError("Exact original runAllTests point differs")
    result = added_import + relocated.replace(WAIT_END, CATCH_END).replace(before_old, before_new)
    recovered = result.removeprefix(added_import).replace(CATCH_END, WAIT_END).replace(before_new, before_old)
    recovered = recovered.replace('"../../../../browser-features/modules/modules/chrome-web-store/test/helpers/debugger-auxiliary-types.ts"',
        '"./helpers/debugger-auxiliary-types.ts"').replace(
        '"../../../../browser-features/chrome/test/utils/test_harness.ts"',
        '"../../../../chrome/test/utils/test_harness.ts"')
    if recovered.encode() != original.read_bytes():
        raise RuntimeError("Original whole source did not recover exactly")
    destination.mkdir(parents=True, exist_ok=False)
    test = destination / original.name
    test.write_text(result, encoding="utf-8", newline="\n")
    shutil.copyfile(snapshot, destination / snapshot.name)
    geometry = snapshot.parent / "windows-alert-geometry-snapshot.sys.mts"
    shutil.copyfile(geometry, destination / geometry.name)
    return test


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "artifact", "application", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("Actual Windows required")
    here = Path(__file__).resolve().parent
    for name, digest in HELPERS.items():
        if sha(here / name) != digest:
            raise RuntimeError("Reviewed helper changed: " + name)
    old = load("owned_windows_late", here / "run-windows-old-diagnostic.py")
    quiet = load("auth_current_windows_late", here / "run-windows-quiet-expiry-diagnostic.py")
    root = args.source_root.resolve(strict=True)
    output = args.output.resolve()
    application, artifact = args.application.resolve(), args.artifact.resolve()
    paths = [here, output, application, artifact]
    if any(path.is_relative_to(root) for path in paths):
        raise RuntimeError("Helpers and allocation roots must be outside authenticated source")
    if any(a == b or a.is_relative_to(b) or b.is_relative_to(a)
           for i, a in enumerate([output, application, artifact])
           for b in [output, application, artifact][i + 1:]):
        raise RuntimeError("Output, application and artifact roots must be disjoint")
    output.mkdir(parents=True, exist_ok=False)
    quiet.clean_source(root)
    original = root / NOTIFICATION
    if sha(original) != NOTIFICATION_SHA:
        raise RuntimeError("Original candidate test source differs")
    environment = dict(os.environ)
    for name in ("MOZ_HEADLESS", "MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_DISABLE_GPU_SANDBOX",
                 "MOZ_DISABLE_RDD_SANDBOX", "MOZ_DISABLE_GMP_SANDBOX", "FLOORP_TEST_RUNTIME_PATCH_OVERRIDE",
                 "FLOORP_TEST_ACTOR_DIRECTORY", "FLOORP_TEST_CANDIDATE_MANIFEST", "FLOORP_CANDIDATE_MANIFEST",
                 "FLOORP_TEST_NATIVE_CANDIDATE_IDENTITY", "FLOORP_TEST_REQUIRE_NATIVE_CORPUS",
                 "GH_TOKEN", "GITHUB_TOKEN"):
        environment.pop(name, None)
    metadata = json.loads(subprocess.check_output([
        "gh", "api", "repos/Floorp-Projects/Floorp/actions/runs/39324371771/artifacts"], timeout=30))
    choices = [row for row in metadata["artifacts"] if row.get("name") == "chrome-native-windows"]
    if len(choices) != 1 or any(choices[0].get(k) != v for k, v in EXPECTED_ARTIFACT.items()):
        raise RuntimeError("Exact current Windows artifact differs before allocation")
    # Deliberately no 65-minute wait and no accepts-notifications gate.
    # Fresh-VM external quiet/busy state is observed; actual parent state and
    # geometry are captured by the unchanged-bounds headed original test.
    def observe(sequence, remaining):
        directory = output / "external-observer" / f"observation-{sequence:04d}"
        snapshot_path = directory / "snapshot.json"
        result = old.bounded_windows(
            [sys.executable, here / "observe-windows-alert-state.py", "--output", snapshot_path],
            root, environment, directory, seconds=remaining)
        if (result["exitCode"] != 0 or result["watchdogStopped"] or
                result["remainingOwned"] != 0 or not result["ownJobAssignedBeforeWork"]):
            raise RuntimeError("Owned read-only observer failed before browser")
        return json.loads(snapshot_path.read_text())

    ci, candidate, manifest, binary, actual_artifact = quiet.authenticate_current(
        old, root, artifact, application, output)
    if any(actual_artifact.get(k) != v for k, v in EXPECTED_ARTIFACT.items()):
        raise RuntimeError("Artifact identity changed during authentication")
    producers = [old.file_record(Path(__file__))] + [old.file_record(here / name) for name in HELPERS]
    for record in producers:
        source = Path(record["path"])
        retained = output / "producer-source" / source.name
        retained.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, retained)
    staged = root / "_dist/chrome-runtime-tests/windows-alert-geometry/source"
    test = stage_test(original, here / "windows-alert-late-snapshot.sys.mts", staged)
    summary = {"scope": "immediate readonly Windows geometry diagnostic; zero qualification credit",
               "candidate": candidate, "artifact": actual_artifact,
               "producerFiles": producers, "originalNotification": old.file_record(original),
               "stagedTest": old.file_record(test), "settingsChanged": False,
               "diagnosticCompleted": False, "originalWaitSeconds": 20, "originalPollMilliseconds": 25,
               "naturalReadinessWaitSeconds": 0, "browserAttempted": False}
    old.save(output / "summary.json", summary)
    reports_at = root / "_dist/chrome-runtime-tests/windows-alert-geometry/browser"
    snapshot_path = staged.parent / "late-snapshot.json"
    if snapshot_path.exists():
        raise RuntimeError("Diagnostic snapshot destination already exists")
    environment["FLOORP_TEST_ALERT_LATE_SNAPSHOT_OUTPUT"] = str(snapshot_path)
    environment["FLOORP_ALERT_GEOMETRY_OUTPUT"] = str(staged.parent)
    pre_test = observe(0, 10)
    old.save(output / "pre-test-state.json", pre_test)
    summary["browserAttempted"] = True
    old.save(output / "summary.json", summary)
    command = [sys.executable, root / "tools/chrome-extension-runtime/run-browser-tests.py",
               "--browser", binary, "--candidate-manifest", manifest, "--output", reports_at,
               "--timeout", "300", "--test", test, "--headed"]
    summary["notifications"] = old.bounded_windows(command, root, environment, output / "notifications")
    reports = list(reports_at.glob("run-*/report.json"))
    report = json.loads(reports[0].read_text()) if len(reports) == 1 else {}
    if report.get("runtime"):
        ci._identity.verify_runtime_identity(candidate, report["runtime"])
    summary["notificationReport"] = report
    summary["lateSnapshot"] = old.file_record(snapshot_path) if snapshot_path.is_file() else None
    copies = []
    for module in report.get("modules", []):
        for record in module["bundleInputs"]:
            source = Path(record["path"]).resolve(strict=True)
            if not source.is_relative_to(root) or sha(source) != record["sha256"]:
                raise RuntimeError("Actual diagnostic bundle input differs or escapes source")
            target = output / "test-input-source" / source.relative_to(root)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            copies.append(old.file_record(target))
    summary["retainedInputs"] = copies
    old.save(output / "summary.json", summary)
    quiet.clean_source(root)
    for record in producers:
        if sha(Path(record["path"])) != record["sha256"]:
            raise RuntimeError("Diagnostic producer changed during alert test")
    summary["externalAfterTest"] = observe(1, 10)
    for phase in ("before-original", "after-original-failure"):
        path = staged.parent / f"geometry-{phase}.json"
        summary["geometry-" + phase] = old.file_record(path) if path.is_file() else None
    if not summary["geometry-before-original"]:
        raise RuntimeError("Actual before-original parent geometry was not retained")
    if not report.get("passed") and not summary["geometry-after-original-failure"]:
        raise RuntimeError("Original failure geometry was not retained")
    after = ci._identity._tree_inventory(binary.parent)
    digest = hashlib.sha256(json.dumps(sorted(after.items()), separators=(",", ":")).encode()).hexdigest()
    if digest != candidate["applicationInventorySha256"]:
        raise RuntimeError("Authenticated application changed")
    if sha(original) != NOTIFICATION_SHA:
        raise RuntimeError("Tracked original fixture changed")
    summary.update({"diagnosticCompleted": True, "applicationInventoryUnchanged": True,
                    "sourceAfter": quiet.clean_source(root)})
    old.save(output / "summary.json", summary)
    # An emitted PASS report cannot override a later runner/ownership failure.
    def lifecycle_passed(result):
        return (result["exitCode"] == 0 and result["watchdogStopped"] is False and
                result["ownJobAssignedBeforeWork"] is True and result["remainingOwned"] == 0)
    summary["notificationLifecyclePassed"] = lifecycle_passed(summary["notifications"])
    passed = report.get("passed") is True and summary["notificationLifecyclePassed"]
    summary["passed"] = passed
    old.save(output / "summary.json", summary)
    # Observing geometry cannot turn an original notification failure into PASS.
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
