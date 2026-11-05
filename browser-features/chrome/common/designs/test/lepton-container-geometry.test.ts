// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import leptonChromeCSS from "@nora/skin/lepton/css/leptonChrome.css?raw";
import { LEPTON_COMPAT_CSS } from "../utils/lepton-compat-152.css.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const PREFS = {
  "userChrome.tab.container": true,
  "userChrome.tab.photon_like_contextline": false,
  "userChrome.tabbar.as_titlebar": false,
  "userChrome.tab.container.always_long": false,
  "userChrome.tab.container.on_top": false,
  "userChrome.tab.close_button_at_pinned": false,
};

async function withContainerFixture(
  check: (
    tabs: HTMLElement,
    content: HTMLElement,
  ) => void | Promise<void>,
  color?: string,
): Promise<void> {
  const saved = Object.entries(PREFS).map(([name, value]) => ({
    name,
    value,
    hadUserValue: Services.prefs.prefHasUserValue(name),
    previous: Services.prefs.getBoolPref(name, false),
  }));
  const element = (className: string): HTMLElement => {
    const node = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "div",
    );
    node.className = className;
    return node;
  };
  const host = element("lepton-container-fixture");
  host.style.cssText = "position:fixed;left:-10000px;top:0;width:300px";
  const shadow = host.attachShadow({ mode: "closed" });
  const style = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "style",
  );
  // Use the actual vendor rules, including their inactive/selected width
  // cascade. Layout scaffolding supplies only native tokens at the consumer;
  // declaring legacy tokens here would conceal the regression.
  style.textContent = leptonChromeCSS + "\n" + LEPTON_COMPAT_CSS + `
    *, *::before, *::after { transition: none !important; animation: none !important; }
    .tabbrowser-tab, .tab-stack { display:block; width:240px; height:32px; }
    .tab-stack, .tab-content { position:relative; }
    .tab-content { display:block; width:240px; height:32px; }
  `;
  const tabs = element("");
  tabs.id = "tabbrowser-tabs";
  const tab = element("tabbrowser-tab");
  if (color) {
    tab.setAttribute("usercontextid", "1");
    tab.style.setProperty("--identity-icon-color", color);
  }
  const stack = element("tab-stack");
  const background = element("tab-background");
  background.append(element("tab-context-line"));
  const content = element("tab-content");
  content.style.setProperty("--tab-padding-inline", "8px");
  content.style.setProperty("--tab-margin-inline-inner", "4px");
  stack.append(background, content);
  tab.append(stack);
  tabs.append(tab);
  shadow.append(style, tabs);
  document.documentElement.append(host);
  try {
    for (const pref of saved) {
      Services.prefs.setBoolPref(pref.name, pref.value);
    }
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
    await check(tabs, content);
  } finally {
    InspectorUtils.removePseudoClassLock(tab, ":hover");
    host.remove();
    for (const pref of saved) {
      if (pref.hadUserValue) {
        Services.prefs.setBoolPref(pref.name, pref.previous);
      } else Services.prefs.clearUserPref(pref.name);
    }
  }
}

async function testAlternativeLineGeometry(): Promise<void> {
  for (const color of ["rgb(55, 173, 255)", "rgb(255, 159, 0)"]) {
    await withContainerFixture(async (tabs, content) => {
      const tab = content.parentElement!.parentElement!;
      for (const orientation of ["horizontal", "vertical"]) {
        tabs.setAttribute("orient", orientation);
        tabs.setAttribute("expanded", "");
        for (
          const state of [
            "inactive",
            "selected",
            "multiselected",
            "selected-multiselected",
          ]
        ) {
          tab.toggleAttribute("selected", state.startsWith("selected"));
          tab.toggleAttribute("visuallyselected", state.startsWith("selected"));
          tab.toggleAttribute("multiselected", state.includes("multiselected"));
          for (const hover of [false, true]) {
            if (hover) InspectorUtils.addPseudoClassLock(tab, ":hover");
            else InspectorUtils.removePseudoClassLock(tab, ":hover");
            assertEquals(tab.matches(":hover"), hover, "real hover selector");
            await new Promise<void>((resolve) =>
              requestAnimationFrame(() => resolve())
            );
            // Read the used width after the vendor pseudo transition settles.
            await new Promise<void>((resolve) => setTimeout(resolve, 350));
            const pseudo = getComputedStyle(content, "::before");
            assert(pseudo, "container pseudo has computed style");
            assertEquals(
              pseudo.width,
              state.startsWith("selected") ? "216px" : "60px",
              `${orientation}/${state}/${hover}: vendor line keeps its width; content=${content.clientWidth}/${
                getComputedStyle(content)?.width
              }; transition=${pseudo.transitionProperty}/${pseudo.transitionDuration}; animation=${pseudo.animationName}`,
            );
            assertEquals(pseudo.borderBottomWidth, "2px", "line thickness");
            assertEquals(pseudo.borderBottomColor, color, "container color");
            assertEquals(
              pseudo.opacity,
              state.startsWith("selected") ? "1" : "0.75",
              "vendor selection opacity is preserved",
            );
          }
        }
      }
      tabs.setAttribute("orient", "vertical");
      tabs.removeAttribute("expanded");
      tab.setAttribute("selected", "");
      tab.setAttribute("visuallyselected", "");
      await new Promise<void>((resolve) => setTimeout(resolve, 350));
      assertEquals(
        getComputedStyle(content, "::before")?.width,
        "16px",
        "collapsed vertical vendor indicator keeps its fixed width",
      );
    }, color);
  }
}

async function testAliasScopeAndPreferenceGates(): Promise<void> {
  await withContainerFixture((_tabs, content) => {
    const computed = getComputedStyle(content);
    assertEquals(
      computed?.getPropertyValue("--tab-inline-padding").trim(),
      "",
      "non-container control gets no compatibility tokens",
    );
    assertEquals(
      getComputedStyle(content, "::before")?.borderBottomWidth,
      "0px",
      "non-container control gets no identity line",
    );
  });
  await withContainerFixture(async (_tabs, content) => {
    for (
      const [name, inactive] of [
        ["userChrome.tab.container", false],
        ["userChrome.tab.photon_like_contextline", true],
        ["userChrome.tabbar.as_titlebar", true],
      ] as const
    ) {
      Services.prefs.setBoolPref(name, inactive);
      await new Promise<void>((resolve) =>
        requestAnimationFrame(() => resolve())
      );
      assertEquals(
        getComputedStyle(content)?.getPropertyValue("--tab-inline-padding")
          .trim(),
        "",
        `${name}: unrelated rendering path gets no aliases`,
      );
      Services.prefs.setBoolPref(name, PREFS[name]);
    }
  }, "rgb(55, 173, 255)");
}

export async function runAllTests(): Promise<void> {
  await runTests("lepton-container-geometry", [
    {
      name: "vendor container underline keeps native-token geometry",
      fn: testAlternativeLineGeometry,
    },
    {
      name: "non-container, Photon and titlebar paths stay outside alias scope",
      fn: testAliasScopeAndPreferenceGates,
    },
  ]);
}
