// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
//
// The vendored Lepton sheets (browser-features/skin/lepton/css/) must stay
// byte-identical to upstream; Floorp changes go to the overlay sheets in
// browser-features/skin/lepton/overlay/. These tests pin where the overlays
// load: right after their Lepton sheet, at the same sheet level, for exactly
// the designs that load that Lepton sheet.

import { createRoot } from "solid-js";
import leptonChromeStylesRaw from "@nora/skin/lepton/css/leptonChrome.css?raw";
import leptonContentStylesRaw from "@nora/skin/lepton/css/leptonContent.css?raw";
import leptonChromeOverlayRaw from "@nora/skin/lepton/overlay/leptonChromeOverlay.css?raw";
import leptonContentOverlayRaw from "@nora/skin/lepton/overlay/leptonContentOverlay.css?raw";
import {
  registerContentStyleSheets,
  replaceIconPaths,
} from "../browser-design-element.tsx";
import { config } from "../configs.ts";
import { getCSSFromConfig } from "../utils/css.ts";
import type { TFloorpDesignConfigs } from "../type.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

type Design = TFloorpDesignConfigs["globalConfigs"]["userInterface"];

const SKIN = "chrome://noraneko-skin/content/lepton";
const LEPTON_CHROME = `${SKIN}/css/leptonChrome.css`;
const LEPTON_CONTENT = `${SKIN}/css/leptonContent.css`;
const CHROME_OVERLAY = `${SKIN}/overlay/leptonChromeOverlay.css`;
const CONTENT_OVERLAY = `${SKIN}/overlay/leptonContentOverlay.css`;

const LEPTON_DESIGNS: Design[] = ["lepton", "photon", "protonfix"];
const OTHER_DESIGNS: Design[] = ["fluerial", "proton"];

function makeConfig(ui: Design): TFloorpDesignConfigs {
  const current = config();
  return {
    ...current,
    globalConfigs: { ...current.globalConfigs, userInterface: ui },
  };
}

/** Indexes of `item` in `list`. */
function positions<T>(list: T[] | undefined, item: T): number[] {
  return (list ?? []).flatMap((value, i) => (value === item ? [i] : []));
}

/**
 * Every load of `sheet` in `list` is directly followed by `overlay`, and
 * `overlay` is loaded nowhere else.
 */
function assertOverlayFollows<T>(
  list: T[] | undefined,
  sheet: T,
  overlay: T,
  where: string,
): number {
  const sheetAt = positions(list, sheet);
  const overlayAt = positions(list, overlay);
  assertEquals(
    JSON.stringify(overlayAt),
    JSON.stringify(sheetAt.map((i) => i + 1)),
    `${where}: overlay must load right after each Lepton sheet ` +
      `(sheet at ${JSON.stringify(sheetAt)})`,
  );
  return sheetAt.length;
}

function testOverlaySourcesAreDistinct(): void {
  // positions() matches raw CSS by value; distinct texts keep it exact.
  const raws = [
    leptonChromeStylesRaw,
    leptonContentStylesRaw,
    leptonChromeOverlayRaw,
    leptonContentOverlayRaw,
  ];
  assertEquals(new Set(raws).size, 4, "the four sheets must differ");
  assert(
    leptonChromeOverlayRaw.includes(
      "Floorp overlay for ../css/leptonChrome.css",
    ),
    "chrome overlay must carry its header",
  );
  assert(
    leptonContentOverlayRaw.includes(
      "Floorp overlay for ../css/leptonContent.css",
    ),
    "content overlay must carry its header",
  );
}

function testProductionOrder(): void {
  for (const design of LEPTON_DESIGNS) {
    const css = getCSSFromConfig(makeConfig(design), false);
    assertEquals(
      assertOverlayFollows(
        css.chromeStyles,
        LEPTON_CHROME,
        CHROME_OVERLAY,
        `${design} chromeStyles`,
      ),
      1,
      `${design} must load leptonChrome.css once as a chrome sheet`,
    );
    assertEquals(
      assertOverlayFollows(
        css.chromeStyles,
        LEPTON_CONTENT,
        CONTENT_OVERLAY,
        `${design} chromeStyles`,
      ),
      1,
      `${design} must load leptonContent.css once as a chrome sheet`,
    );
    const agentLoads = assertOverlayFollows(
      css.styles,
      LEPTON_CONTENT,
      CONTENT_OVERLAY,
      `${design} styles (AGENT_SHEET)`,
    );
    assertEquals(
      agentLoads,
      design === "lepton" ? 0 : 1,
      `${design} AGENT_SHEET loads of leptonContent.css`,
    );
    for (const raw of css.chromeStylesRaw ?? []) {
      assert(
        raw !== leptonChromeOverlayRaw && raw !== leptonContentOverlayRaw,
        `${design} production must load the overlays by URL, not inline`,
      );
    }
  }
}

function testDevOrder(): void {
  for (const design of LEPTON_DESIGNS) {
    const css = getCSSFromConfig(makeConfig(design), true);
    assertEquals(
      assertOverlayFollows(
        css.chromeStylesRaw,
        leptonChromeStylesRaw,
        leptonChromeOverlayRaw,
        `${design} dev chromeStylesRaw`,
      ),
      1,
      `${design} dev must inline leptonChrome.css once`,
    );
    const chromeContent = assertOverlayFollows(
      css.chromeStylesRaw,
      leptonContentStylesRaw,
      leptonContentOverlayRaw,
      `${design} dev chromeStylesRaw`,
    );
    const agentContent = assertOverlayFollows(
      css.stylesRaw,
      leptonContentStylesRaw,
      leptonContentOverlayRaw,
      `${design} dev stylesRaw (AGENT_SHEET)`,
    );
    assertEquals(
      chromeContent + agentContent,
      1,
      `${design} dev must load leptonContent.css once`,
    );
  }
}

