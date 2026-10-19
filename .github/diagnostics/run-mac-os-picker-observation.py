#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Observe original screen request OS picker externally after exact reviewed authentication.

Uses the exact current b781 package with normal sandbox and OS permissions.
All new inputs are confined to ignored files; this is not qualification.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
EXPECTED_HEAD = "b781ab6b56fb6e41bd87eeba91221230daebb579"
EXPECTED_BUILD = "20261019210538"
EXPECTED_ARTIFACT = {"id": 12224034260, "size_in_bytes": 128821564,
                     "digest": "sha256:7394a1f4d633f1c6a3e1fda43768647ae8aba0d5d20657eef080d9b82331fd35"}
EXPECTED_PACKAGE = {"name": "floorp-158.0.1.en-US.mac.dmg", "bytes": 128616642,
                    "sha256": "1990d459ba1c1ed09d28cbc87f916824cd2f26b3d9c0d628202ae0476909345d"}
EXPECTED_BINARY = "c1bca39bfbde25c96c6e375ae9d5ec06a789498ee8f58170eb7a8aa4dc4d77f4"
EXPECTED_INVENTORY = "610c4c54614a2f0674604d863128866e98beb9a3876426af8c9895687dac42b6"
EXISTING_HELPERS = {
    "run-mac-startup-diagnostic.py": "3f884e83e08a765068346ac6ef124882d0079e5d827cd53faf8c5d8a61068239",
    "run-windows-old-diagnostic.py": "2908ab5a2368c1ac38b84031be502abe0fa16dbb7bec314fd8d3d893549526c0",
}
PREFIX = "browser-features/modules/modules/chrome-web-store/test/"
CORRECTED = {'browser-features/modules/modules/chrome-web-store/test/ChromeDebuggerNative.test.mts': '1f501413eb705f942bf81c4ee1e9d9f12e3785b069cc429f5c43c9bc3f17154b', 'browser-features/modules/modules/chrome-web-store/test/auxiliary/AuxiliaryNative.test.mts': 'aa38b66ade94a9084ae9b09bfbf4ad9e7fd373670cbbe31866a0d93d7af02076', 'browser-features/modules/modules/chrome-web-store/test/auxiliary/auxiliary-font-fixture.mts': '8c4a973dc316cfd8ad61327eb9fd22b7945625edb46b00441bfa593c61a6cbdf', 'browser-features/modules/modules/chrome-web-store/test/helpers/auxiliary-font-test-types.ts': '957b7a380ecaf4214dc5430c6757efd3975816a34819a8d06ca57efa1ef09cba'}
UNCHANGED = {'browser-features/chrome/test/utils/test_harness.ts': '91a279ad59ccbc50d7df97eca08035314d1fb768f5fa92a12ae48f8f86ed3a2c', 'browser-features/modules/modules/chrome-web-store/test/helpers/debugger-auxiliary-types.ts': '5ae63d7361b85440157a2f60c6d69f33994a12e26cf5dc82489da3773fede60d', 'browser-features/modules/modules/chrome-web-store/test/helpers/auxiliary-native-test-types.ts': '6a92033b394f09e961c24bf5b2e64f59c5b96a4fdb5b508e56962eea2f52ac74', 'browser-features/modules/modules/chrome-web-store/test/helpers/capture-native-test-types.ts': '62333966003172257ad372ebcf0464e23f0462ffbf4a91b2fe57673e9234a1e1', 'browser-features/modules/modules/chrome-web-store/test/auxiliary/auxiliary-defaults.mts': '25b82db9c655b185e4af1e7b83633af3ef311399b4f1e63954bce7ebc4953cb4', 'browser-features/modules/modules/chrome-web-store/test/polyfills/capture/DesktopScreenNative.test.mts': 'a23945710b78f704ffe47edd4a31c58c43742417b79fa8dff5586f9273b925c8'}
DESKTOP_END = "  } finally {\n    for (const dialog of ownedDialogs)"
DESKTOP_CATCH = '''  } catch (originalError) {
    try {
      await captureMacDesktopFailure(
        mode, id, target.linkedBrowser, window, report, originalError,
      );
    } catch (snapshotError) {
      console.error("FLOORP_MAC_DESKTOP_LATE_SNAPSHOT_ERROR", String(snapshotError));
    }
    throw originalError;
  } finally {
    for (const dialog of ownedDialogs)'''


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def record(path):
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": sha(path)}


def clean(root):
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    status = subprocess.check_output(["git", "status", "--porcelain", "--untracked-files=no"], cwd=root, text=True)
    if head != EXPECTED_HEAD or status:
        raise RuntimeError("Expected clean exact b781 production source")
    return {"head": head, "trackedStatus": status}


