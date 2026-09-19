// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import { KeyboardShortcutController } from "../controller.ts";
import { getConfig, isEnabled, setConfig, setEnabled } from "../config.ts";
import { gestureActions } from "../../mouse-gesture/utils/gestures.ts";

function testOptionAndAltGr(): void {
  const previousConfig = getConfig();
  const previousEnabled = isEnabled();
  const action = "gecko-forward";
  const originalAction = gestureActions.getAction(action)!;
  let executions = 0;
  gestureActions.registerAction({ name: action, fn: () => executions++ });
  try {
    for (const platform of ["macosx", "win", "linux"]) {
      for (const focus of ["page", "input", "remote-input"]) {
        for (
          const chord of ["option", "ctrl-option", "cmd-option", "shift-option"]
        ) {
          // Option can change key to a symbol or Dead; code remains physical
          // on US and non-US layouts. The saved shortcut must use that code.
          for (
            const { key, code, textInput } of [
              { key: "π", code: "KeyP", textInput: true },
              { key: "Dead", code: "KeyP", textInput: true },
              { key: "å", code: "KeyP", textInput: true },
              { key: "😀", code: "KeyP", textInput: true },
              { key: "ḍ\u0307", code: "KeyP", textInput: true },
              { key: "👩‍💻", code: "KeyP", textInput: true },
              { key: "ArrowLeft", code: "ArrowLeft", textInput: false },
              { key: "Backspace", code: "Backspace", textInput: false },
              { key: "F1", code: "F1", textInput: false },
            ]
          ) {
            const ctrl = chord === "ctrl-option";
            const meta = chord === "cmd-option";
            const shift = chord === "shift-option";
            setEnabled(true);
            setConfig({
              enabled: true,
              shortcuts: {
                option: {
                  key: code,
                  modifiers: { alt: true, ctrl, meta, shift },
                  action,
                },
              },
            });
            const win = new EventTarget() as unknown as Window;
            Object.defineProperty(win, "document", {
              value: {
                activeElement: {
                  localName: focus === "remote-input"
                    ? "browser"
                    : focus === "input"
                    ? "input"
                    : "body",
                  closest: () => null,
                },
              },
            });
            const controller = new KeyboardShortcutController(
              win,
              { isEditableFocused: () => focus === "remote-input" },
              platform,
            );
            try {
              const expected = platform === "macosx" &&
                (focus === "page" || ctrl || meta || !textInput);
              const event = new KeyboardEvent("keydown", {
                key,
                code,
                altKey: true,
                ctrlKey: ctrl,
                metaKey: meta,
                shiftKey: shift,
                modifierAltGraph: true,
                cancelable: true,
              });
              const before = executions;
              win.dispatchEvent(event);
              const label = `${platform}/${focus}/${chord}/${key}`;
              assertEquals(event.defaultPrevented, expected, label);
              assertEquals(executions - before, expected ? 1 : 0, label);

              const composing = new KeyboardEvent("keydown", {
                key,
                code,
                altKey: true,
                ctrlKey: ctrl,
                metaKey: meta,
                shiftKey: shift,
                modifierAltGraph: true,
                isComposing: true,
                cancelable: true,
              });
              const beforeComposition = executions;
              win.dispatchEvent(composing);
              assertEquals(composing.defaultPrevented, false, `${label}/IME`);
              assertEquals(
                executions,
                beforeComposition,
                `${label}/IME action`,
              );
            } finally {
              controller.destroy();
            }
          }
        }
      }
    }
  } finally {
    gestureActions.registerAction({ name: action, fn: originalAction });
    setConfig(previousConfig);
    setEnabled(previousEnabled);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("keyboardShortcutOption.test.ts", [
    {
      name: "Option shortcuts preserve AltGr, layout and editable input",
      fn: testOptionAndAltGr,
    },
  ]);
}
