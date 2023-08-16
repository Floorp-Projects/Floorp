/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

requestLongerTimeout(2);

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.7).

const TEST_LIB = "lib_jquery_1.7_min.js";
const TEST_URL = URL_ROOT_SSL + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "DOMContentLoaded",
        filename: URL_ROOT_SSL + TEST_LIB + ":2:14177",
        attributes: ["Bubbling"],
        handler: `
          function() {
            c.removeEventListener("DOMContentLoaded", C, !1), e.ready()
          }`,
      },
      {
        type: "load",
        filename: TEST_URL + ":29:38",
        attributes: ["Bubbling"],
        handler: getDocMarkupEventsJQueryLoadHandlerText(),
      },
      {
        type: "load",
        filename: URL_ROOT_SSL + TEST_LIB + ":2:9526",
        attributes: ["Bubbling"],
        handler: `
          function(a) {
            if (a === !0 && !--e.readyWait || a !== !0 && !e.isReady) {
              if (!c.body) return setTimeout(e.ready, 1);
              e.isReady = !0;
              if (a !== !0 && --e.readyWait > 0) return;
              B.fireWith(c, [e]), e.fn.trigger && e(c).trigger("ready").unbind("ready")
            }
          }`,
      },
    ],
  },
  {
    selector: "#testdiv",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":36:43",
        attributes: ["jQuery"],
        handler: `
          function divClick1() {
            alert(7);
          }`,
      },
      {
        type: "click",
        filename: TEST_URL + ":37:43",
        attributes: ["jQuery"],
        handler: `
          function divClick2() {
            alert(8);
          }`,
      },
      {
        type: "keydown",
        filename: TEST_URL + ":38:44",
        attributes: ["jQuery"],
        handler: `
          function divKeyDown() {
            alert(9);
          }`,
      },
    ],
  },
  {
    selector: "#livediv",
    expected: [
      {
        type: "dblclick",
        filename: TEST_URL + ":30:49",
        attributes: ["jQuery", "Live"],
        handler: `
          function liveDivDblClick() {
            alert(1);
          }`,
      },
      {
        type: "dragend",
        filename: TEST_URL + ":33:48",
        attributes: ["jQuery", "Live"],
        handler: `
          function liveDivDragEnd() {
            alert(4);
          }`,
      },
      {
        type: "dragleave",
        filename: TEST_URL + ":32:50",
        attributes: ["jQuery", "Live"],
        handler: `
          function liveDivDragLeave() {
            alert(3);
          }`,
      },
      {
        type: "dragover",
        filename: TEST_URL + ":35:49",
        attributes: ["jQuery", "Live"],
        handler: `
          function liveDivDragOver() {
            alert(6);
          }`,
      },
      {
        type: "dragstart",
        filename: TEST_URL + ":31:50",
        attributes: ["jQuery", "Live"],
        handler: `
          function liveDivDragStart() {
            alert(2);
          }`,
      },
      {
        type: "drop",
        filename: TEST_URL + ":34:45",
        attributes: ["jQuery", "Live"],
        handler: `
          function liveDivDrop() {
            alert(5);
          }`,
      },
    ],
  },
];
/* eslint-enable */

add_task(async function () {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
