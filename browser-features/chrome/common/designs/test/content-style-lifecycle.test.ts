// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createEffect, createSignal } from "solid-js";
import { createRootHMR } from "@nora/solid-xul";
import { registerContentStyleSheets } from "../browser-design-element.tsx";
import { config, setConfig } from "../configs.ts";
import type { getCSSFromConfig } from "../utils/css.ts";
import { TAB_COLOR_LIKE_TOOLBAR_CSS } from "../utils/tab-color-like-toolbar.css.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const sheetService = Cc["@mozilla.org/content/style-sheet-service;1"]
  .getService(
    Ci.nsIStyleSheetService,
  );
const sheetType = Ci.nsIStyleSheetService.AGENT_SHEET as number;
const cssURI = (css: string) =>
  Services.io.newURI(
    `data:text/css;charset=utf-8,${encodeURIComponent(css)}`,
  );
const registered = (uri: nsIURI) =>
  sheetService.sheetRegistered(uri, sheetType);

function createStyleOwner(initial: ReturnType<typeof getCSSFromConfig>) {
  const target = new EventTarget();
  const [styles, setStyles] = createSignal(initial);
  const dispose = createRootHMR(
    (dispose) => {
      createEffect(() => registerContentStyleSheets(styles()));
      return dispose;
    },
    undefined,
    target,
  );
  return { target, setStyles, dispose };
}

async function testSharedContentSheetOwners(): Promise<void> {
  const token = crypto.randomUUID();
  const selector = `[data-floorp-sheet-lease="${token}"]`;
  const oldCSS =
    `${selector} { --floorp-sheet-lease: old; padding-left: 13px !important; }`;
  const newCSS =
    `${selector} { --floorp-sheet-lease: new; padding-left: 29px !important; }`;
  const oldURI = cssURI(oldCSS);
  const newURI = cssURI(newCSS);
  const host = document.createElement("div");
  host.setAttribute("data-floorp-sheet-lease", token);
  host.style.paddingLeft = "0px";
  document.body.appendChild(host);
  let fresh: HTMLDivElement | undefined;
  let first: ReturnType<typeof createStyleOwner> | undefined;
  let survivor: ReturnType<typeof createStyleOwner> | undefined;
  const computed = (element = host) => {
    const style = getComputedStyle(element);
    assert(style, "the real test node has computed styles");
    return style.getPropertyValue("--floorp-sheet-lease").trim();
  };
  const timerWindow: Window = window;
  const waitForComputed = async (
    expected: string,
    label: string,
    element = host,
  ) => {
    // Native sheet changes can queue document restyling after registration has
    // already changed. Require the real computed rule to settle within 2s.
    for (let attempt = 0; attempt < 100; attempt++) {
      if (computed(element) === expected) return;
      await new Promise<void>((resolve) => timerWindow.setTimeout(resolve, 20));
    }
    assertEquals(computed(element), expected, label);
  };
  try {
    assertEquals(registered(oldURI), false, "unique old sheet starts unloaded");
    assertEquals(registered(newURI), false, "unique new sheet starts unloaded");
    first = createStyleOwner({ userjs: null, stylesRaw: [oldCSS] });
    // Exercise both production URL and development raw-CSS paths with one URI.
    survivor = createStyleOwner({ userjs: null, styles: [oldURI.spec] });
    await waitForComputed("old", "native agent sheet reaches a real node");
    assertEquals(
      getComputedStyle(host)?.paddingLeft,
      "13px",
      "native agent sheet changes an ordinary layout property",
    );

    first.target.dispatchEvent(new Event("unload"));
    first.dispose();
    assertEquals(
      registered(oldURI),
      true,
      "first owner unload must preserve the survivor's native registration",
    );
    assertEquals(computed(), "old", "surviving owner's CSS still applies");
    assertEquals(
      getComputedStyle(host)?.paddingLeft,
      "13px",
      "surviving owner's layout rule still applies",
    );

    survivor.setStyles({ userjs: null, stylesRaw: [newCSS] });
    assertEquals(registered(oldURI), false, "theme change releases old URI");
    assertEquals(registered(newURI), true, "theme change acquires new URI");
    await waitForComputed("new", "new theme replaces the old computed rule");
    assertEquals(
      getComputedStyle(host)?.paddingLeft,
      "29px",
      "new theme replaces the ordinary layout rule",
    );

    survivor.dispose();
    survivor.target.dispatchEvent(new Event("unload"));
    assertEquals(registered(newURI), false, "last owner releases its URI");
    // On Windows the locked Runtime can retain removed agent-sheet rules on
    // existing elements, independently reproduced with native APIs and no
    // Floorp/Solid. Native existing-node invalidation remains a QA limitation;
    // owner cleanup must release registration and stop styling new nodes.
    fresh = document.createElement("div");
    fresh.setAttribute("data-floorp-sheet-lease", token);
    fresh.style.paddingLeft = "0px";
    document.body.appendChild(fresh);
    await waitForComputed(
      "",
      "released sheet cannot style a newly created matching node",
      fresh,
    );
    assertEquals(
      getComputedStyle(fresh)?.paddingLeft,
      "0px",
      "released layout rule cannot style a newly created matching node",
    );
  } finally {
    first?.dispose();
    survivor?.dispose();
    fresh?.remove();
    host.remove();
  }
}

function testTabColorSheetBalance(): void {
  const previous = config();
  const uri = cssURI(TAB_COLOR_LIKE_TOOLBAR_CSS);
  let first: ReturnType<typeof createStyleOwner> | undefined;
  let survivor: ReturnType<typeof createStyleOwner> | undefined;
  try {
    // Release the test browser's real theme lease, if present, before checking
    // the two test-owned registrations of this production CSS URI.
    setConfig({
      ...previous,
      globalConfigs: { ...previous.globalConfigs, userInterface: "proton" },
    });
    assertEquals(registered(uri), false, "Proton has no tab-color agent sheet");
    first = createStyleOwner({ userjs: null, useTabColorAsToolbarColor: true });
    survivor = createStyleOwner({
      userjs: null,
      useTabColorAsToolbarColor: true,
    });
    first.target.dispatchEvent(new Event("unload"));
    assert(registered(uri), "first owner close preserves tab-color CSS");

    for (let cycle = 0; cycle < 3; cycle++) {
      survivor.setStyles({ userjs: null, useTabColorAsToolbarColor: false });
      assertEquals(
        registered(uri),
        false,
        "theme change releases tab-color CSS",
      );
      survivor.setStyles({ userjs: null, useTabColorAsToolbarColor: true });
      assert(registered(uri), "theme change reacquires tab-color CSS");
    }
    survivor.dispose();
    first.dispose();
    assertEquals(registered(uri), false, "final close balances tab-color CSS");
  } finally {
    first?.dispose();
    survivor?.dispose();
    setConfig(previous);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("content-style-lifecycle.test.ts", [
    {
      name:
        "native shared sheet survives owner unload and releases on theme change",
      fn: testSharedContentSheetOwners,
    },
    {
      name:
        "tab-color sheet balances two owners, theme changes, and final close",
      fn: testTabColorSheetBalance,
    },
  ]);
}
