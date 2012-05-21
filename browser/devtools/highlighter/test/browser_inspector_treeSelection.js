/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let h1;

function createDocument()
{
  let div = doc.createElement("div");
  let h1 = doc.createElement("h1");
  let p1 = doc.createElement("p");
  let p2 = doc.createElement("p");
  doc.title = "Inspector Tree Selection Test";
  h1.textContent = "Inspector Tree Selection Test";
  p1.textContent = "This is some example text";
  p2.textContent = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
  div.appendChild(h1);
  div.appendChild(p1);
  div.appendChild(p2);
  doc.body.appendChild(div);
  setupSelectionTests();
}

function setupSelectionTests()
{
  h1 = doc.querySelectorAll("h1")[0];
  ok(h1, "we have the header node");
  Services.obs.addObserver(runSelectionTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function runSelectionTests()
{
  Services.obs.removeObserver(runSelectionTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  executeSoon(function() {
    InspectorUI.highlighter.addListener("nodeselected", performTestComparisons);
    InspectorUI.inspectNode(h1);
  });
}

function performTestComparisons(evt)
{
  InspectorUI.highlighter.removeListener("nodeselected", performTestComparisons);

  is(h1, InspectorUI.selection, "selection matches node");
  ok(isHighlighting(), "highlighter is highlighting");
  is(getHighlitNode(), h1, "highlighter highlighting correct node");

  finishUp();
}

function finishUp() {
  InspectorUI.closeInspectorUI();
  doc = h1 = null;
  gBrowser.removeCurrentTab();
  finish();
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

