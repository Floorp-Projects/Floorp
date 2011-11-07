/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource:///modules/source-editor.jsm");

let testWin;
let editor;

function test()
{
  waitForExplicitFinish();

  const pageUrl = "data:text/html,<ul><li>test<li>foobarBug684862";

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    waitForFocus(pageLoaded, content);
  }, true);

  content.location = pageUrl;
}

function pageLoaded()
{
  const windowUrl = "data:application/vnd.mozilla.xul+xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 684862 - paste HTML' width='600' height='500'>" +
    "<script type='application/javascript' src='chrome://global/content/globalOverlay.js'/>" +
    "<box flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  let doCopy = function() {
    content.focus();
    EventUtils.synthesizeKey("a", {accelKey: true}, content);
    EventUtils.synthesizeKey("c", {accelKey: true}, content);
  };

  let clipboardValidator = function(aData) aData.indexOf("foobarBug684862") > -1;

  let onCopy = function() {
    testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
    testWin.addEventListener("load", function onWindowLoad() {
      testWin.removeEventListener("load", onWindowLoad, false);
      waitForFocus(initEditor, testWin);
    }, false);
  };

  waitForClipboard(clipboardValidator, doCopy, onCopy, testEnd);
}

function initEditor()
{
  let box = testWin.document.querySelector("box");

  editor = new SourceEditor();
  editor.init(box, {}, editorLoaded);
}

function editorLoaded()
{
  editor.focus();

  ok(!editor.getText(), "editor has no content");
  is(editor.getCaretOffset(), 0, "caret location");

  let onPaste = function() {
    editor.removeEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste);

    let text = editor.getText();
    ok(text, "editor has content after paste");

    let pos = text.indexOf("foobarBug684862");
    isnot(pos, -1, "editor content is correct");
    // Test for bug 699541 - Pasted HTML shows twice in Orion.
    is(text.lastIndexOf("foobarBug684862"), pos, "editor content is correct (no duplicate)");

    executeSoon(function() {
      editor.setCaretOffset(4);
      EventUtils.synthesizeKey("a", {}, testWin);
      EventUtils.synthesizeKey("VK_RIGHT", {}, testWin);

      text = editor.getText();

      is(text.indexOf("foobarBug684862"), pos + 1,
            "editor content is correct after navigation");
      is(editor.getCaretOffset(), 6, "caret location");

      executeSoon(testEnd);
    });
  };

  editor.addEventListener(SourceEditor.EVENTS.TEXT_CHANGED, onPaste);

  // Do paste
  executeSoon(function() {
    testWin.goDoCommand("cmd_paste");
  });
}

function testEnd()
{
  if (editor) {
    editor.destroy();
  }
  if (testWin) {
    testWin.close();
  }
  testWin = editor = null;
  gBrowser.removeCurrentTab();

  waitForFocus(finish, window);
}

