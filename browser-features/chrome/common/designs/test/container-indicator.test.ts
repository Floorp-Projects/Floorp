// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import leptonChromeCSS from "@nora/skin/lepton/css/leptonChrome.css?raw";
import { LEPTON_COMPAT_CSS } from "../utils/lepton-compat-152.css.ts";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

function testContainerIdentitySurvivesPhotonSelection(): void {
  const pref = "userChrome.tab.photon_like_contextline";
  const hadPref = Services.prefs.prefHasUserValue(pref);
  const oldPref = Services.prefs.getBoolPref(pref, false);
  const host = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
  host.style.cssText = "position:fixed;left:-10000px;top:0;width:300px";
  const shadow = host.attachShadow({ mode: "closed" });
  const style = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "style",
  );
  style.textContent = leptonChromeCSS + "\n" + LEPTON_COMPAT_CSS +
    "\n* { transition: none !important; animation: none !important; }";
  const tabs = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
  tabs.id = "tabbrowser-tabs";
  shadow.append(style, tabs);
  document.documentElement.appendChild(host);
  try {
    Services.prefs.setBoolPref(pref, true);
    for (const color of ["rgb(55, 173, 255)", "rgb(255, 159, 0)"]) {
      for (
        const state of [
          "selected-empty",
          "selected-true",
          "multiselected",
          "inactive",
        ]
      ) {
        const tab = document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "div",
        );
        tab.className = "tabbrowser-tab";
        tab.setAttribute("usercontextid", "1");
        tab.style.setProperty("--identity-icon-color", color);
        if (state === "selected-empty") tab.setAttribute("selected", "");
        if (state === "selected-true") tab.setAttribute("selected", "true");
        if (state === "multiselected") {
          tab.setAttribute("multiselected", "true");
        }
        const stack = document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "div",
        );
        stack.className = "tab-stack";
        const background = document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "div",
        );
        background.className = "tab-background";
        const line = document.createElementNS(
          "http://www.w3.org/1999/xhtml",
          "div",
        );
        line.className = "tab-context-line";
        background.append(line);
        stack.append(background);
        tab.append(stack);
        tabs.append(tab);
        const computed = getComputedStyle(line);
        if (!computed) throw new Error("Container line has no computed style");
        assertEquals(
          computed.opacity,
          "1",
          `${state}: identity remains visible`,
        );
        assertEquals(
          computed.transform,
          "none",
          `${state}: identity is not collapsed`,
        );
        assertEquals(
          computed.backgroundColor,
          color,
          `${state}: container's own color`,
        );
        tabs.setAttribute("orient", "vertical");
        const verticalStyle = getComputedStyle(line);
        if (!verticalStyle) {
          throw new Error("Vertical line has no computed style");
        }
        assertEquals(
          verticalStyle.height,
          "24px",
          `${state}: vertical indicator height`,
        );
        tabs.removeAttribute("orient");
        tab.remove();
      }
    }
  } finally {
    host.remove();
    if (hadPref) Services.prefs.setBoolPref(pref, oldPref);
    else Services.prefs.clearUserPref(pref);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("container-indicator", [{
    name: "Photon keeps each container color across selection representations",
    fn: testContainerIdentitySurvivesPhotonSelection,
  }]);
}

await runAllTests();
