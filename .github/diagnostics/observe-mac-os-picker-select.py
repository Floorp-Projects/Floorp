#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Observe one original headed screen request and optionally press the exact guarded OS button."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-observer", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--ax-observer", required=True, type=Path)
    parser.add_argument("--browser-executable", required=True, type=Path)
    parser.add_argument("--press-share-entire-screen", action="store_true")
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if sys.platform != "darwin" or not args.command or args.command[0] != "--":
        parser.error("Genuine macOS and an explicit -- browser command required")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    phases = output / "phases"
    phases.mkdir()
    env = {**os.environ, "FLOORP_TEST_MAC_OS_PICKER_PHASES": str(phases)}
    snapshots = []
    observed_phases = set()
    browser = None
    after_allow = None
    action_inspected = False
    action_result = None
    baseline_picker_absent = None
    periodic = [0.5, 1.0, 2.0, 4.0, 8.0, 15.0, 25.0]

    def snapshot(label):
        if len(snapshots) >= 12:
            raise RuntimeError("Bounded snapshot count exceeded")
        table = subprocess.check_output(
            ["/bin/ps", "-axo", "pid=,ppid=,pgid=,stat=,comm="],
            text=True, timeout=5)
        owned = []
        for line in table.splitlines():
            fields = line.strip().split(None, 4)
            if len(fields) == 5 and int(fields[2]) == os.getpgrp():
                owned.append(int(fields[0]))
        ordinal = len(snapshots)
        stem = output / f"{ordinal:02}-{label}"
        (stem.with_suffix(".processes.txt")).write_text(table)
        image = stem.with_suffix(".png")
        outcome = {"label": label, "observedAt": time.time(), "ownedPids": owned}
        try:
            native = subprocess.run(
                [args.native_observer, image, label, ",".join(map(str, owned))],
                capture_output=True, timeout=5)
            stem.with_suffix(".stdout.json").write_bytes(native.stdout)
            stem.with_suffix(".stderr.log").write_bytes(native.stderr)
            outcome["exitCode"] = native.returncode
            try:
                outcome["native"] = json.loads(native.stdout)
            except (ValueError, UnicodeError) as error:
                outcome["parseError"] = str(error)
            if image.exists():
                data = image.read_bytes()
                outcome["image"] = {"path": str(image), "bytes": len(data),
                                    "sha256": hashlib.sha256(data).hexdigest()}
                if len(data) > 8 * 1024 * 1024:
                    raise RuntimeError("Actual image exceeded8MiB; retain and stop")
        except subprocess.TimeoutExpired as error:
            outcome["snapshotError"] = str(error)
        snapshots.append(outcome)
        (output / "snapshots.json").write_text(json.dumps(snapshots, indent=2) + "\n")

        return outcome

    def maybe_press(observation):
        nonlocal action_inspected, action_result
        if action_inspected or baseline_picker_absent is not True or after_allow is None or time.monotonic() - after_allow < 1:
            return
        native = observation.get("native", {})
        controls = [window for window in native.get("windows", [])
                    if window.get("owner") == "Control Center" and
                    window.get("name") == "SharedContentPickerDisplayControls"]
        if (len(controls) != 1 or not native.get("accessibilityTrusted") or
                native.get("preflightScreenCaptureAccess") is not True):
            return
        # At most one inspection/action invocation. A timeout never retries input.
        action_inspected = True
        action_result = {"invokedAt": time.time(), "snapshotLabel": observation["label"],
                         "actionRequested": args.press_share_entire_screen}
        try:
            actual = subprocess.run([args.ax_observer,
                "--press-share-entire-screen" if args.press_share_entire_screen else "--inspect-only",
                args.browser_executable],
                capture_output=True, timeout=5)
            (output / "actual-ax.stdout.json").write_bytes(actual.stdout)
            (output / "actual-ax.stderr.log").write_bytes(actual.stderr)
            action_result["exitCode"] = actual.returncode
            try:
                action_result["native"] = json.loads(actual.stdout)
            except (ValueError, UnicodeError) as error:
                action_result["parseError"] = str(error)
        except subprocess.TimeoutExpired as error:
            action_result["error"] = str(error)
            action_result["actionOutcomeUnknownNoRetry"] = True
        (output / "actual-ax-invocation.json").write_text(json.dumps(action_result, indent=2) + "\n")

    deadline = time.monotonic() + 360
    try:
        browser = subprocess.Popen(args.command[1:], env=env)
        while browser.poll() is None:
            if time.monotonic() >= deadline:
                raise RuntimeError("Observation wrapper360s deadline")
            for phase in ("before-request", "before-allow", "after-allow", "finally"):
                marker = phases / (phase + ".request.json")
                if phase in observed_phases or not marker.exists():
                    continue
                request = json.loads(marker.read_text())
                if request.get("phase") != phase:
                    raise RuntimeError("Unexpected diagnostic phase")
                observation = snapshot(phase)
                if phase == "before-allow":
                    native = observation.get("native", {})
                    baseline_picker_absent = (native.get("preflightScreenCaptureAccess") is True and
                        "windows" in native and not any(window.get("name") == "SharedContentPickerDisplayControls"
                                                        for window in native["windows"]))
                observed_phases.add(phase)
                (phases / (phase + ".ack.json")).write_text(json.dumps({
                    "phase": phase, "externalSnapshotComplete": True,
                    "capturedAt": time.time()}))
                if phase == "after-allow":
                    after_allow = time.monotonic()
            if after_allow is not None and periodic and (
                    time.monotonic() - after_allow >= periodic[0]):
                offset = periodic.pop(0)
                observation = snapshot("after-allow-" + str(offset).replace(".", "p"))
                maybe_press(observation)
            time.sleep(0.1)
        snapshot("after-runner")
        (output / "outcome.json").write_text(json.dumps({
            "browserRunnerExitCode": browser.returncode,
            "phasesObserved": sorted(observed_phases),
            "osSelectionInputSent": (False if action_result is None else
                action_result.get("native", {}).get("pressAttempted")),
            "pickerAbsentBeforeOwnedAllow": baseline_picker_absent,
            "axAction": action_result,
            "axActionRequested": args.press_share_entire_screen,
            "observerCallsExtensionAPI": False,
            "observerAttachesWorker": False,
            "qualificationCredit": 0}, indent=2) + "\n")
        return browser.returncode
    finally:
        # The enclosing reviewed bounded supervisor owns this whole fresh group.
        # It performs identity-checked termination and proves zero survivors.
        if browser is not None and browser.poll() is None:
            browser.terminate()
            try:
                browser.wait(timeout=5)
            except subprocess.TimeoutExpired:
                browser.kill()
                browser.wait(timeout=5)


if __name__ == "__main__":
    raise SystemExit(main())
