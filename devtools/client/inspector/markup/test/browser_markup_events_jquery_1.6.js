/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

requestLongerTimeout(2);

// Test that markup view event bubbles show the correct event info for jQuery
// and jQuery Live events (jQuery version 1.6).

const TEST_LIB = "lib_jquery_1.6_min.js";
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
        filename: URL_ROOT + TEST_LIB + ":16",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function(a) {\n" +
                 "  if (a === !0 && !--e.readyWait || a !== !0 && !e.isReady) {\n" +
                 "    if (!c.body) return setTimeout(e.ready, 1);\n" +
                 "    e.isReady = !0;\n" +
                 "    if (a !== !0 && --e.readyWait > 0) return;\n" +
                 "    y.resolveWith(c, [e]), e.fn.trigger && e(c).trigger(\"ready\").unbind(\"ready\")\n" +
                 "  }\n" +
                 "}"
      },
      {
        type: "DOMContentLoaded",
        filename: URL_ROOT + TEST_LIB + ":16",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {\n" +
                 "  c.removeEventListener(\"DOMContentLoaded\", z, !1), e.ready()\n" +
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
        type: "dblclick",
        filename: URL_ROOT + TEST_LIB + ":16",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function M(a) {\n" +
                 "  var b, c, d, e, g, h, i, j, k, l, m, n, o, p = [],\n" +
                 "    q = [],\n" +
                 "    r = f._data(this, \"events\");\n" +
                 "  if (!(a.liveFired === this || !r || !r.live || a.target.disabled || a.button && a.type === \"click\")) {\n" +
                 "    a.namespace && (n = new RegExp(\"(^|\\\\.)\" + a.namespace.split(\".\").join(\"\\\\.(?:.*\\\\.)?\") + \"(\\\\.|$)\")), a.liveFired = this;\n" +
                 "    var s = r.live.slice(0);\n" +
                 "    for (i = 0; i < s.length; i++) g = s[i], g.origType.replace(x, \"\") === a.type ? q.push(g.selector) : s.splice(i--, 1);\n" +
                 "    e = f(a.target).closest(q, a.currentTarget);\n" +
                 "    for (j = 0, k = e.length; j < k; j++) {\n" +
                 "      m = e[j];\n" +
                 "      for (i = 0; i < s.length; i++) {\n" +
                 "        g = s[i];\n" +
                 "        if (m.selector === g.selector && (!n || n.test(g.namespace)) && !m.elem.disabled) {\n" +
                 "          h = m.elem, d = null;\n" +
                 "          if (g.preType === \"mouseenter\" || g.preType === \"mouseleave\") a.type = g.preType, d = f(a.relatedTarget).closest(g.selector)[0], d && f.contains(h, d) && (d = h);\n" +
                 "          (!d || d !== h) && p.push({\n" +
                 "            elem: h,\n" +
                 "            handleObj: g,\n" +
                 "            level: m.level\n" +
                 "          })\n" +
                 "        }\n" +
                 "      }\n" +
                 "    }\n" +
                 "    for (j = 0, k = p.length; j < k; j++) {\n" +
                 "      e = p[j];\n" +
                 "      if (c && e.level > c) break;\n" +
                 "      a.currentTarget = e.elem, a.data = e.handleObj.data, a.handleObj = e.handleObj, o = e.handleObj.origHandler.apply(e.elem, arguments);\n" +
                 "      if (o === !1 || a.isPropagationStopped()) {\n" +
                 "        c = e.level, o === !1 && (b = !1);\n" +
                 "        if (a.isImmediatePropagationStopped()) break\n" +
                 "      }\n" +
                 "    }\n" +
                 "    return b\n" +
                 "  }\n" +
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
        type: "dragend",
        filename: URL_ROOT + TEST_LIB + ":16",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function M(a) {\n" +
                 "  var b, c, d, e, g, h, i, j, k, l, m, n, o, p = [],\n" +
                 "    q = [],\n" +
                 "    r = f._data(this, \"events\");\n" +
                 "  if (!(a.liveFired === this || !r || !r.live || a.target.disabled || a.button && a.type === \"click\")) {\n" +
                 "    a.namespace && (n = new RegExp(\"(^|\\\\.)\" + a.namespace.split(\".\").join(\"\\\\.(?:.*\\\\.)?\") + \"(\\\\.|$)\")), a.liveFired = this;\n" +
                 "    var s = r.live.slice(0);\n" +
                 "    for (i = 0; i < s.length; i++) g = s[i], g.origType.replace(x, \"\") === a.type ? q.push(g.selector) : s.splice(i--, 1);\n" +
                 "    e = f(a.target).closest(q, a.currentTarget);\n" +
                 "    for (j = 0, k = e.length; j < k; j++) {\n" +
                 "      m = e[j];\n" +
                 "      for (i = 0; i < s.length; i++) {\n" +
                 "        g = s[i];\n" +
                 "        if (m.selector === g.selector && (!n || n.test(g.namespace)) && !m.elem.disabled) {\n" +
                 "          h = m.elem, d = null;\n" +
                 "          if (g.preType === \"mouseenter\" || g.preType === \"mouseleave\") a.type = g.preType, d = f(a.relatedTarget).closest(g.selector)[0], d && f.contains(h, d) && (d = h);\n" +
                 "          (!d || d !== h) && p.push({\n" +
                 "            elem: h,\n" +
                 "            handleObj: g,\n" +
                 "            level: m.level\n" +
                 "          })\n" +
                 "        }\n" +
                 "      }\n" +
                 "    }\n" +
                 "    for (j = 0, k = p.length; j < k; j++) {\n" +
                 "      e = p[j];\n" +
                 "      if (c && e.level > c) break;\n" +
                 "      a.currentTarget = e.elem, a.data = e.handleObj.data, a.handleObj = e.handleObj, o = e.handleObj.origHandler.apply(e.elem, arguments);\n" +
                 "      if (o === !1 || a.isPropagationStopped()) {\n" +
                 "        c = e.level, o === !1 && (b = !1);\n" +
                 "        if (a.isImmediatePropagationStopped()) break\n" +
                 "      }\n" +
                 "    }\n" +
                 "    return b\n" +
                 "  }\n" +
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
        type: "dragleave",
        filename: URL_ROOT + TEST_LIB + ":16",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function M(a) {\n" +
                 "  var b, c, d, e, g, h, i, j, k, l, m, n, o, p = [],\n" +
                 "    q = [],\n" +
                 "    r = f._data(this, \"events\");\n" +
                 "  if (!(a.liveFired === this || !r || !r.live || a.target.disabled || a.button && a.type === \"click\")) {\n" +
                 "    a.namespace && (n = new RegExp(\"(^|\\\\.)\" + a.namespace.split(\".\").join(\"\\\\.(?:.*\\\\.)?\") + \"(\\\\.|$)\")), a.liveFired = this;\n" +
                 "    var s = r.live.slice(0);\n" +
                 "    for (i = 0; i < s.length; i++) g = s[i], g.origType.replace(x, \"\") === a.type ? q.push(g.selector) : s.splice(i--, 1);\n" +
                 "    e = f(a.target).closest(q, a.currentTarget);\n" +
                 "    for (j = 0, k = e.length; j < k; j++) {\n" +
                 "      m = e[j];\n" +
                 "      for (i = 0; i < s.length; i++) {\n" +
                 "        g = s[i];\n" +
                 "        if (m.selector === g.selector && (!n || n.test(g.namespace)) && !m.elem.disabled) {\n" +
                 "          h = m.elem, d = null;\n" +
                 "          if (g.preType === \"mouseenter\" || g.preType === \"mouseleave\") a.type = g.preType, d = f(a.relatedTarget).closest(g.selector)[0], d && f.contains(h, d) && (d = h);\n" +
                 "          (!d || d !== h) && p.push({\n" +
                 "            elem: h,\n" +
                 "            handleObj: g,\n" +
                 "            level: m.level\n" +
                 "          })\n" +
                 "        }\n" +
                 "      }\n" +
                 "    }\n" +
                 "    for (j = 0, k = p.length; j < k; j++) {\n" +
                 "      e = p[j];\n" +
                 "      if (c && e.level > c) break;\n" +
                 "      a.currentTarget = e.elem, a.data = e.handleObj.data, a.handleObj = e.handleObj, o = e.handleObj.origHandler.apply(e.elem, arguments);\n" +
                 "      if (o === !1 || a.isPropagationStopped()) {\n" +
                 "        c = e.level, o === !1 && (b = !1);\n" +
                 "        if (a.isImmediatePropagationStopped()) break\n" +
                 "      }\n" +
                 "    }\n" +
                 "    return b\n" +
                 "  }\n" +
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
        type: "dragstart",
        filename: URL_ROOT + TEST_LIB + ":16",
        attributes: [
          "jQuery",
          "Live"
        ],
        handler: "function M(a) {\n" +
                 "  var b, c, d, e, g, h, i, j, k, l, m, n, o, p = [],\n" +
                 "    q = [],\n" +
                 "    r = f._data(this, \"events\");\n" +
                 "  if (!(a.liveFired === this || !r || !r.live || a.target.disabled || a.button && a.type === \"click\")) {\n" +
                 "    a.namespace && (n = new RegExp(\"(^|\\\\.)\" + a.namespace.split(\".\").join(\"\\\\.(?:.*\\\\.)?\") + \"(\\\\.|$)\")), a.liveFired = this;\n" +
                 "    var s = r.live.slice(0);\n" +
                 "    for (i = 0; i < s.length; i++) g = s[i], g.origType.replace(x, \"\") === a.type ? q.push(g.selector) : s.splice(i--, 1);\n" +
                 "    e = f(a.target).closest(q, a.currentTarget);\n" +
                 "    for (j = 0, k = e.length; j < k; j++) {\n" +
                 "      m = e[j];\n" +
                 "      for (i = 0; i < s.length; i++) {\n" +
                 "        g = s[i];\n" +
                 "        if (m.selector === g.selector && (!n || n.test(g.namespace)) && !m.elem.disabled) {\n" +
                 "          h = m.elem, d = null;\n" +
                 "          if (g.preType === \"mouseenter\" || g.preType === \"mouseleave\") a.type = g.preType, d = f(a.relatedTarget).closest(g.selector)[0], d && f.contains(h, d) && (d = h);\n" +
                 "          (!d || d !== h) && p.push({\n" +
                 "            elem: h,\n" +
                 "            handleObj: g,\n" +
                 "            level: m.level\n" +
                 "          })\n" +
                 "        }\n" +
                 "      }\n" +
                 "    }\n" +
                 "    for (j = 0, k = p.length; j < k; j++) {\n" +
                 "      e = p[j];\n" +
                 "      if (c && e.level > c) break;\n" +
                 "      a.currentTarget = e.elem, a.data = e.handleObj.data, a.handleObj = e.handleObj, o = e.handleObj.origHandler.apply(e.elem, arguments);\n" +
                 "      if (o === !1 || a.isPropagationStopped()) {\n" +
                 "        c = e.level, o === !1 && (b = !1);\n" +
                 "        if (a.isImmediatePropagationStopped()) break\n" +
                 "      }\n" +
                 "    }\n" +
                 "    return b\n" +
                 "  }\n" +
                 "}"
      }
    ]
  },
];
/*eslint-enable */

add_task(function* () {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
