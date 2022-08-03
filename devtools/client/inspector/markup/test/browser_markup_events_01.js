/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT_SSL + "doc_markup_events_01.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "load",
        filename: TEST_URL,
        attributes: ["Bubbling"],
        handler: "function onload(event) {\n" + "  init();\n" + "}",
      },
    ],
  },
  {
    selector: "#container",
    expected: [
      {
        type: "mouseover",
        filename: TEST_URL + ":48:31",
        attributes: ["Capturing"],
        handler:
          "function mouseoverHandler(event) {\n" +
          '  if (event.target.id !== "container") {\n' +
          '    const output = document.getElementById("output");\n' +
          "    output.textContent = event.target.textContent;\n" +
          "  }\n" +
          "}",
      },
    ],
  },
  {
    selector: "#multiple",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":55:27",
        attributes: ["Bubbling"],
        handler:
          "function clickHandler(event) {\n" +
          '  const output = document.getElementById("output");\n' +
          '  output.textContent = "click";\n' +
          "}",
      },
      {
        type: "mouseup",
        filename: TEST_URL + ":60:29",
        attributes: ["Bubbling"],
        handler:
          "function mouseupHandler(event) {\n" +
          '  const output = document.getElementById("output");\n' +
          '  output.textContent = "mouseup";\n' +
          "}",
      },
    ],
  },
  // #noevents tests check that dynamically added events are properly displayed
  // in the markupview
  {
    selector: "#noevents",
    expected: [],
  },
  {
    selector: "#noevents",
    async beforeTest(inspector) {
      const nodeMutated = inspector.once("markupmutation");
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
        content.wrappedJSObject.addNoeventsClickHandler()
      );
      await nodeMutated;
    },
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":76:35",
        attributes: ["Bubbling"],
        handler:
          "function noeventsClickHandler(event) {\n" +
          '  alert("noevents has an event listener");\n' +
          "}",
      },
    ],
  },
  {
    selector: "#noevents",
    async beforeTest(inspector) {
      const nodeMutated = inspector.once("markupmutation");
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () =>
        content.wrappedJSObject.removeNoeventsClickHandler()
      );
      await nodeMutated;
    },
    expected: [],
  },
  {
    selector: "#DOM0",
    expected: [
      {
        type: "click",
        filename: TEST_URL,
        attributes: ["Bubbling"],
        handler: "function onclick(event) {\n" + "  alert('DOM0')\n" + "}",
      },
    ],
  },
  {
    selector: "#handleevent",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":71:29",
        attributes: ["Bubbling"],
        handler: "function(blah) {\n" + '  alert("handleEvent");\n' + "}",
      },
    ],
  },
];

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
