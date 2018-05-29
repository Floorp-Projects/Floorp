/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

requestLongerTimeout(2);

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.7).

const TEST_LIB = "lib_jquery_1.7_min.js";
const TEST_URL = URL_ROOT + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "DOMContentLoaded",
        filename: URL_ROOT + TEST_LIB + ":2",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {\n" +
                 "  c.removeEventListener(\"DOMContentLoaded\", C, !1), e.ready()\n" +
                 "}"
      },
      {
        type: "load",
        filename: TEST_URL + ":27",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "() => {\n" +
                 "  var handler1 = function liveDivDblClick() {\n" +
                 "    alert(1);\n" +
                 "  };\n" +
                 "  var handler2 = function liveDivDragStart() {\n" +
                 "    alert(2);\n" +
                 "  };\n" +
                 "  var handler3 = function liveDivDragLeave() {\n" +
                 "    alert(3);\n" +
                 "  };\n" +
                 "  var handler4 = function liveDivDragEnd() {\n" +
                 "    alert(4);\n" +
                 "  };\n" +
                 "  var handler5 = function liveDivDrop() {\n" +
                 "    alert(5);\n" +
                 "  };\n" +
                 "  var handler6 = function liveDivDragOver() {\n" +
                 "    alert(6);\n" +
                 "  };\n" +
                 "  var handler7 = function divClick1() {\n" +
                 "    alert(7);\n" +
                 "  };\n" +
                 "  var handler8 = function divClick2() {\n" +
                 "    alert(8);\n" +
                 "  };\n" +
                 "  var handler9 = function divKeyDown() {\n" +
                 "    alert(9);\n" +
                 "  };\n" +
                 "  var handler10 = function divDragOut() {\n" +
                 "    alert(10);\n" +
                 "  };\n" +
                 "\n" +
                 "  if ($(\"#livediv\").live) {\n" +
                 "    $(\"#livediv\").live(\"dblclick\", handler1);\n" +
                 "    $(\"#livediv\").live(\"dragstart\", handler2);\n" +
                 "  }\n" +
                 "\n" +
                 "  if ($(\"#livediv\").delegate) {\n" +
                 "    $(document).delegate(\"#livediv\", \"dragleave\", handler3);\n" +
                 "    $(document).delegate(\"#livediv\", \"dragend\", handler4);\n" +
                 "  }\n" +
                 "\n" +
                 "  if ($(\"#livediv\").on) {\n" +
                 "    $(document).on(\"drop\", \"#livediv\", handler5);\n" +
                 "    $(document).on(\"dragover\", \"#livediv\", handler6);\n" +
                 "    $(document).on(\"dragout\", \"#livediv:xxxxx\", handler10);\n" +
                 "  }\n" +
                 "\n" +
                 "  var div = $(\"div\")[0];\n" +
                 "  $(div).click(handler7);\n" +
                 "  $(div).click(handler8);\n" +
                 "  $(div).keydown(handler9);\n" +
                 "}"
      },
      {
        type: "load",
        filename: URL_ROOT + TEST_LIB + ":2",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function(a) {\n" +
                 "  if (a === !0 && !--e.readyWait || a !== !0 && !e.isReady) {\n" +
                 "    if (!c.body) return setTimeout(e.ready, 1);\n" +
                 "    e.isReady = !0;\n" +
                 "    if (a !== !0 && --e.readyWait > 0) return;\n" +
                 "    B.fireWith(c, [e]), e.fn.trigger && e(c).trigger(\"ready\").unbind(\"ready\")\n" +
                 "  }\n" +
                 "}"
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
        handler: "function divClick1() {\n" +
                 "  alert(7);\n" +
                 "}"
      },
      {
        type: "click",
        filename: TEST_URL + ":35",
        attributes: [
          "jQuery"
        ],
        handler: "function divClick2() {\n" +
                 "  alert(8);\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: TEST_URL + ":36",
        attributes: [
          "jQuery"
        ],
        handler: "function divKeyDown() {\n" +
                 "  alert(9);\n" +
                 "}"
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
        handler: "function liveDivDblClick() {\n" +
                 "  alert(1);\n" +
                 "}"
      },
      {
        type: "dragend",
        filename: TEST_URL + ":31",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function liveDivDragEnd() {\n" +
                 "  alert(4);\n" +
                 "}"
      },
      {
        type: "dragleave",
        filename: TEST_URL + ":30",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function liveDivDragLeave() {\n" +
                 "  alert(3);\n" +
                 "}"
      },
      {
        type: "dragover",
        filename: TEST_URL + ":33",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function liveDivDragOver() {\n" +
                 "  alert(6);\n" +
                 "}"
      },
      {
        type: "dragstart",
        filename: TEST_URL + ":29",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function liveDivDragStart() {\n" +
                 "  alert(2);\n" +
                 "}"
      },
      {
        type: "drop",
        filename: TEST_URL + ":32",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function liveDivDrop() {\n" +
                 "  alert(5);\n" +
                 "}"
      }
    ]
  },
];
/* eslint-enable */

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
