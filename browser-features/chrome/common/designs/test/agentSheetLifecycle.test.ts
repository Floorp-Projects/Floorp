// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { h } from "preact";
import { act } from "preact/test-utils";
import { signal } from "@preact/signals";
import { safeRender } from "@nora/preact-xul";
import { useBrowserDesignAgentStyles } from "../browser-design-element.tsx";
import { TAB_COLOR_LIKE_TOOLBAR_CSS } from "../utils/tab-color-like-toolbar.css.ts";
import type { getCSSFromConfig } from "../utils/css.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
  Ci.nsIStyleSheetService,
);
const AGENT_SHEET = Ci.nsIStyleSheetService.AGENT_SHEET as number;
const toURI = (css: string) =>
  Services.io.newURI(`data:text/css;charset=utf-8,${encodeURIComponent(css)}`);

function makeHost(): HTMLElement {
  const host = document.createElementNS("http://www.w3.org/1999/xhtml", "div");
  document.documentElement.appendChild(host);
  return host;
}

async function testIndependentRawAndUrlOwners(): Promise<void> {
  for (const mode of ["raw", "url"]) {
    const token = `lease-${Date.now()}-${Math.random()}`;
    const css = `:root { --floorp-agent-sheet-lifecycle-test: ${token}; }`;
    const uri = toURI(css);
    const settings = signal<ReturnType<typeof getCSSFromConfig>>({
      userjs: null,
      ...(mode === "raw" ? { stylesRaw: [css] } : { styles: [uri.spec] }),
    });
    const Consumer = () => {
      useBrowserDesignAgentStyles(() => settings.value);
      return null;
    };
    const firstHost = makeHost();
    const secondHost = makeHost();
    let disposeFirst = () => {};
    let disposeSecond = () => {};
    try {
      assert(
        !sss.sheetRegistered(uri, AGENT_SHEET),
        "fixture sheet starts absent",
      );
      await act(() => {
        disposeFirst = safeRender(() => h(Consumer, {}), firstHost);
        disposeSecond = safeRender(() => h(Consumer, {}), secondHost);
      });
      assert(
        sss.sheetRegistered(uri, AGENT_SHEET),
        `${mode}: two mounts register the sheet`,
      );
      await act(() => disposeFirst());
      assert(
        sss.sheetRegistered(uri, AGENT_SHEET),
        `${mode}: first disposal preserves the other mount's sheet`,
      );
      assertEquals(
        getComputedStyle(document.documentElement)?.getPropertyValue(
          "--floorp-agent-sheet-lifecycle-test",
        ).trim(),
        token,
        `${mode}: the remaining sheet still affects native computed styles`,
      );
      await act(() => disposeSecond());
      assert(
        !sss.sheetRegistered(uri, AGENT_SHEET),
        `${mode}: final disposal releases the sheet`,
      );
    } finally {
      disposeFirst();
      disposeSecond();
      firstHost.remove();
      secondHost.remove();
      // Isolated URI: keep a failing regression from contaminating later tests.
      while (sss.sheetRegistered(uri, AGENT_SHEET)) {
        sss.unregisterSheet(uri, AGENT_SHEET);
      }
    }
  }
}

// Gecko exposes only a boolean, so count entries by removing and restoring them
// synchronously. No event loop turn occurs while an existing window's CSS is
// removed, and every pre-existing registration is restored before returning.
function countRegistrations(uri: nsIURI): number {
  let count = 0;
  try {
    while (sss.sheetRegistered(uri, AGENT_SHEET)) {
      assert(count < 100, "unexpected number of pre-existing tab-color sheets");
      sss.unregisterSheet(uri, AGENT_SHEET);
      count++;
    }
    return count;
  } finally {
    for (let index = 0; index < count; index++) {
      sss.loadAndRegisterSheet(uri, AGENT_SHEET);
    }
  }
}

async function testTabColorReleasedOnUnmount(): Promise<void> {
  const uri = toURI(TAB_COLOR_LIKE_TOOLBAR_CSS);
  const baseline = countRegistrations(uri);
  function Consumer() {
    useBrowserDesignAgentStyles(() => ({
      userjs: null,
      useTabColorAsToolbarColor: true,
    }));
    return null;
  }
  const firstHost = makeHost();
  const secondHost = makeHost();
  let disposeFirst = () => {};
  let disposeSecond = () => {};
  try {
    await act(() => {
      disposeFirst = safeRender(() => h(Consumer, {}), firstHost);
      disposeSecond = safeRender(() => h(Consumer, {}), secondHost);
    });
    assertEquals(
      countRegistrations(uri),
      baseline + 2,
      "each enabled tab-color consumer owns a registration",
    );
    await act(() => disposeFirst());
    assertEquals(
      countRegistrations(uri),
      baseline + 1,
      "first unmount releases one enabled tab-color sheet",
    );
    await act(() => disposeSecond());
    assertEquals(
      countRegistrations(uri),
      baseline,
      "final unmount preserves only pre-existing registrations",
    );
  } finally {
    disposeFirst();
    disposeSecond();
    firstHost.remove();
    secondHost.remove();
    const remaining = countRegistrations(uri);
    for (let index = baseline; index < remaining; index++) {
      sss.unregisterSheet(uri, AGENT_SHEET);
    }
    for (let index = remaining; index < baseline; index++) {
      sss.loadAndRegisterSheet(uri, AGENT_SHEET);
    }
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("agentSheetLifecycle.test.ts", [
    {
      name: "raw and URL agent sheets survive another mount's disposal",
      fn: testIndependentRawAndUrlOwners,
    },
    {
      name: "enabled tab-color sheets release each mount's registration",
      fn: testTabColorReleasedOnUnmount,
    },
  ]);
}
