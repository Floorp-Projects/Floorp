// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createEffect, createRoot } from "solid-js";
import { config } from "../config.ts";
import { updatePwaToolbarVisibility } from "../toolbarVisibility.ts";
import styles from "../pwa-window-style.css?inline";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

export async function runAllTests(): Promise<void> {
  await runTests("toolbarVisibility.test.ts", [{
    name:
      "legacy false, new defaults and live preference changes control the whole toolbar",
    fn() {
      const pref = "floorp.browser.ssb.config";
      const hadPref = Services.prefs.prefHasUserValue(pref);
      const oldValue = Services.prefs.getStringPref(pref, "{}");
      const doc = document;
      const hadHiddenToolbar = doc.documentElement.hasAttribute(
        "floorp-pwa-hide-toolbar",
      );
      const style = doc.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "style",
      );
      style.textContent = styles;
      doc.documentElement.appendChild(style);
      const nav = doc.getElementById("nav-bar")!;
      const bookmarks = doc.getElementById("PersonalToolbar")!;
      const collapsed = bookmarks.getAttribute("collapsed");
      const inlineStyle = bookmarks.getAttribute("style");
      let dispose = () => {};
      try {
        // Existing configurations (including unknown future keys) keep false.
        Services.prefs.setStringPref(
          pref,
          '{"showToolbar":false,"futureKey":"keep"}',
        );
        createRoot((cleanup) => {
          dispose = cleanup;
          createEffect(() =>
            updatePwaToolbarVisibility(doc, config().showToolbar)
          );
        });
        const display = () => doc.defaultView!.getComputedStyle(nav)!.display;
        assertEquals(
          display(),
          "none",
          "disabled hides entire bar, including icon and strip",
        );
        assertEquals(
          JSON.parse(Services.prefs.getStringPref(pref)).futureKey,
          "keep",
          "unknown settings retained",
        );
        Services.prefs.setStringPref(pref, '{"showToolbar":true}');
        assert(display() !== "none", "live enable restores toolbar");
        Services.prefs.setStringPref(pref, '{"showToolbar":false}');
        assertEquals(display(), "none", "live disable hides toolbar again");
        Services.prefs.setStringPref(pref, "{}");
        assertEquals(
          config().showToolbar,
          true,
          "missing setting uses new-install default",
        );
        assert(display() !== "none", "new configuration shows toolbar");
        assertEquals(
          bookmarks.getAttribute("collapsed"),
          collapsed,
          "existing bookmark visibility retained",
        );
        assertEquals(
          bookmarks.getAttribute("style"),
          inlineStyle,
          "unrelated inline styles retained",
        );
      } finally {
        dispose();
        style.remove();
        doc.documentElement.toggleAttribute(
          "floorp-pwa-hide-toolbar",
          hadHiddenToolbar,
        );
        if (hadPref) Services.prefs.setStringPref(pref, oldValue);
        else Services.prefs.clearUserPref(pref);
      }
    },
  }]);
}
