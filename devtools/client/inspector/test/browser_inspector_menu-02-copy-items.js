/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the various copy items in the context menu works correctly.

const TEST_URL = URL_ROOT + "doc_inspector_menu.html";
const SELECTOR_UNIQUE = "devtools.copy.unique.css.selector.opened";
const SELECTOR_FULL = "devtools.copy.full.css.selector.opened";
const XPATH = "devtools.copy.xpath.opened";

const COPY_ITEMS_TEST_DATA = [
  {
    desc: "copy inner html",
    id: "node-menu-copyinner",
    selector: "[data-id=\"copy\"]",
    text: "Paragraph for testing copy",
  },
  {
    desc: "copy outer html",
    id: "node-menu-copyouter",
    selector: "[data-id=\"copy\"]",
    text: "<p data-id=\"copy\">Paragraph for testing copy</p>",
  },
  {
    desc: "copy unique selector",
    id: "node-menu-copyuniqueselector",
    selector: "[data-id=\"copy\"]",
    text: "body > div:nth-child(1) > p:nth-child(2)",
  },
  {
    desc: "copy CSS path",
    id: "node-menu-copycsspath",
    selector: "[data-id=\"copy\"]",
    text: "html body div p",
  },
  {
    desc: "copy XPath",
    id: "node-menu-copyxpath",
    selector: "[data-id=\"copy\"]",
    text: "/html/body/div/p[1]",
  },
  {
    desc: "copy image data uri",
    id: "node-menu-copyimagedatauri",
    selector: "#copyimage",
    text: "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABC" +
      "AAAAAA6fptVAAAACklEQVQYV2P4DwABAQEAWk1v8QAAAABJRU5ErkJggg==",
  },
];

add_task(function* () {
  let Telemetry = loadTelemetryAndRecordLogs();
  let { inspector } = yield openInspectorForURL(TEST_URL);

  for (let {desc, id, selector, text} of COPY_ITEMS_TEST_DATA) {
    info("Testing " + desc);
    yield selectNode(selector, inspector);

    let allMenuItems = openContextMenuAndGetAllItems(inspector);
    let item = allMenuItems.find(i => i.id === id);
    ok(item, "The popup has a " + desc + " menu item.");

    yield waitForClipboardPromise(() => item.click(), text);
  }

  checkTelemetryResults(Telemetry);
  stopRecordingTelemetryLogs(Telemetry);
});

function checkTelemetryResults(Telemetry) {
  let data = Telemetry.prototype.telemetryInfo;
  let results = new Map();

  for (let key in data) {
    if (key.toLowerCase() === key) {
      let pings = data[key].length;

      results.set(key, pings);
    }
  }

  is(results.size, 3, "The correct number of scalars were logged");

  let pings = checkPings(SELECTOR_UNIQUE, results);
  is(pings, 1, `${SELECTOR_UNIQUE} has just 1 ping`);

  pings = checkPings(SELECTOR_FULL, results);
  is(pings, 1, `${SELECTOR_FULL} has just 1 ping`);

  pings = checkPings(XPATH, results);
  is(pings, 1, `${XPATH} has just 1 ping`);
}

function checkPings(scalarId, results) {
  return results.get(scalarId);
}
