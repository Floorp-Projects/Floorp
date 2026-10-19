#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""One late-only Windows alert diagnostic and the exact UTF-8 UI fixture fix.

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
}
NOTIFICATION = "browser-features/modules/modules/chrome-web-store/test/ChromeNotificationsCloseNative.test.mts"
NOTIFICATION_SHA = "6a86399fba5653672f6d2bdd99adbc1cbebb5a7d5bbcaa9b0871a5eed3f06936"
UI = "tools/chrome-extension-runtime/test-diagnostics-ui.py"
UI_SHA = "b91fc0cd18765bce924fde1faeb756b78edcc4d2a6c04bded23e83155775116b"
UI_FIX_OLD = '(ROOT / "i18n/ja-JP/browser-chrome.json").read_text()'
UI_FIX_NEW = '(ROOT / "i18n/ja-JP/browser-chrome.json").read_text(encoding="utf-8")'
WAIT_END = '      "real active native alert before clear",\n    );'
CATCH_END = '''      "real active native alert before clear",
    ).catch(async (originalError: unknown) => {
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
    added_import = 'import { captureLateAlertFailure } from "./windows-alert-late-snapshot.sys.mts";\n'
    result = added_import + relocated.replace(WAIT_END, CATCH_END)
    recovered = result.removeprefix(added_import).replace(CATCH_END, WAIT_END)
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
    original, original_ui = root / NOTIFICATION, root / UI
    if sha(original) != NOTIFICATION_SHA or sha(original_ui) != UI_SHA:
        raise RuntimeError("Original candidate test or UI source differs")
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
    readiness_summary = {"scope": "late-only current Windows diagnostic; zero qualification credit",
                         "browserAttempted": False, "maximumReadinessWaitSeconds": quiet.MAX_WAIT_SECONDS,
                         "pollIntervalSeconds": quiet.POLL_SECONDS, "settingsChanged": False}
    old.save(output / "summary.json", readiness_summary)

    def observe(sequence, remaining):
        quiet.clean_source(root)
        for name, digest in HELPERS.items():
            if sha(here / name) != digest:
                raise RuntimeError("Reviewed helper changed during natural-readiness wait")
        directory = output / "readiness" / f"poll-{sequence:04d}"
        snapshot_path = directory / "snapshot.json"
        result = old.bounded_windows(
            [sys.executable, here / "observe-windows-alert-state.py", "--output", snapshot_path],
            root, environment, directory, seconds=remaining)
        if (result["exitCode"] != 0 or result["watchdogStopped"] or
                result["remainingOwned"] != 0 or not result["ownJobAssignedBeforeWork"]):
            raise RuntimeError("Read-only native observer failed; no browser is allowed")
        return json.loads(snapshot_path.read_text())

    def retain(row):
        old.save(output / "readiness" / f"observation-{row['sequence']:04d}.json", row)
        readiness_summary["lastReadinessObservation"] = row
        old.save(output / "summary.json", readiness_summary)

    # Reuse the exact reviewed 65-minute / 15-second native readiness policy.
    readiness = quiet.poll_until_accepting(observe, retain)
    readiness_summary["readiness"] = readiness
    old.save(output / "summary.json", readiness_summary)
    if not readiness["ready"]:
        return 2
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
    staged = root / "_dist/chrome-runtime-tests/windows-alert-late/source"
    test = stage_test(original, here / "windows-alert-late-snapshot.sys.mts", staged)
    ui_source = original_ui.read_text(encoding="utf-8")
    if ui_source.count(UI_FIX_OLD) != 1:
        raise RuntimeError("Exact UI UTF8 fix point differs")
    ui_fixed = ui_source.replace(UI_FIX_OLD, UI_FIX_NEW)
    if ui_fixed.replace(UI_FIX_NEW, UI_FIX_OLD).encode() != original_ui.read_bytes():
        raise RuntimeError("UI whole source did not recover exactly")
    ui = staged / "test-diagnostics-ui-utf8.py"
    ui.write_text(ui_fixed, encoding="utf-8", newline="\n")
    summary = {"scope": "late-only alert diagnostic and one UTF8 fixture fix; zero qualification credit",
               "candidate": candidate, "artifact": actual_artifact,
               "producerFiles": producers, "originalNotification": old.file_record(original),
               "originalUI": old.file_record(original_ui), "stagedTest": old.file_record(test),
               "stagedUI": old.file_record(ui), "settingsChanged": False,
               "diagnosticCompleted": False, "originalWaitSeconds": 20, "originalPollMilliseconds": 25,
               "readiness": readiness, "maximumReadinessWaitSeconds": quiet.MAX_WAIT_SECONDS,
               "pollIntervalSeconds": quiet.POLL_SECONDS, "browserAttempted": False}
    old.save(output / "summary.json", summary)
    reports_at = root / "_dist/chrome-runtime-tests/windows-alert-late/browser"
    snapshot_path = staged.parent / "late-snapshot.json"
    if snapshot_path.exists():
        raise RuntimeError("Diagnostic snapshot destination already exists")
    environment["FLOORP_TEST_ALERT_LATE_SNAPSHOT_OUTPUT"] = str(snapshot_path)
    pre_test = observe(readiness["acceptedSequence"] + 1, 10)
    old.save(output / "pre-test-state.json", pre_test)
    if not quiet.accepts_notifications(pre_test):
        summary.update({"diagnosticCompleted": True,
                        "reason": "OS-readiness-changed-before-browser-attempt"})
        old.save(output / "summary.json", summary)
        return 2
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
    # The copied UI has exactly the canonical UTF8 hunk. ROOT is rebound by the
    # launcher to the authenticated candidate; no tracked file is overwritten.
    launcher = ("import importlib.util,sys; from pathlib import Path; "
                "s=importlib.util.spec_from_file_location('utf8_ui',sys.argv[1]); "
                "m=importlib.util.module_from_spec(s);s.loader.exec_module(m); "
                "m.ROOT=Path(sys.argv[2]);sys.argv=[sys.argv[1],'--browser',sys.argv[3]];sys.exit(m.main())")
    ui_reports_before = set((root / "_dist").glob("diagnostics-ui-*/report.json"))
    environment.pop("FLOORP_TEST_ALERT_LATE_SNAPSHOT_OUTPUT", None)
    summary["diagnosticsUI"] = old.bounded_windows(
        [sys.executable, "-c", launcher, ui, root, binary], root, environment, output / "diagnostics-ui")
    ui_reports = set((root / "_dist").glob("diagnostics-ui-*/report.json")) - ui_reports_before
    summary["diagnosticsUIReports"] = [old.file_record(path) for path in sorted(ui_reports)]
    if len(ui_reports) == 1:
        summary["diagnosticsUIReport"] = json.loads(next(iter(ui_reports)).read_text())
    after = ci._identity._tree_inventory(binary.parent)
    digest = hashlib.sha256(json.dumps(sorted(after.items()), separators=(",", ":")).encode()).hexdigest()
    if digest != candidate["applicationInventorySha256"]:
        raise RuntimeError("Authenticated application changed")
    if sha(original) != NOTIFICATION_SHA or sha(original_ui) != UI_SHA:
        raise RuntimeError("Tracked original fixtures changed")
    summary.update({"diagnosticCompleted": True, "applicationInventoryUnchanged": True,
                    "sourceAfter": quiet.clean_source(root)})
    old.save(output / "summary.json", summary)
    # An emitted PASS report cannot override a later runner/ownership failure.
    def lifecycle_passed(result):
        return (result["exitCode"] == 0 and result["watchdogStopped"] is False and
                result["ownJobAssignedBeforeWork"] is True and result["remainingOwned"] == 0)
    summary["notificationLifecyclePassed"] = lifecycle_passed(summary["notifications"])
    summary["uiLifecyclePassed"] = lifecycle_passed(summary["diagnosticsUI"])
    passed = (report.get("passed") is True and summary["notificationLifecyclePassed"] and
              summary["uiLifecyclePassed"] and summary.get("diagnosticsUIReport", {}).get("passed") is True)
    summary["passed"] = passed
    old.save(output / "summary.json", summary)
    # Original notification failure remains a failure; the UI outcome is separate.
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
