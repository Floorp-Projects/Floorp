/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {
    // appendTo
    let src = win.document.querySelector("iframe").getAttribute("src");
    ok(~src.indexOf(".CodeMirror"), "correct iframe is there");

    // getOption/setOption
    ok(ed.getOption("styleActiveLine"), "getOption works");
    ed.setOption("styleActiveLine", false);
    ok(!ed.getOption("styleActiveLine"), "setOption works");

    // Language modes
    is(ed.getMode(), Editor.modes.text, "getMode");
    ed.setMode(Editor.modes.js);
    is(ed.getMode(), Editor.modes.js, "setMode");

    // Content
    is(ed.getText(), "Hello.", "getText");
    ed.setText("Hi.\nHow are you?");
    is(ed.getText(), "Hi.\nHow are you?", "setText");
    is(ed.getText(1), "How are you?", "getText(num)");
    is(ed.getText(5), "", "getText(num) when num is out of scope");

    ed.replaceText("YOU", { line: 1, ch: 8 }, { line: 1, ch: 11 });
    is(ed.getText(1), "How are YOU?", "replaceText(str, from, to)");
    ed.replaceText("you?", { line: 1, ch: 8 });
    is(ed.getText(1), "How are you?", "replaceText(str, from)");
    ed.replaceText("Hello.");
    is(ed.getText(), "Hello.", "replaceText(str)");

    ed.insertText(", sir/madam", { line: 0, ch: 5});
    is(ed.getText(), "Hello, sir/madam.", "insertText");

    // Add-ons
    ed.extend({ whoami: () => "Anton", whereami: () => "Mozilla" });
    is(ed.whoami(), "Anton", "extend/1");
    is(ed.whereami(), "Mozilla", "extend/2");

    // Line classes
    ed.setText("Hello!\nHow are you?");
    ok(!ed.hasLineClass(0, "test"), "no test line class");
    ed.addLineClass(0, "test");
    ok(ed.hasLineClass(0, "test"), "test line class is there");
    ed.removeLineClass(0, "test");
    ok(!ed.hasLineClass(0, "test"), "test line class is gone");

    // Font size
    let size = ed.getFontSize();
    is("number", typeof size, "we have the default font size");
    ed.setFontSize(ed.getFontSize() + 1);
    is(ed.getFontSize(), size + 1, "new font size was set");

    teardown(ed, win);
  });
}
