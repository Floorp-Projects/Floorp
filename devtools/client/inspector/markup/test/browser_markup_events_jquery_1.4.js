/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.4).

const TEST_LIB = "lib_jquery_1.4_min.js";
const TEST_URL = URL_ROOT + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "DOMContentLoaded",
        filename: URL_ROOT + TEST_LIB + ":32",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: `
          function() {
            s.removeEventListener(\"DOMContentLoaded\", M, false);
            c.ready()
          }`
      },
      {
        type: "load",
        filename: TEST_URL + ":27",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: `
          () => {
            var handler1 = function liveDivDblClick() {
              alert(1);
            };
            var handler2 = function liveDivDragStart() {
              alert(2);
            };
            var handler3 = function liveDivDragLeave() {
              alert(3);
            };
            var handler4 = function liveDivDragEnd() {
              alert(4);
            };
            var handler5 = function liveDivDrop() {
              alert(5);
            };
            var handler6 = function liveDivDragOver() {
              alert(6);
            };
            var handler7 = function divClick1() {
              alert(7);
            };
            var handler8 = function divClick2() {
              alert(8);
            };
            var handler9 = function divKeyDown() {
              alert(9);
            };
            var handler10 = function divDragOut() {
              alert(10);
            };

            if ($("#livediv").live) {
              $("#livediv").live("dblclick", handler1);
              $("#livediv").live("dragstart", handler2);
            }

            if ($("#livediv").delegate) {
              $(document).delegate("#livediv", "dragleave", handler3);
              $(document).delegate("#livediv", "dragend", handler4);
            }

            if ($("#livediv").on) {
              $(document).on("drop", "#livediv", handler5);
              $(document).on("dragover", "#livediv", handler6);
              $(document).on("dragout", "#livediv:xxxxx", handler10);
            }

            var div = $("div")[0];
            $(div).click(handler7);
            $(div).click(handler8);
            $(div).keydown(handler9);
          }`
      },
      {
        type: "load",
        filename: URL_ROOT + TEST_LIB + ":26",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
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
          }`
      }
    ]
  },
  {
    selector: "#testdiv",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":34",
        attributes: [
          "jQuery"
        ],
        handler: `
          function divClick1() {
            alert(7);
          }`
      },
      {
        type: "click",
        filename: TEST_URL + ":35",
        attributes: [
          "jQuery"
        ],
        handler: `
          function divClick2() {
            alert(8);
          }`
      },
      {
        type: "keydown",
        filename: TEST_URL + ":36",
        attributes: [
          "jQuery"
        ],
        handler: `
          function divKeyDown() {
            alert(9);
          }`
      }
    ]
  },
  {
    selector: "#livediv",
    expected: [
      {
        type: "dblclick",
        filename: TEST_URL + ":28",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`
      },
      {
        type: "dblclick",
        filename: URL_ROOT + TEST_LIB + ":17",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`
      },
      {
        type: "dragstart",
        filename: TEST_URL + ":29",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`
      },
      {
        type: "dragstart",
        filename: URL_ROOT + TEST_LIB + ":17",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: `
          function() {
            return a.apply(d || this, arguments)
          }`
      }
    ]
  },
];
/* eslint-enable */

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
