// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  BUTTON_ID,
  installTranslateSwapController,
  STYLE_MARKER_ATTR,
  TranslateSwapController,
  uninstallTranslateSwapController,
} from "../controller.ts";
import { canSwapLanguages } from "../swap.ts";
import type {
  NativeMenuList,
  SelectTranslationsPanelApi,
  TranslateSwapDocument,
} from "../types.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type Fixture = {
  doc: TranslateSwapDocument;
  panel: Element;
  row: Element;
  fromMenu: NativeMenuList;
  toMenu: NativeMenuList;
  keypresses: { bubbled: boolean; key: string }[];
  api: SelectTranslationsPanelApi & { calls: string[]; phaseValue: string };
};

const FROM_LANGS = ["en", "tr", "ja"];
const TO_LANGS = ["en", "tr", "de"];

function popupWith(doc: Document, id: string, values: string[]): Element {
  const popup = doc.createElement("div");
  popup.id = id;
  for (const value of values) {
    const item = doc.createElement("menuitem");
    item.setAttribute("value", value);
    popup.appendChild(item);
  }
  return popup;
}

/**
 * Builds a stand-in for the native panel in a detached document so the real
 * browser chrome is never touched. Menulists are plain elements with an
 * expando `value`, which is all the controller reads and writes.
 */
function createFixture(from = "en", to = "tr"): Fixture {
  const doc = document.implementation.createHTMLDocument(
    "translate-swap",
  ) as unknown as TranslateSwapDocument;
  doc.createXULElement = (tag: string) => doc.createElement(tag);

  const panel = doc.createElement("div");
  panel.id = "select-translations-panel";
  const row = doc.createElement("div");
  row.id = "select-translations-panel-lang-selection";

  const fromColumn = doc.createElement("div");
  const fromMenu = doc.createElement("div") as unknown as NativeMenuList;
  fromMenu.id = "select-translations-panel-from";
  fromMenu.value = from;
  fromColumn.append(
    fromMenu,
    popupWith(doc, "select-translations-panel-from-menupopup", FROM_LANGS),
  );

  const toColumn = doc.createElement("div");
  const toMenu = doc.createElement("div") as unknown as NativeMenuList;
  toMenu.id = "select-translations-panel-to";
  toMenu.value = to;
  toColumn.append(
    toMenu,
    popupWith(doc, "select-translations-panel-to-menupopup", TO_LANGS),
  );

  row.append(fromColumn, toColumn);
  panel.appendChild(row);
  doc.body.appendChild(panel);

  const keypresses: Fixture["keypresses"] = [];
  toMenu.addEventListener("keypress", (event) => {
    keypresses.push({
      bubbled: event.bubbles,
      key: (event as KeyboardEvent).key,
    });
  });
  // A bubbling keypress would also reach the panel's own macOS handler.
  panel.addEventListener("keypress", () => {
    keypresses.push({ bubbled: true, key: "panel" });
  });

  const api: Fixture["api"] = {
    calls: [],
    phaseValue: "translatable",
    onChangeFromLanguage() {
      api.calls.push("from");
    },
    onChangeToLanguage() {
      api.calls.push("to");
    },
    phase() {
      return api.phaseValue;
    },
  };

  return { doc, panel, row, fromMenu, toMenu, keypresses, api };
}

function startController(fixture: Fixture): TranslateSwapController {
  const controller = new TranslateSwapController({
    document: fixture.doc,
    getPanelApi: () => fixture.api,
    label: "Swap languages",
  });
  controller.start();
  return controller;
}

function fireOnPanel(fixture: Fixture, type: string): void {
  fixture.panel.dispatchEvent(new Event(type, { bubbles: true }));
}

function swapButton(fixture: Fixture): Element {
  const button = fixture.doc.getElementById(BUTTON_ID);
  assert(button !== null, "swap button should exist");
  return button;
}

function withCapturedErrors(run: () => void): unknown[][] {
  const original = console.error;
  const captured: unknown[][] = [];
  console.error = (...args: unknown[]) => {
    captured.push(args);
  };
  try {
    run();
  } finally {
    console.error = original;
  }
  return captured;
}

function testCanSwapRules(): void {
  const base = {
    from: "en",
    to: "tr",
    fromOptions: FROM_LANGS,
    toOptions: TO_LANGS,
  };
  assert(canSwapLanguages(base), "a valid pair can be swapped");
  assert(!canSwapLanguages({ ...base, from: "" }), "empty source blocks swap");
  assert(!canSwapLanguages({ ...base, to: "" }), "empty target blocks swap");
  assert(
    !canSwapLanguages({ ...base, to: "en" }),
    "identical languages cannot be swapped",
  );
  assert(
    !canSwapLanguages({ ...base, from: "ja", to: "tr" }),
    "source language missing from the target list blocks swap",
  );
  assert(
    !canSwapLanguages({ ...base, from: "tr", to: "de" }),
    "target language missing from the source list blocks swap",
  );
}

