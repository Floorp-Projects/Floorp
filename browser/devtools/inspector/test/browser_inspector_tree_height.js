/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
let doc;
let salutation;
let closing;

const NEWHEIGHT = 226;

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
  doc.title = "Inspector Initialization Test";
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

  if (InspectorUI.treePanelEnabled) {
    Services.obs.addObserver(treePanelTests,
      InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);

    InspectorUI.stopInspecting();

    InspectorUI.treePanel.open();
  } else
    finishInspectorTests();
}

function treePanelTests()
{
  Services.obs.removeObserver(treePanelTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);
  Services.obs.addObserver(treePanelTests2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);

  ok(InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is open");

  let height = Services.prefs.getIntPref("devtools.inspector.htmlHeight");

  is(InspectorUI.treePanel.container.height, height,
     "Container height is " + height);

  InspectorUI.treePanel.container.height = NEWHEIGHT;

  executeSoon(function() {
    InspectorUI.treePanel.close();
    InspectorUI.treePanel.open();
  });
}

function treePanelTests2()
{
  Services.obs.removeObserver(treePanelTests2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);

  ok(InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is open");

  let height = Services.prefs.getIntPref("devtools.inspector.htmlHeight");

  is(InspectorUI.treePanel.container.height, NEWHEIGHT,
     "Container height is now " + height);

  InspectorUI.treePanel.close();
  executeSoon(function() {
    finishInspectorTests()
  });
}

function finishInspectorTests()
{
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,browser_inspector_tree_height.js";
}

