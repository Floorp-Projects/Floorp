// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { GECKO_152_VAR_ALIASES_CSS } from "../utils/gecko-152-var-aliases.css.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

/** Model the layered native token separately from the frame's inherited color.
 * The old alias invalidated the toolbar token when legacy variables were absent,
 * or replaced it with an unrelated legacy color when they were present. */
function preservesToolbarColor(
  nativeColor: string,
  inheritedColor: string,
  legacyTokens = "",
): void {
  const html = "http://www.w3.org/1999/xhtml";
  const host = document.createElementNS(html, "div");
  host.style.cssText =
    "position: fixed; left: -10000px; width: 1px; height: 1px;";
  const shadow = host.attachShadow({ mode: "open" });
  document.documentElement.appendChild(host);
  try {
    const win = document.defaultView;
    assert(win, "browser window must be available");
    const style = document.createElementNS(html, "style");
    // Scope only the root selector to an isolated host; retain the production
    // declarations and native-token cascade layers without changing browser UI.
    const aliases = GECKO_152_VAR_ALIASES_CSS.replace(":root {", ":host {");
    style.textContent = `
      @layer native-tokens {
        :host { --toolbar-text-color: ${nativeColor}; }
      }
      :host {
        --lwt-text-color: initial;
        --toolbar-color: initial;
        ${legacyTokens}
        color: ${inheritedColor};
      }
      #toolbar { color: var(--toolbar-text-color); }
      #selected-tab { color: inherit; }
      ${aliases}
    `;
    shadow.appendChild(style);
    const toolbar = document.createElementNS(html, "div");
    toolbar.id = "toolbar";
    const tab = document.createElementNS(html, "span");
    tab.id = "selected-tab";
    toolbar.appendChild(tab);
    shadow.appendChild(toolbar);
    for (const element of [toolbar, tab]) {
      assertEquals(
        win.getComputedStyle(element).color,
        nativeColor,
        `${element.id} must retain the native/theme toolbar text color`,
      );
    }
  } finally {
    host.remove();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("toolbar-native-color.test.ts", [
    {
      name: "light toolbar keeps dark text without legacy tokens",
      fn: () => preservesToolbarColor("rgb(21, 20, 26)", "rgb(255, 255, 255)"),
    },
    {
      name: "dark toolbar keeps light text without legacy tokens",
      fn: () => preservesToolbarColor("rgb(255, 255, 255)", "rgb(0, 0, 0)"),
    },
    {
      name: "theme toolbar color outranks unrelated legacy colors",
      fn: () =>
        preservesToolbarColor(
          "rgb(30, 60, 90)",
          "rgb(255, 255, 255)",
          "--lwt-text-color: rgb(180, 30, 40); --toolbar-color: rgb(20, 160, 50);",
        ),
    },
  ]);
}

await runAllTests();