function testOtherDesignsLoadNoOverlay(): void {
  for (const design of OTHER_DESIGNS) {
    for (const isDev of [true, false]) {
      const css = getCSSFromConfig(makeConfig(design), isDev);
      const all: string[] = [
        ...(css.chromeStyles ?? []),
        ...(css.chromeStylesRaw ?? []),
        ...(css.styles ?? []),
        ...(css.stylesRaw ?? []),
      ];
      for (
        const item of [
          CHROME_OVERLAY,
          CONTENT_OVERLAY,
          leptonChromeOverlayRaw,
          leptonContentOverlayRaw,
        ]
      ) {
        assert(
          !all.includes(item),
          `${design} (dev=${isDev}) must not load a Lepton overlay`,
        );
      }
    }
  }
}

async function testOverlayUrlsResolve(): Promise<void> {
  for (
    const [url, raw] of [
      [CHROME_OVERLAY, leptonChromeOverlayRaw],
      [CONTENT_OVERLAY, leptonContentOverlayRaw],
    ] as const
  ) {
    const response = await fetch(url);
    assert(response.ok, `${url} must be served (status ${response.status})`);
    assertEquals(
      (await response.text()).replaceAll("\r\n", "\n"),
      raw.replaceAll("\r\n", "\n"),
      `${url} must serve the overlay source`,
    );
  }
}

function testProductionAgentSheetsRegisterTogether(): void {
  const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
    Ci.nsIStyleSheetService,
  );
  const AGENT = Ci.nsIStyleSheetService.AGENT_SHEET as number;
  const lepton = Services.io.newURI(LEPTON_CONTENT);
  const overlay = Services.io.newURI(CONTENT_OVERLAY);
  const wasRegistered = sss.sheetRegistered(overlay, AGENT);
  const dispose = createRoot((dispose) => {
    registerContentStyleSheets(getCSSFromConfig(makeConfig("photon"), false));
    return dispose;
  });
  try {
    assert(
      sss.sheetRegistered(lepton, AGENT),
      "photon must register leptonContent.css as an AGENT_SHEET",
    );
    assert(
      sss.sheetRegistered(overlay, AGENT),
      "photon must register the content overlay as an AGENT_SHEET",
    );
  } finally {
    dispose();
  }
  assertEquals(
    sss.sheetRegistered(overlay, AGENT),
    wasRegistered,
    "disposing the owner must unregister the overlay again",
  );
}

/**
 * The running window: when a Lepton-family design is active, its chrome
 * sheets are in the document, and the overlay comes right after Lepton.
 */
function testLiveWindowOrder(): void {
  const design = config().globalConfigs.userInterface;
  if (!LEPTON_DESIGNS.includes(design)) {
    console.log(`[lepton-overlay.test] live check skipped: design ${design}`);
    return;
  }
  const css = getCSSFromConfig(config());
  if (css.chromeStyles?.length) {
    const hrefs = Array.from(
      document.querySelectorAll<HTMLLinkElement>("link[rel=stylesheet]"),
      (link) => link.getAttribute("href") ?? "",
    );
    assertOverlayFollows(hrefs, LEPTON_CHROME, CHROME_OVERLAY, "live <link>s");
    assertOverlayFollows(
      hrefs,
      LEPTON_CONTENT,
      CONTENT_OVERLAY,
      "live <link>s",
    );
    return;
  }
  const inline = Array.from(
    document.querySelectorAll("style"),
    (style) => style.textContent ?? "",
  ).find((text) => text.includes("Floorp overlay for ../css/leptonChrome.css"));
  assert(inline, `${design}: the chrome overlay must be in the document`);
  const leptonAt = inline.indexOf(
    replaceIconPaths(leptonChromeStylesRaw, css.iconBasePath).slice(0, 400),
  );
  const leptonEnd = leptonAt +
    replaceIconPaths(leptonChromeStylesRaw, css.iconBasePath).length;
  const overlayAt = inline.indexOf(
    replaceIconPaths(leptonChromeOverlayRaw, css.iconBasePath),
  );
  assert(leptonAt >= 0, `${design}: leptonChrome.css must be inlined`);
  assertEquals(
    overlayAt,
    leptonEnd + 1,
    `${design}: the chrome overlay must follow leptonChrome.css directly`,
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("lepton-overlay.test.ts", [
    { name: "overlay sources are distinct", fn: testOverlaySourcesAreDistinct },
    {
      name: "production: overlays load right after their Lepton sheet",
      fn: testProductionOrder,
    },
    {
      name: "dev: overlays load right after their Lepton sheet",
      fn: testDevOrder,
    },
    {
      name: "fluerial and proton load no Lepton overlay",
      fn: testOtherDesignsLoadNoOverlay,
    },
    {
      name: "overlay chrome:// URLs are served",
      fn: testOverlayUrlsResolve,
    },
    {
      name: "production content overlay registers as AGENT_SHEET",
      fn: testProductionAgentSheetsRegisterTogether,
    },
    {
      name: "live window: chrome overlay follows Lepton",
      fn: testLiveWindowOrder,
    },
  ]);
}
