/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT + "doc_markup_events.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [ // eslint-disable-line
  {
    selector: "html",
    expected: [
      {
        type: "load",
        filename: TEST_URL,
        attributes: [
          "Bubbling",
          "DOM0"
        ],
        handler: "init();"
      }
    ]
  },
  {
    selector: "#container",
    expected: [
      {
        type: "mouseover",
        filename: TEST_URL + ":62",
        attributes: [
          "Capturing",
          "DOM2"
        ],
        handler: "function mouseoverHandler(event) {\n" +
                 "  if (event.target.id !== \"container\") {\n" +
                 "    let output = document.getElementById(\"output\");\n" +
                 "    output.textContent = event.target.textContent;\n" +
                 "  }\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#multiple",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":69",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function clickHandler(event) {\n" +
                 "  let output = document.getElementById(\"output\");\n" +
                 "  output.textContent = \"click\";\n" +
                 "}"
      },
      {
        type: "mouseup",
        filename: TEST_URL + ":78",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function mouseupHandler(event) {\n" +
                 "  let output = document.getElementById(\"output\");\n" +
                 "  output.textContent = \"mouseup\";\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#DOM0",
    expected: [
      {
        type: "click",
        filename: TEST_URL,
        attributes: [
          "Bubbling",
          "DOM0"
        ],
        handler: "alert('hi')"
      }
    ]
  },
  {
    selector: "#handleevent",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":89",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "handleEvent: function(blah) {\n" +
                 "  alert(\"handleEvent clicked\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#fatarrow",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":57",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "event => {\n" +
                 "  alert(\"Yay for the fat arrow!\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#boundhe",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":101",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "handleEvent: function() {\n" +
                 "  alert(\"boundHandleEvent clicked\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#bound",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":74",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function boundClickHandler(event) {\n" +
                 "  alert(\"Bound event clicked\");\n" +
                 "}"
      }
    ]
  },
  // #noevents tests check that dynamically added events are properly displayed
  // in the markupview
  {
    selector: "#noevents",
    expected: []
  },
  {
    selector: "#noevents",
    beforeTest: function* (inspector, testActor) {
      let nodeMutated = inspector.once("markupmutation");
      yield testActor.eval("window.wrappedJSObject.addNoeventsClickHandler();");
      yield nodeMutated;
    },
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":106",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function noeventsClickHandler(event) {\n" +
                 "  alert(\"noevents has an event listener\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#noevents",
    beforeTest: function* (inspector, testActor) {
      let nodeMutated = inspector.once("markupmutation");
      yield testActor.eval(
        "window.wrappedJSObject.removeNoeventsClickHandler();");
      yield nodeMutated;
    },
    expected: []
  },
];

add_task(function*() {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
