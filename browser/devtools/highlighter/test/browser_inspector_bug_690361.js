/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let salutation;
let closing;

function createDocument()
{
  doc.body.innerHTML = '<div id="first" style="{ margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA}">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="{font-size: 12pt}">hi.</p>\n' +
    '<p id="body" style="{font-size: 12pt}">I am a test-case. This text exists ' +
    'solely to provide some things to test the inspector initialization.</p>\n' +
    'If you are reading this, you should go do something else instead. Maybe ' +
    'read a book. Or better yet, write some test-cases for another bit of code. ' +
    '<span style="{font-style: italic}">Maybe more inspector test-cases!</span></p>\n' +
    '<p id="closing">end transmission</p>\n' +
    '</div>';
  doc.title = "Inspector Opening and Closing Test";
  startInspectorTests();
}

function startInspectorTests()
{
  ok(InspectorUI, "InspectorUI variable exists");
  Services.obs.addObserver(runInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.toggleInspectorUI();
}

function runInspectorTests()
{
  Services.obs.removeObserver(runInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
  Services.obs.addObserver(closeInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

  ok(InspectorUI.toolbar, "we have the toolbar.");
  ok(!InspectorUI.toolbar.hidden, "toolbar is visible");
  ok(InspectorUI.inspecting, "Inspector is inspecting");
  ok(!InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is not open");
  ok(InspectorUI.highlighter, "Highlighter is up");

  salutation = doc.getElementById("salutation");
  InspectorUI.inspectNode(salutation);

  let button = document.getElementById("highlighter-closebutton");
  button.click();
}

function closeInspectorTests()
{
  Services.obs.removeObserver(closeInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
  Services.obs.addObserver(inspectorOpenedTrap,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  ok(!InspectorUI.isInspectorOpen, "Inspector Tree Panel is not open");

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    gBrowser.removeCurrentTab();
  }, true);

  gBrowser.tabContainer.addEventListener("TabSelect", finishInspectorTests, false);
}

function inspectorOpenedTrap()
{
  ok(false, "Inspector opened! Should not have done so.");
  InspectorUI.closeInspectorUI(false);
}

function finishInspectorTests()
{
  gBrowser.tabContainer.removeEventListener("TabSelect", finishInspectorTests, false);

  Services.obs.removeObserver(inspectorOpenedTrap,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

  requestLongerTimeout(4); // give the inspector a chance to open
  executeSoon(function() {
    gBrowser.removeCurrentTab();
    finish();
  });
}

function test()
{
  waitForExplicitFinish();
  ignoreAllUncaughtExceptions();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html,basic tests for inspector";
}