function testInjectsOnceBetweenColumns(): void {
  const fixture = createFixture();
  const controller = startController(fixture);

  const button = swapButton(fixture);
  assertEquals(
    button.parentElement,
    fixture.row,
    "button lives in the lang row",
  );
  assertEquals(
    button.nextElementSibling,
    fixture.toMenu.parentElement,
    "button sits right before the target column",
  );
  assertEquals(
    button.getAttribute("tooltiptext"),
    "Swap languages",
    "button carries the label",
  );

  fireOnPanel(fixture, "popupshowing");
  fireOnPanel(fixture, "popupshown");
  assertEquals(
    fixture.doc.querySelectorAll(`#${BUTTON_ID}`).length,
    1,
    "repeated popup events do not duplicate the button",
  );
  controller.destroy();
}

function testInjectsWhenPanelAppearsLater(): void {
  const fixture = createFixture();
  fixture.panel.remove();
  const controller = startController(fixture);
  assertEquals(
    fixture.doc.getElementById(BUTTON_ID),
    null,
    "no button before the panel exists",
  );

  fixture.doc.body.appendChild(fixture.panel);
  fireOnPanel(fixture, "popupshowing");
  assert(
    fixture.doc.getElementById(BUTTON_ID) !== null,
    "button appears on the first popupshowing",
  );
  controller.destroy();
}

function testSwapExchangesLanguagesAndRetranslates(): void {
  const fixture = createFixture("en", "tr");
  const controller = startController(fixture);

  const errors = withCapturedErrors(() => controller.swap());

  assertEquals(fixture.fromMenu.value, "tr", "source becomes the old target");
  assertEquals(fixture.toMenu.value, "en", "target becomes the old source");
  assertEquals(
    fixture.api.calls.join(","),
    "from,to",
    "panel is told about both language changes",
  );
  assertEquals(
    fixture.keypresses.length,
    1,
    "exactly one keypress reaches the target menu and none the panel",
  );
  assertEquals(
    fixture.keypresses[0]?.key,
    "Enter",
    "Enter requests translation",
  );
  assertEquals(
    fixture.keypresses[0]?.bubbled,
    false,
    "keypress does not bubble",
  );
  assertEquals(errors.length, 0, "no error when translation starts");
  controller.destroy();
}

function testSwapReportsWhenPanelDoesNotTranslate(): void {
  const fixture = createFixture("en", "tr");
  fixture.api.phaseValue = "translated";
  const controller = startController(fixture);

  const errors = withCapturedErrors(() => controller.swap());
  assertEquals(errors.length, 1, "a stalled panel is logged");
  assertEquals(
    errors[0]?.[0],
    "[translate-swap]",
    "log carries the feature tag",
  );
  controller.destroy();
}

function testSwapIsNoopForInvalidPairs(): void {
  const same = createFixture("en", "en");
  const sameController = startController(same);
  sameController.swap();
  assertEquals(same.fromMenu.value, "en", "identical pair is left alone");
  assertEquals(same.keypresses.length, 0, "no translation is requested");
  assertEquals(same.api.calls.length, 0, "panel is not notified");
  sameController.destroy();

  const missing = createFixture("ja", "tr");
  const missingController = startController(missing);
  missingController.swap();
  assertEquals(missing.fromMenu.value, "ja", "unswappable pair is left alone");
  assertEquals(missing.toMenu.value, "tr", "target is left alone");
  missingController.destroy();
}

function testButtonStateFollowsSelection(): void {
  const fixture = createFixture("en", "tr");
  const controller = startController(fixture);
  fireOnPanel(fixture, "popupshown");
  assert(
    !swapButton(fixture).hasAttribute("disabled"),
    "valid pair is enabled",
  );

  fixture.toMenu.value = "en";
  const menuItem = fixture.doc.createElement("menuitem");
  fixture.toMenu.parentElement?.appendChild(menuItem);
  menuItem.dispatchEvent(new Event("command", { bubbles: true }));
  assert(
    swapButton(fixture).hasAttribute("disabled"),
    "a menu change inside the panel refreshes the button",
  );

  fixture.toMenu.value = "de";
  fixture.fromMenu.value = "ja";
  menuItem.dispatchEvent(new Event("command", { bubbles: true }));
  assert(
    swapButton(fixture).hasAttribute("disabled"),
    "a language missing from the opposite list disables the button",
  );

  fixture.fromMenu.value = "tr";
  fixture.toMenu.value = "en";
  menuItem.dispatchEvent(new Event("command", { bubbles: true }));
  assert(
    !swapButton(fixture).hasAttribute("disabled"),
    "a valid pair re-enables the button",
  );
  controller.destroy();
}

