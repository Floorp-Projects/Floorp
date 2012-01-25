/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let tempScope = {};
Cu.import("resource:///modules/source-editor.jsm", tempScope);
let SourceEditor = tempScope.SourceEditor;

let testWin;
let editor;

function test()
{
  waitForExplicitFinish();

  const windowUrl = "data:text/xml,<?xml version='1.0'?>" +
    "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
    " title='test for bug 650345' width='600' height='500'><hbox flex='1'/></window>";
  const windowFeatures = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

  testWin = Services.ww.openWindow(null, windowUrl, "_blank", windowFeatures, null);
  testWin.addEventListener("load", function onWindowLoad() {
    testWin.removeEventListener("load", onWindowLoad, false);
    waitForFocus(initEditor, testWin);
  }, false);
}

function initEditor()
{
  let hbox = testWin.document.querySelector("hbox");
  editor = new SourceEditor();
  editor.init(hbox, {}, editorLoaded);
}

function editorLoaded()
{
  editor.focus();

  let text = "foobar bug650345\nBug650345 bazbaz\nfoobar omg\ntest";
  editor.setText(text);

  let needle = "foobar";
  is(editor.find(), -1, "find() works");
  ok(!editor.lastFind, "no editor.lastFind yet");

  is(editor.find(needle), 0, "find('" + needle + "') works");
  is(editor.lastFind.str, needle, "lastFind.str is correct");
  is(editor.lastFind.index, 0, "lastFind.index is correct");
  is(editor.lastFind.lastFound, 0, "lastFind.lastFound is correct");
  is(editor.lastFind.ignoreCase, false, "lastFind.ignoreCase is correct");

  let newIndex = text.indexOf(needle, needle.length);
  is(editor.findNext(), newIndex, "findNext() works");
  is(editor.lastFind.str, needle, "lastFind.str is correct");
  is(editor.lastFind.index, newIndex, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex, "lastFind.lastFound is correct");
  is(editor.lastFind.ignoreCase, false, "lastFind.ignoreCase is correct");

  is(editor.findNext(), -1, "findNext() works again");
  is(editor.lastFind.index, -1, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex, "lastFind.lastFound is correct");

  is(editor.findPrevious(), 0, "findPrevious() works");
  is(editor.lastFind.index, 0, "lastFind.index is correct");
  is(editor.lastFind.lastFound, 0, "lastFind.lastFound is correct");

  is(editor.findPrevious(), -1, "findPrevious() works again");
  is(editor.lastFind.index, -1, "lastFind.index is correct");
  is(editor.lastFind.lastFound, 0, "lastFind.lastFound is correct");

  is(editor.findNext(), newIndex, "findNext() works");
  is(editor.lastFind.index, newIndex, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex, "lastFind.lastFound is correct");

  is(editor.findNext(true), 0, "findNext(true) works");
  is(editor.lastFind.index, 0, "lastFind.index is correct");
  is(editor.lastFind.lastFound, 0, "lastFind.lastFound is correct");

  is(editor.findNext(true), newIndex, "findNext(true) works again");
  is(editor.lastFind.index, newIndex, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex, "lastFind.lastFound is correct");

  is(editor.findPrevious(true), 0, "findPrevious(true) works");
  is(editor.lastFind.index, 0, "lastFind.index is correct");
  is(editor.lastFind.lastFound, 0, "lastFind.lastFound is correct");

  is(editor.findPrevious(true), newIndex, "findPrevious(true) works again");
  is(editor.lastFind.index, newIndex, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex, "lastFind.lastFound is correct");

  needle = "error";
  is(editor.find(needle), -1, "find('" + needle + "') works");
  is(editor.lastFind.str, needle, "lastFind.str is correct");
  is(editor.lastFind.index, -1, "lastFind.index is correct");
  is(editor.lastFind.lastFound, -1, "lastFind.lastFound is correct");
  is(editor.lastFind.ignoreCase, false, "lastFind.ignoreCase is correct");

  is(editor.findNext(), -1, "findNext() works");
  is(editor.lastFind.str, needle, "lastFind.str is correct");
  is(editor.lastFind.index, -1, "lastFind.index is correct");
  is(editor.lastFind.lastFound, -1, "lastFind.lastFound is correct");
  is(editor.findNext(true), -1, "findNext(true) works");

  is(editor.findPrevious(), -1, "findPrevious() works");
  is(editor.findPrevious(true), -1, "findPrevious(true) works");

  needle = "bug650345";
  newIndex = text.indexOf(needle);

  is(editor.find(needle), newIndex, "find('" + needle + "') works");
  is(editor.findNext(), -1, "findNext() works");
  is(editor.findNext(true), newIndex, "findNext(true) works");
  is(editor.findPrevious(), -1, "findPrevious() works");
  is(editor.findPrevious(true), newIndex, "findPrevious(true) works");
  is(editor.lastFind.index, newIndex, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex, "lastFind.lastFound is correct");

  is(editor.find(needle, {ignoreCase: 1}), newIndex,
     "find('" + needle + "', {ignoreCase: 1}) works");
  is(editor.lastFind.ignoreCase, true, "lastFind.ignoreCase is correct");

  let newIndex2 = text.toLowerCase().indexOf(needle, newIndex + needle.length);
  is(editor.findNext(), newIndex2, "findNext() works");
  is(editor.findNext(), -1, "findNext() works");
  is(editor.lastFind.index, -1, "lastFind.index is correct");
  is(editor.lastFind.lastFound, newIndex2, "lastFind.lastFound is correct");

  is(editor.findNext(true), newIndex, "findNext(true) works");

  is(editor.findPrevious(), -1, "findPrevious() works");
  is(editor.findPrevious(true), newIndex2, "findPrevious(true) works");
  is(editor.findPrevious(), newIndex, "findPrevious() works again");

  needle = "foobar";
  newIndex = text.indexOf(needle, 2);
  is(editor.find(needle, {start: 2}), newIndex,
     "find('" + needle + "', {start:2}) works");
  is(editor.findNext(), -1, "findNext() works");
  is(editor.findNext(true), 0, "findNext(true) works");

  editor.destroy();

  testWin.close();
  testWin = editor = null;

  waitForFocus(finish, window);
}
