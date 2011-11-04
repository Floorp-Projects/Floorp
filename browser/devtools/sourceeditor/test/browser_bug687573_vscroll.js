/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/source-editor.jsm");

let testWin;
let editor;

function test()
{
  let component = Services.prefs.getCharPref(SourceEditor.PREFS.COMPONENT);
  if (component == "textarea") {
    ok(true, "skip test for bug 687573: not applicable for TEXTAREAs");
    return; // TEXTAREAs have different behavior
  }

  waitForExplicitFinish();

  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 687573 - vertical scroll' width='300' height='500'>" +
    "<box flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);
}

function initEditor()
{
  let box = testWin.document.querySelector("box");

  let text = "abba\n" +
             "\n" +
             "abbaabbaabbaabbaabbaabbaabbaabbaabbaabba\n" +
             "abbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabbaabba\n" +
             "abbaabbaabbaabbaabbaabbaabbaabbaabbaabba\n" +
             "\n" +
             "abba\n";

  let config = {
    showLineNumbers: true,
    placeholderText: text,
  };

  editor = new SourceEditor();
  editor.init(box, config, editorLoaded);
}

function editorLoaded()
{
  let VK_LINE_END = "VK_END";
  let VK_LINE_END_OPT = {};
  let OS = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
  if (OS == "Darwin") {
    VK_LINE_END = "VK_RIGHT";
    VK_LINE_END_OPT = {accelKey: true};
  }

  editor.focus();

  editor.setCaretOffset(0);
  is(editor.getCaretOffset(), 0, "caret location at start");

  EventUtils.synthesizeKey("VK_DOWN", {}, testWin);
  EventUtils.synthesizeKey("VK_DOWN", {}, testWin);

  // line 3
  is(editor.getCaretOffset(), 6, "caret location, keypress Down two times, line 3");

  // line 3 end
  EventUtils.synthesizeKey(VK_LINE_END, VK_LINE_END_OPT, testWin);
  is(editor.getCaretOffset(), 46, "caret location, keypress End, line 3 end");

  // line 4
  EventUtils.synthesizeKey("VK_DOWN", {}, testWin);
  is(editor.getCaretOffset(), 87, "caret location, keypress Down, line 4");

  // line 4 end
  EventUtils.synthesizeKey(VK_LINE_END, VK_LINE_END_OPT, testWin);
  is(editor.getCaretOffset(), 135, "caret location, keypress End, line 4 end");

  // line 5 end
  EventUtils.synthesizeKey("VK_DOWN", {}, testWin);
  is(editor.getCaretOffset(), 176, "caret location, keypress Down, line 5 end");

  // line 6 end
  EventUtils.synthesizeKey("VK_DOWN", {}, testWin);
  is(editor.getCaretOffset(), 177, "caret location, keypress Down, line 6 end");

  // The executeSoon() calls are needed to allow reflows...
  EventUtils.synthesizeKey("VK_UP", {}, testWin);
  executeSoon(function() {
    // line 5 end
    is(editor.getCaretOffset(), 176, "caret location, keypress Up, line 5 end");

    EventUtils.synthesizeKey("VK_UP", {}, testWin);
    executeSoon(function() {
      // line 4 end
      is(editor.getCaretOffset(), 135, "caret location, keypress Up, line 4 end");

      // line 3 end
      EventUtils.synthesizeKey("VK_UP", {}, testWin);
      is(editor.getCaretOffset(), 46, "caret location, keypress Up, line 3 end");

      // line 2 end
      EventUtils.synthesizeKey("VK_UP", {}, testWin);
      is(editor.getCaretOffset(), 5, "caret location, keypress Up, line 2 end");

      // line 3 end
      EventUtils.synthesizeKey("VK_DOWN", {}, testWin);
      is(editor.getCaretOffset(), 46, "caret location, keypress Down, line 3 end");

      // line 4 end
      EventUtils.synthesizeKey("VK_DOWN", {}, testWin);
      is(editor.getCaretOffset(), 135, "caret location, keypress Down, line 4 end");

      editor.destroy();
      testWin.close();
      testWin = editor = null;

      waitForFocus(finish, window);
    });
  });
}
