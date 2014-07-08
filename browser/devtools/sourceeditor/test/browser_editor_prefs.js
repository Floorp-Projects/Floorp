/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test to make sure that the editor reacts to preference changes

const TAB_SIZE    = "devtools.editor.tabsize";
const EXPAND_TAB  = "devtools.editor.expandtab";
const KEYMAP      = "devtools.editor.keymap";
const AUTO_CLOSE  = "devtools.editor.autoclosebrackets";
const AUTOCOMPLETE  = "devtools.editor.autocomplete";
const DETECT_INDENT = "devtools.editor.detectindentation";

function test() {
  waitForExplicitFinish();
  setup((ed, win) => {

    ed.setText("Checking preferences.");

    info ("Turning prefs off");

    ed.setOption("autocomplete", true);

    Services.prefs.setIntPref(TAB_SIZE, 2);
    Services.prefs.setBoolPref(EXPAND_TAB, false);
    Services.prefs.setCharPref(KEYMAP, "default");
    Services.prefs.setBoolPref(AUTO_CLOSE, false);
    Services.prefs.setBoolPref(AUTOCOMPLETE, false);
    Services.prefs.setBoolPref(DETECT_INDENT, false);

    is(ed.getOption("tabSize"), 2, "tabSize is correct");
    is(ed.getOption("indentUnit"), 2, "indentUnit is correct");
    is(ed.getOption("indentWithTabs"), true, "indentWithTabs is correct");
    is(ed.getOption("keyMap"), "default", "keyMap is correct");
    is(ed.getOption("autoCloseBrackets"), "", "autoCloseBrackets is correct");
    is(ed.getOption("autocomplete"), true, "autocomplete is correct");
    ok(!ed.isAutocompletionEnabled(), "Autocompletion is not enabled");

    info ("Turning prefs on");

    Services.prefs.setIntPref(TAB_SIZE, 4);
    Services.prefs.setBoolPref(EXPAND_TAB, true);
    Services.prefs.setCharPref(KEYMAP, "sublime");
    Services.prefs.setBoolPref(AUTO_CLOSE, true);
    Services.prefs.setBoolPref(AUTOCOMPLETE, true);

    is(ed.getOption("tabSize"), 4, "tabSize is correct");
    is(ed.getOption("indentUnit"), 4, "indentUnit is correct");
    is(ed.getOption("indentWithTabs"), false, "indentWithTabs is correct");
    is(ed.getOption("keyMap"), "sublime", "keyMap is correct");
    is(ed.getOption("autoCloseBrackets"), "()[]{}''\"\"", "autoCloseBrackets is correct");
    is(ed.getOption("autocomplete"), true, "autocomplete is correct");
    ok(ed.isAutocompletionEnabled(), "Autocompletion is enabled");

    info ("Checking indentation detection");

    Services.prefs.setBoolPref(DETECT_INDENT, true);

    ed.setText("Detecting\n\tTabs");
    is(ed.getOption("indentWithTabs"), true, "indentWithTabs is correct");
    is(ed.getOption("indentUnit"), 4, "indentUnit is correct");

    ed.setText("body {\n  color:red;\n  a:b;\n}");
    is(ed.getOption("indentWithTabs"), false, "indentWithTabs is correct");
    is(ed.getOption("indentUnit"), 2, "indentUnit is correct");

    Services.prefs.clearUserPref(TAB_SIZE);
    Services.prefs.clearUserPref(EXPAND_TAB);
    Services.prefs.clearUserPref(KEYMAP);
    Services.prefs.clearUserPref(AUTO_CLOSE);
    Services.prefs.clearUserPref(AUTOCOMPLETE);
    Services.prefs.clearUserPref(DETECT_INDENT);

    teardown(ed, win);
  });
}