def stage_graph(root, branch, destination):
    destination.mkdir(parents=True, exist_ok=False)
    sources = []
    for relative, digest in {**UNCHANGED, **CORRECTED}.items():
        source = (branch if relative in CORRECTED else root) / relative
        if sha(source) != digest:
            raise RuntimeError("Reviewed graph input differs: " + relative)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        sources.append({"original": record(source), "staged": record(target)})
    desktop = destination / PREFIX / "polyfills/capture/DesktopScreenNative.test.mts"
    original = desktop.read_text(encoding="utf-8")
    if original.count(DESKTOP_END) != 1:
        raise RuntimeError("Original desktop catch point differs")
    added = 'import { captureMacDesktopFailure } from "./mac-desktop-late-snapshot.sys.mts";\n'
    diagnostic = added + original.replace(DESKTOP_END, DESKTOP_CATCH)
    if diagnostic.removeprefix(added).replace(DESKTOP_CATCH, DESKTOP_END).encode() != desktop.read_bytes():
        raise RuntimeError("Whole original desktop source failed exact inverse")
    desktop.write_text(diagnostic, encoding="utf-8", newline="\n")
    for row in sources:
        if row["staged"]["path"] == str(desktop):
            row["staged"] = record(desktop)
            row["transformation"] = "failure-only catch; entire original bytes recovered by inverse"
    helper = branch / ".github/diagnostics/mac-desktop-late-snapshot.sys.mts"
    if sha(helper) != "77f0137f7343a610590aab96d83cd79e243b9db77590b55e761294574d95e912":
        raise RuntimeError("Reviewed desktop helper differs")
    shutil.copyfile(helper, desktop.parent / helper.name)
    sources.append({"original": record(helper), "staged": record(desktop.parent / helper.name)})
    return sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "artifact", "application", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    if sys.platform != "darwin":
        parser.error("Genuine macOS is required")
    here = Path(__file__).resolve().parent
    branch = here.parents[1]
    root = args.source_root.resolve(strict=True)
    artifact, application, output = [getattr(args, name).resolve() for name in ("artifact", "application", "output")]
    if any(path.is_relative_to(root) for path in (branch, artifact, application, output)):
        raise RuntimeError("Diagnostic helpers and allocations must remain outside authenticated source")
    if any(a == b or a.is_relative_to(b) or b.is_relative_to(a)
           for i, a in enumerate((artifact, application, output)) for b in (artifact, application, output)[i + 1:]):
        raise RuntimeError("Artifact, application and output roots must be disjoint")
    for name, digest in EXISTING_HELPERS.items():
        if sha(here / name) != digest:
            raise RuntimeError("Existing reviewed helper differs")
    owned = load("reviewed_mac_ownership", here / "run-mac-startup-diagnostic.py")
    safe = load("reviewed_outer_zip", here / "run-windows-old-diagnostic.py")
    clean(root)
    output.mkdir(parents=True, exist_ok=False)
    metadata = json.loads(subprocess.check_output(
        ["gh", "api", "repos/Floorp-Projects/Floorp/actions/runs/39324371771/artifacts"], timeout=30))
    choices = [row for row in metadata["artifacts"] if row.get("name") == "chrome-native-mac"]
    if len(choices) != 1 or any(choices[0].get(k) != v for k, v in EXPECTED_ARTIFACT.items()):
        raise RuntimeError("Exact current Mac artifact differs")
    row = choices[0]
    workflow = row.get("workflow_run", {})
    if (row.get("expired") is not False or workflow.get("id") != 39324371771 or
            workflow.get("head_sha") != "3bd78b645242c1fd45cd634109b1f1cb52d3e6d7" or
            workflow.get("repository_id") != 437505831 or workflow.get("head_repository_id") != 437505831):
        raise RuntimeError("Current Mac artifact provenance differs")
    save(output / "artifact-metadata.json", row)
    artifact.mkdir(parents=True, exist_ok=False)
    archive_path = artifact / "github-artifact.zip"
    with archive_path.open("xb") as stream:
        result = subprocess.run(["gh", "api", "repos/Floorp-Projects/Floorp/actions/artifacts/12224034260/zip"],
                                stdout=stream, stderr=subprocess.PIPE, timeout=300)
    # Never retain or print a signed redirect URL.
    error = re.sub(rb"https?://[^\s]+", b"[redacted-download-url]", result.stderr)
    (output / "artifact-download-stderr.log").write_bytes(error)
    if result.returncode or archive_path.stat().st_size != EXPECTED_ARTIFACT["size_in_bytes"] or sha(archive_path) != EXPECTED_ARTIFACT["digest"].removeprefix("sha256:"):
        raise RuntimeError("Exact current Mac archive failed download/digest check")
    payload = artifact / "payload"
    payload.mkdir()
    with zipfile.ZipFile(archive_path) as archive:
        for member in safe.safe_zip_members(archive):
            archive.extract(member, payload)
    manifests = list(payload.rglob("candidate-manifest.json"))
    packages = list(payload.rglob(EXPECTED_PACKAGE["name"]))
    if len(manifests) != 1 or len(packages) != 1:
        raise RuntimeError("Expected unique current manifest and DMG")
    manifest_path, package = manifests[0], packages[0]
    manifest = json.loads(manifest_path.read_text())
    if (manifest.get("floorpCommit") != EXPECTED_HEAD or manifest.get("buildId") != EXPECTED_BUILD or
            manifest.get("platform") != "mac" or manifest.get("target") != "aarch64-apple-darwin" or
            package.is_symlink() or package.stat().st_size != EXPECTED_PACKAGE["bytes"] or
            sha(package) != EXPECTED_PACKAGE["sha256"]):
        raise RuntimeError("Current Mac manifest/package differs")
    ci = load("current_mac_candidate", root / "tools/chrome-extension-runtime/ci/test-candidate.py")
    ci._identity.validate_maintained_patches(manifest, root)
    if ci._selection.select_native_package(manifest["packages"], "mac") != EXPECTED_PACKAGE:
        raise RuntimeError("Authenticated Mac package selection differs")
    application.mkdir(parents=True, exist_ok=False)
    mount = application / "mounted"
    mount.mkdir()
    attached = False
    try:
        subprocess.run(["hdiutil", "attach", "-readonly", "-nobrowse", "-mountpoint", mount, package], check=True, timeout=120)
        attached = True
        apps = list(mount.glob("*.app"))
        if len(apps) != 1 or apps[0].is_symlink():
            raise RuntimeError("Expected one regular current Mac application")
        subprocess.run(["ditto", apps[0], application / apps[0].name], check=True, timeout=180)
    finally:
        detached = subprocess.run(["hdiutil", "detach", mount], timeout=60)
        if attached and detached.returncode:
            raise RuntimeError("Readonly current DMG could not be detached")
    mount.rmdir()
    binary = ci.binary_in(application, "mac")
    candidate = ci._identity.authenticate_candidate(manifest_path, binary, root)
    if candidate["binarySha256"] != EXPECTED_BINARY or candidate["applicationInventorySha256"] != EXPECTED_INVENTORY:
        raise RuntimeError("Current Mac binary/application inventory differs")
    app = next(path for path in binary.parents if path.suffix == ".app")
    base = root / "_dist/chrome-runtime-tests/mac-os-picker-observation196"
    staged = base / "source"
    staged.mkdir(parents=True, exist_ok=False)
    prefix = "browser-features/modules/modules/chrome-web-store/test/"
    needed = {relative: digest for relative, digest in UNCHANGED.items()
              if relative.endswith(("test_harness.ts", "capture-native-test-types.ts",
                                    "DesktopScreenNative.test.mts"))}
    staged_records = []
    for relative, digest in needed.items():
        source = root / relative
        if sha(source) != digest:
            raise RuntimeError("Original desktop graph differs: " + relative)
        target = staged / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        staged_records.append(record(target))
    stage_spec = json.loads((here / "mac-os-picker-stage-spec.json").read_text())
    desktop = staged / prefix / "polyfills/capture/DesktopScreenNative.test.mts"
    original = desktop.read_text()
    if hashlib.sha256(original.encode()).hexdigest() != stage_spec["originalSha256"]:
        raise RuntimeError("Exact original desktop entry differs")
    head, _ = original.split("export function runAllTests() {", 1)
    modified = head
    for change in stage_spec["replacements"]:
        if modified.count(change["before"]) != 1:
            raise RuntimeError("Exact diagnostic insertion point differs")
        modified = modified.replace(change["before"], change["after"])
    inverse = modified
    for change in reversed(stage_spec["replacements"]):
        inverse = inverse.replace(change["after"], change["before"])
    if inverse != head:
        raise RuntimeError("Original complete case function inverse failed")
    observed = modified + stage_spec["phaseHelper"] + stage_spec["replacementEntry"]
    if hashlib.sha256(observed.encode()).hexdigest() != stage_spec["previewSha256"]:
        raise RuntimeError("Reviewed copied desktop program differs")
    desktop.write_text(observed, newline="\n")
    staged_records = [record(Path(a["path"])) for a in staged_records]
    env = dict(os.environ)
    for name in ("MOZ_HEADLESS", "MOZ_DISABLE_CONTENT_SANDBOX", "MOZ_DISABLE_GPU_SANDBOX",
                 "MOZ_DISABLE_RDD_SANDBOX", "MOZ_DISABLE_GMP_SANDBOX", "FLOORP_TEST_RUNTIME_PATCH_OVERRIDE",
                 "FLOORP_TEST_ACTOR_DIRECTORY", "FLOORP_TEST_CANDIDATE_MANIFEST", "FLOORP_CANDIDATE_MANIFEST",
                 "FLOORP_TEST_NATIVE_CANDIDATE_IDENTITY", "FLOORP_TEST_REQUIRE_NATIVE_CORPUS", "GH_TOKEN", "GITHUB_TOKEN"):
        env.pop(name, None)
    def integrity():
        clean(root)
        for item in staged_records:
            path = Path(item["path"])
            if path.stat().st_size != item["bytes"] or sha(path) != item["sha256"]:
                raise RuntimeError("Copied diagnostic graph changed")
        actual = ci._identity._tree_inventory(app)
        digest = hashlib.sha256(json.dumps(sorted(actual.items()), separators=(",", ":")).encode()).hexdigest()
        if digest != EXPECTED_INVENTORY:
            raise RuntimeError("Current Mac application changed")
    summary = {"qualificationCredit": 0, "candidate": candidate,
               "scope": "original record-screen only, external read-only OS snapshots",
               "stagedRecords": staged_records, "runs": []}
    producer = output / "producer-source"
    producer.mkdir()
    for name in ("run-mac-os-picker-observation.py", "observe-mac-os-picker.py",
                 "mac-os-picker-snapshot.m", "mac-os-picker-stage-spec.json",
                 *EXISTING_HELPERS):
        shutil.copyfile(here / name, producer / name)
    def run(label, command, seconds):
        integrity()
        outcome = owned.bounded(command, root, output / label, seconds, env)
        summary["runs"].append({"label": label, "outcome": outcome})
        save(output / "summary.json", summary)
        if outcome["survivingOwnedGroup"]:
            raise RuntimeError("Owned process remains")
        integrity()
        return outcome
    native = output / "mac-os-picker-snapshot"
    compiled = run("compile-external-observer", ["xcrun", "clang", "-fobjc-arc", "-O2",
        here / "mac-os-picker-snapshot.m", "-o", native, "-framework", "AppKit",
        "-framework", "ApplicationServices", "-framework", "ScreenCaptureKit",
        "-framework", "ImageIO"], 120)
    if compiled["exitCode"] != 0 or compiled["watchdogStopped"]:
        raise RuntimeError("Real Mac external observer did not compile")
    command = [sys.executable, here / "observe-mac-os-picker.py",
        "--native-observer", native, "--output", output / "external-snapshots", "--",
        sys.executable, root / "tools/chrome-extension-runtime/run-browser-tests.py",
        "--browser", binary, "--candidate-manifest", manifest_path,
        "--output", base / "original-screen", "--timeout", "300", "--test", desktop, "--headed"]
    actual = run("original-screen-external-observation", command, 420)
    reports = list((base / "original-screen").glob("run-*/report.json"))
    if len(reports) != 1:
        raise RuntimeError("Expected one original screen diagnostic report")
    report = json.loads(reports[0].read_text())
    if report.get("runtime"):
        ci._identity.verify_runtime_identity(candidate, report["runtime"])
    if report.get("nativeCandidateIdentityMatches") is not True:
        raise RuntimeError("Actual report native identity differs")
    modules = report.get("modules", [])
    if (len(modules) != 1 or
            Path(modules[0]["path"]).resolve() != desktop.resolve() or
            modules[0].get("testSourceSha256") != stage_spec["previewSha256"]):
        raise RuntimeError("Actual single screen module binding differs")
    for item in modules[0]["bundleInputs"]:
        source = Path(item["path"]).resolve(strict=True)
        if not source.is_relative_to(root) or sha(source) != item["sha256"]:
            raise RuntimeError("Actual bundle input differs or escapes candidate")
        target = output / "executed-inputs" / source.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    summary.update({"report": record(reports[0]), "originalTestPassed": report.get("passed"),
        "diagnosticCompleted": True, "sourceAfter": clean(root),
        "applicationInventoryUnchanged": True,
        "osPickerSelectionAutomated": False,
        "observerCallsExtensionAPI": False,
        "observerAttachesWorker": False})
    integrity()
    save(output / "summary.json", summary)
    # Preserve the original screen result. An unselected OS picker may fail it.
    return 0 if report.get("passed") is True and actual["exitCode"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
