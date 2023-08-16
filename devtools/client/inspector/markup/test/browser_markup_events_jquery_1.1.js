/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.1).

const TEST_LIB = "lib_jquery_1.1.js";
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
      {
        type: "load",
        filename: URL_ROOT_SSL + TEST_LIB + ":1224:17",
        attributes: ["Bubbling"],
        handler: `
          function(event) {
            if (typeof jQuery == "undefined") return false;

            // Empty object is for triggered events with no data
            event = jQuery.event.fix(event || window.event || {});

            // returned undefined or false
            var returnValue;

            var c = this.events[event.type];

            var args = [].slice.call(arguments, 1);
            args.unshift(event);

            for (var j in c) {
              // Pass in a reference to the handler function itself
              // So that we can later remove it
              args[0].handler = c[j];
              args[0].data = c[j].data;

              if (c[j].apply(this, args) === false) {
                event.preventDefault();
                event.stopPropagation();
                returnValue = false;
              }
            }

            // Clean up added properties in IE to prevent memory leak
            if (jQuery.browser.msie) event.target = event.preventDefault = event.stopPropagation = event.handler = event.data = null;

            return returnValue;
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
        type: "click",
        filename: URL_ROOT_SSL + TEST_LIB + ":1224:17",
        attributes: ["Bubbling"],
        handler: `
          function(event) {
            if (typeof jQuery == "undefined") return false;

            // Empty object is for triggered events with no data
            event = jQuery.event.fix(event || window.event || {});

            // returned undefined or false
            var returnValue;

            var c = this.events[event.type];

            var args = [].slice.call(arguments, 1);
            args.unshift(event);

            for (var j in c) {
              // Pass in a reference to the handler function itself
              // So that we can later remove it
              args[0].handler = c[j];
              args[0].data = c[j].data;

              if (c[j].apply(this, args) === false) {
                event.preventDefault();
                event.stopPropagation();
                returnValue = false;
              }
            }

            // Clean up added properties in IE to prevent memory leak
            if (jQuery.browser.msie) event.target = event.preventDefault = event.stopPropagation = event.handler = event.data = null;

            return returnValue;
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
        filename: URL_ROOT_SSL + TEST_LIB + ":1224:17",
        attributes: ["Bubbling"],
        handler: `
          function(event) {
            if (typeof jQuery == "undefined") return false;

            // Empty object is for triggered events with no data
            event = jQuery.event.fix(event || window.event || {});

            // returned undefined or false
            var returnValue;

            var c = this.events[event.type];

            var args = [].slice.call(arguments, 1);
            args.unshift(event);

            for (var j in c) {
              // Pass in a reference to the handler function itself
              // So that we can later remove it
              args[0].handler = c[j];
              args[0].data = c[j].data;

              if (c[j].apply(this, args) === false) {
                event.preventDefault();
                event.stopPropagation();
                returnValue = false;
              }
            }

            // Clean up added properties in IE to prevent memory leak
            if (jQuery.browser.msie) event.target = event.preventDefault = event.stopPropagation = event.handler = event.data = null;

            return returnValue;
          }`,
      },
    ],
  },
];
/* eslint-enable */

add_task(async function () {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
