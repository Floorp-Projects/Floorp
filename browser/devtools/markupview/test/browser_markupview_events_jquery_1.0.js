/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.0).

const TEST_LIB = "lib_jquery_1.0.js";
const TEST_URL = TEST_URL_ROOT + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "load",
        filename: TEST_URL_ROOT + TEST_LIB,
        attributes: [
          "jQuery"
        ],
        handler: "// Handle when the DOM is ready\n" +
                 "ready: function() {\n" +
                 "  // Make sure that the DOM is not already loaded\n" +
                 "  if (!jQuery.isReady) {\n" +
                 "    // Remember that the DOM is ready\n" +
                 "    jQuery.isReady = true;\n" +
                 "\n" +
                 "    // If there are functions bound, to execute\n" +
                 "    if (jQuery.readyList) {\n" +
                 "      // Execute all of them\n" +
                 "      for (var i = 0; i < jQuery.readyList.length; i++)\n" +
                 "        jQuery.readyList[i].apply(document);\n" +
                 "\n" +
                 "      // Reset the list of functions\n" +
                 "      jQuery.readyList = null;\n" +
                 "    }\n" +
                 "  }\n" +
                 "}"
      },
      {
        type: "load",
        filename: TEST_URL,
        attributes: [
          "Bubbling",
          "DOM0"
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
        filename: TEST_URL_ROOT + TEST_LIB,
        attributes: [
          "Bubbling",
          "DOM0"
        ],
        handler: "handle: function(event) {\n" +
                 "  if (typeof jQuery == \"undefined\") return;\n" +
                 "\n" +
                 "  event = event || jQuery.event.fix(window.event);\n" +
                 "\n" +
                 "  // If no correct event was found, fail\n" +
                 "  if (!event) return;\n" +
                 "\n" +
                 "  var returnValue = true;\n" +
                 "\n" +
                 "  var c = this.events[event.type];\n" +
                 "\n" +
                 "  for (var j in c) {\n" +
                 "    if (c[j].apply(this, [event]) === false) {\n" +
                 "      event.preventDefault();\n" +
                 "      event.stopPropagation();\n" +
                 "      returnValue = false;\n" +
                 "    }\n" +
                 "  }\n" +
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
        type: "click",
        filename: TEST_URL_ROOT + TEST_LIB + ":894",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "handle: function(event) {\n" +
                 "  if (typeof jQuery == \"undefined\") return;\n" +
                 "\n" +
                 "  event = event || jQuery.event.fix(window.event);\n" +
                 "\n" +
                 "  // If no correct event was found, fail\n" +
                 "  if (!event) return;\n" +
                 "\n" +
                 "  var returnValue = true;\n" +
                 "\n" +
                 "  var c = this.events[event.type];\n" +
                 "\n" +
                 "  for (var j in c) {\n" +
                 "    if (c[j].apply(this, [event]) === false) {\n" +
                 "      event.preventDefault();\n" +
                 "      event.stopPropagation();\n" +
                 "      returnValue = false;\n" +
                 "    }\n" +
                 "  }\n" +
                 "\n" +
                 "  return returnValue;\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: TEST_URL_ROOT + TEST_LIB + ":894",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "handle: function(event) {\n" +
                 "  if (typeof jQuery == \"undefined\") return;\n" +
                 "\n" +
                 "  event = event || jQuery.event.fix(window.event);\n" +
                 "\n" +
                 "  // If no correct event was found, fail\n" +
                 "  if (!event) return;\n" +
                 "\n" +
                 "  var returnValue = true;\n" +
                 "\n" +
                 "  var c = this.events[event.type];\n" +
                 "\n" +
                 "  for (var j in c) {\n" +
                 "    if (c[j].apply(this, [event]) === false) {\n" +
                 "      event.preventDefault();\n" +
                 "      event.stopPropagation();\n" +
                 "      returnValue = false;\n" +
                 "    }\n" +
                 "  }\n" +
                 "\n" +
                 "  return returnValue;\n" +
                 "}"
      }
    ]
  },
];

let test = asyncTest(runEventPopupTests);
