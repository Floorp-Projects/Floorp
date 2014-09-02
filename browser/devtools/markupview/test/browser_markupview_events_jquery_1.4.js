/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.4).

const TEST_LIB = "lib_jquery_1.4_min.js";
const TEST_URL = TEST_URL_ROOT + "doc_markup_events_jquery.html?" + TEST_LIB;

loadHelperScript("helper_events_test_runner.js");

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
        filename: TEST_URL_ROOT + TEST_LIB + ":26",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "ready: function() {\n" +
                 "  if (!c.isReady) {\n" +
                 "    if (!s.body) return setTimeout(c.ready, 13);\n" +
                 "    c.isReady = true;\n" +
                 "    if (Q) {\n" +
                 "      for (var a, b = 0; a = Q[b++];) a.call(s, c);\n" +
                 "      Q = null\n" +
                 "    }\n" +
                 "    c.fn.triggerHandler && c(s).triggerHandler(\"ready\")\n" +
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
        filename: TEST_URL_ROOT + TEST_LIB + ":48",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "j = function() {\n" +
                 "  return typeof c !== \"undefined\" && !c.event.triggered ? c.event.handle.apply(j.elem, arguments) : w\n" +
                 "}"
      },
      {
        type: "keydown",
        filename: TEST_URL_ROOT + TEST_LIB + ":48",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "j = function() {\n" +
                 "  return typeof c !== \"undefined\" && !c.event.triggered ? c.event.handle.apply(j.elem, arguments) : w\n" +
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
        handler: "var handler1 = function liveDivDblClick() {\n" +
                 "  alert(1);\n" +
                 "}"
      },
      {
        type: "dragstart",
        filename: TEST_URL + ":29",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "var handler2 = function liveDivDragStart() {\n" +
                 "  alert(2);\n" +
                 "}"
      },
      {
        type: "dblclick",
        filename: TEST_URL_ROOT + TEST_LIB + ":17",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function qa(a) {\n" +
                 "  var b = true,\n" +
                 "    d = [],\n" +
                 "    f = [],\n" +
                 "    e = arguments,\n" +
                 "    i, j, o, p, n, t = c.extend({}, c.data(this, \"events\").live);\n" +
                 "  for (p in t) {\n" +
                 "    j = t[p];\n" +
                 "    if (j.live === a.type || j.altLive && c.inArray(a.type, j.altLive) > -1) {\n" +
                 "      i = j.data;\n" +
                 "      i.beforeFilter && i.beforeFilter[a.type] && !i.beforeFilter[a.type](a) || f.push(j.selector)\n" +
                 "    } else delete t[p]\n" +
                 "  }\n" +
                 "  i = c(a.target).closest(f, a.currentTarget);\n" +
                 "  n = 0;\n" +
                 "  for (l = i.length; n < l; n++)\n" +
                 "    for (p in t) {\n" +
                 "      j = t[p];\n" +
                 "      o = i[n].elem;\n" +
                 "      f = null;\n" +
                 "      if (i[n].selector === j.selector) {\n" +
                 "        if (j.live === \"mouseenter\" || j.live === \"mouseleave\") f = c(a.relatedTarget).closest(j.selector)[0];\n" +
                 "        if (!f || f !== o) d.push({\n" +
                 "          elem: o,\n" +
                 "          fn: j\n" +
                 "        })\n" +
                 "      }\n" +
                 "    }\n" +
                 "  n = 0;\n" +
                 "  for (l = d.length; n < l; n++) {\n" +
                 "    i = d[n];\n" +
                 "    a.currentTarget = i.elem;\n" +
                 "    a.data = i.fn.data;\n" +
                 "    if (i.fn.apply(i.elem, e) === false) {\n" +
                 "      b = false;\n" +
                 "      break\n" +
                 "    }\n" +
                 "  }\n" +
                 "  return b\n" +
                 "}"
      },
      {
        type: "dragstart",
        filename: TEST_URL_ROOT + TEST_LIB + ":17",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function qa(a) {\n" +
                 "  var b = true,\n" +
                 "    d = [],\n" +
                 "    f = [],\n" +
                 "    e = arguments,\n" +
                 "    i, j, o, p, n, t = c.extend({}, c.data(this, \"events\").live);\n" +
                 "  for (p in t) {\n" +
                 "    j = t[p];\n" +
                 "    if (j.live === a.type || j.altLive && c.inArray(a.type, j.altLive) > -1) {\n" +
                 "      i = j.data;\n" +
                 "      i.beforeFilter && i.beforeFilter[a.type] && !i.beforeFilter[a.type](a) || f.push(j.selector)\n" +
                 "    } else delete t[p]\n" +
                 "  }\n" +
                 "  i = c(a.target).closest(f, a.currentTarget);\n" +
                 "  n = 0;\n" +
                 "  for (l = i.length; n < l; n++)\n" +
                 "    for (p in t) {\n" +
                 "      j = t[p];\n" +
                 "      o = i[n].elem;\n" +
                 "      f = null;\n" +
                 "      if (i[n].selector === j.selector) {\n" +
                 "        if (j.live === \"mouseenter\" || j.live === \"mouseleave\") f = c(a.relatedTarget).closest(j.selector)[0];\n" +
                 "        if (!f || f !== o) d.push({\n" +
                 "          elem: o,\n" +
                 "          fn: j\n" +
                 "        })\n" +
                 "      }\n" +
                 "    }\n" +
                 "  n = 0;\n" +
                 "  for (l = d.length; n < l; n++) {\n" +
                 "    i = d[n];\n" +
                 "    a.currentTarget = i.elem;\n" +
                 "    a.data = i.fn.data;\n" +
                 "    if (i.fn.apply(i.elem, e) === false) {\n" +
                 "      b = false;\n" +
                 "      break\n" +
                 "    }\n" +
                 "  }\n" +
                 "  return b\n" +
                 "}"
      }
    ]
  },
];

let test = asyncTest(runEventPopupTests);
