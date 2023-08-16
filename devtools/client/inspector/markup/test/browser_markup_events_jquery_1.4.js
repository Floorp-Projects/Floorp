/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.4).

const TEST_LIB = "lib_jquery_1.4_min.js";
const TEST_URL = URL_ROOT_SSL + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "DOMContentLoaded",
        filename: URL_ROOT_SSL + TEST_LIB + ":32:355",
        attributes: ["Bubbling"],
        handler: `
          function() {
            s.removeEventListener(\"DOMContentLoaded\", M, false);
            c.ready()
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
        filename: URL_ROOT_SSL + TEST_LIB + ":26:107",
        attributes: ["Bubbling"],
        handler: `
          function() {
            if (!c.isReady) {
              if (!s.body) return setTimeout(c.ready, 13);
              c.isReady = true;
              if (Q) {
                for (var a, b = 0; a = Q[b++];) a.call(s, c);
                Q = null
              }
              c.fn.triggerHandler && c(s).triggerHandler("ready")
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
          function() {
            return a.apply(d || this, arguments)
          }`,
      },
      {
        type: "dblclick",
        filename: URL_ROOT_SSL + TEST_LIB + ":17:183",
        attributes: ["jQuery", "Live"],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`,
      },
      {
        type: "dragstart",
        filename: TEST_URL + ":31:50",
        attributes: ["jQuery", "Live"],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`,
      },
      {
        type: "dragstart",
        filename: URL_ROOT_SSL + TEST_LIB + ":17:183",
        attributes: ["jQuery", "Live"],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`,
      },
    ],
  },
];
/* eslint-enable */

add_task(async function () {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
