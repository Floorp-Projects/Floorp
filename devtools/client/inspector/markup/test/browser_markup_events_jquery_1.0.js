/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.0).

const TEST_LIB = "lib_jquery_1.0.js";
const TEST_URL = URL_ROOT_SSL + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "DOMContentLoaded",
        filename: URL_ROOT_SSL + TEST_LIB + ":1117:16",
        attributes: ["Bubbling"],
        handler: `
          function() {
            // Make sure that the DOM is not already loaded
            if (!jQuery.isReady) {
              // Remember that the DOM is ready
              jQuery.isReady = true;

              // If there are functions bound, to execute
              if (jQuery.readyList) {
                // Execute all of them
                for (var i = 0; i < jQuery.readyList.length; i++)
                  jQuery.readyList[i].apply(document);

                // Reset the list of functions
                jQuery.readyList = null;
              }
            }
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
        filename: URL_ROOT_SSL + TEST_LIB + ":894:18",
        attributes: ["Bubbling"],
        handler: `
          function(event) {
            if (typeof jQuery == "undefined") return;

            event = event || jQuery.event.fix(window.event);

            // If no correct event was found, fail
            if (!event) return;

            var returnValue = true;

            var c = this.events[event.type];

            for (var j in c) {
              if (c[j].apply(this, [event]) === false) {
                event.preventDefault();
                event.stopPropagation();
                returnValue = false;
              }
            }

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
        filename: URL_ROOT_SSL + TEST_LIB + ":894:18",
        attributes: ["Bubbling"],
        handler: `
          function(event) {
            if (typeof jQuery == "undefined") return;

            event = event || jQuery.event.fix(window.event);

            // If no correct event was found, fail
            if (!event) return;

            var returnValue = true;

            var c = this.events[event.type];

            for (var j in c) {
              if (c[j].apply(this, [event]) === false) {
                event.preventDefault();
                event.stopPropagation();
                returnValue = false;
              }
            }

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
        filename: URL_ROOT_SSL + TEST_LIB + ":894:18",
        attributes: ["Bubbling"],
        handler: `
          function(event) {
            if (typeof jQuery == "undefined") return;

            event = event || jQuery.event.fix(window.event);

            // If no correct event was found, fail
            if (!event) return;

            var returnValue = true;

            var c = this.events[event.type];

            for (var j in c) {
              if (c[j].apply(this, [event]) === false) {
                event.preventDefault();
                event.stopPropagation();
                returnValue = false;
              }
            }

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