function testCommandOutsidePanelIsIgnored(): void {
  const fixture = createFixture("en", "tr");
  const controller = startController(fixture);
  fixture.toMenu.value = "en";
  const outside = fixture.doc.createElement("div");
  fixture.doc.body.appendChild(outside);
  outside.dispatchEvent(new Event("command", { bubbles: true }));
  assert(
    !swapButton(fixture).hasAttribute("disabled"),
    "unrelated command events do not touch the button",
  );
  controller.destroy();
}

function testLabelFollowsLocale(): void {
  const fixture = createFixture();
  const controller = startController(fixture);
  controller.setLabel("言語を入れ替える");
  assertEquals(
    swapButton(fixture).getAttribute("aria-label"),
    "言語を入れ替える",
    "label is refreshed when the locale changes",
  );
  controller.destroy();
}

function testDestroyRemovesEverything(): void {
  const fixture = createFixture();
  const controller = startController(fixture);
  assert(
    fixture.doc.querySelector(`[${STYLE_MARKER_ATTR}]`) !== null,
    "style is installed",
  );

  controller.destroy();
  assertEquals(fixture.doc.getElementById(BUTTON_ID), null, "button removed");
  assertEquals(
    fixture.doc.querySelector(`[${STYLE_MARKER_ATTR}]`),
    null,
    "style removed",
  );

  fireOnPanel(fixture, "popupshowing");
  assertEquals(
    fixture.doc.getElementById(BUTTON_ID),
    null,
    "listeners are detached after destroy",
  );
}

function testReinstallReplacesPreviousController(): void {
  const fixture = createFixture();
  const host: Record<string, unknown> = {};
  const options = {
    document: fixture.doc,
    getPanelApi: () => fixture.api,
    label: "Swap languages",
  };

  const first = installTranslateSwapController(options, host);
  const second = installTranslateSwapController(options, host);

  assertEquals(
    fixture.doc.querySelectorAll(`#${BUTTON_ID}`).length,
    1,
    "hot reload keeps a single button",
  );
  assertEquals(
    fixture.doc.querySelectorAll(`[${STYLE_MARKER_ATTR}]`).length,
    1,
    "hot reload keeps a single style",
  );

  // The replaced controller is already destroyed; its late cleanup must not
  // tear down the artifacts or the slot that now belong to the live one.
  uninstallTranslateSwapController(first, host);
  assert(
    fixture.doc.getElementById(BUTTON_ID) !== null,
    "a stale controller's cleanup leaves the live button alone",
  );
  assertEquals(Object.keys(host).length, 1, "slot still holds the live one");

  uninstallTranslateSwapController(second, host);
  assertEquals(
    fixture.doc.getElementById(BUTTON_ID),
    null,
    "the live controller's cleanup removes its button",
  );
  assertEquals(Object.keys(host).length, 0, "slot is released");
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "canSwapLanguages rules", fn: testCanSwapRules },
    {
      name: "button is injected once between columns",
      fn: testInjectsOnceBetweenColumns,
    },
    {
      name: "button appears when the lazy panel shows",
      fn: testInjectsWhenPanelAppearsLater,
    },
    {
      name: "swap exchanges languages and retranslates",
      fn: testSwapExchangesLanguagesAndRetranslates,
    },
    {
      name: "swap logs when the panel does not translate",
      fn: testSwapReportsWhenPanelDoesNotTranslate,
    },
    {
      name: "swap is a no-op for invalid pairs",
      fn: testSwapIsNoopForInvalidPairs,
    },
    {
      name: "button state follows the selection",
      fn: testButtonStateFollowsSelection,
    },
    {
      name: "command events outside the panel are ignored",
      fn: testCommandOutsidePanelIsIgnored,
    },
    { name: "label follows the locale", fn: testLabelFollowsLocale },
    {
      name: "destroy removes button, style and listeners",
      fn: testDestroyRemovesEverything,
    },
    {
      name: "reinstall replaces the previous controller",
      fn: testReinstallReplacesPreviousController,
    },
  ];

  await runTests("translateSwap.test.ts", tests);
}
