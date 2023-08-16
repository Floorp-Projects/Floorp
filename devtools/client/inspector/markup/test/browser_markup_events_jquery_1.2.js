/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.2).

const TEST_LIB = "lib_jquery_1.2_min.js";
const TEST_URL = URL_ROOT_SSL + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "load",
        filename: TEST_URL + ":29:38",
        attributes: ["Bubbling"],
        handler: getDocMarkupEventsJQueryLoadHandlerText(),
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
        type: "click",
        filename: URL_ROOT_SSL + TEST_LIB + ":24:10040",
        attributes: ["Bubbling"],
        handler: `
          function() {
            var val;
            if (typeof jQuery == "undefined" || jQuery.event.triggered) return val;
            val = jQuery.event.handle.apply(element, arguments);
            return val;
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
      {
        type: "keydown",
        filename: URL_ROOT_SSL + TEST_LIB + ":24:10040",
        attributes: ["Bubbling"],
        handler: `
          function() {
            var val;
            if (typeof jQuery == "undefined" || jQuery.event.triggered) return val;
            val = jQuery.event.handle.apply(element, arguments);
            return val;
          }`,
      },
    ],
  },
];
/* eslint-enable */

add_task(async function () {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
