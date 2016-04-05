/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.2).

const TEST_LIB = "lib_jquery_1.2_min.js";
const TEST_URL = URL_ROOT + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "html",
    expected: [
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
        filename: URL_ROOT + TEST_LIB,
        attributes: [
          "Bubbling",
          "DOM0"
        ],
        handler: "handle: function(event) {\n" +
                 "  if (typeof jQuery == \"undefined\") return false;\n" +
                 "\n" +
                 "  // Empty object is for triggered events with no data\n" +
                 "  event = jQuery.event.fix(event || window.event || {});\n" +
                 "\n" +
                 "  // returned undefined or false\n" +
                 "  var returnValue;\n" +
                 "\n" +
                 "  var c = this.events[event.type];\n" +
                 "\n" +
                 "  var args = [].slice.call(arguments, 1);\n" +
                 "  args.unshift(event);\n" +
                 "\n" +
                 "  for (var j in c) {\n" +
                 "    // Pass in a reference to the handler function itself\n" +
                 "    // So that we can later remove it\n" +
                 "    args[0].handler = c[j];\n" +
                 "    args[0].data = c[j].data;\n" +
                 "\n" +
                 "    if (c[j].apply(this, args) === false) {\n" +
                 "      event.preventDefault();\n" +
                 "      event.stopPropagation();\n" +
                 "      returnValue = false;\n" +
                 "    }\n" +
                 "  }\n" +
                 "\n" +
                 "  // Clean up added properties in IE to prevent memory leak\n" +
                 "  if (jQuery.browser.msie) event.target = event.preventDefault = event.stopPropagation = event.handler = event.data = null;\n" +
                 "\n" +
                 "  return returnValue;\n" +
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
        handler: "var handler7 = function divClick1() {\n" +
                 "  alert(7);\n" +
                 "}"
      },
      {
        type: "click",
        filename: TEST_URL + ":35",
        attributes: [
          "jQuery"
        ],
        handler: "var handler8 = function divClick2() {\n" +
                 "  alert(8);\n" +
                 "}"
      },
      {
        type: "click",
        filename: URL_ROOT + TEST_LIB + ":24",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {\n" +
                 "  var val;\n" +
                 "  if (typeof jQuery == \"undefined\" || jQuery.event.triggered) return val;\n" +
                 "  val = jQuery.event.handle.apply(element, arguments);\n" +
                 "  return val;\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: TEST_URL + ":36",
        attributes: [
          "jQuery"
        ],
        handler: "var handler9 = function divKeyDown() {\n" +
                 "  alert(9);\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: URL_ROOT + TEST_LIB + ":24",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {\n" +
                 "  var val;\n" +
                 "  if (typeof jQuery == \"undefined\" || jQuery.event.triggered) return val;\n" +
                 "  val = jQuery.event.handle.apply(element, arguments);\n" +
                 "  return val;\n" +
                 "}"
      }
    ]
  },
];
/*eslint-enable */

add_task(function* () {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
