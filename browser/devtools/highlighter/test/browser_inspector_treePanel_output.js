/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc = null;
let xhr = null;
let expectedResult = "";

const TEST_URI = "http://mochi.test:8888/browser/browser/devtools/highlighter/test/browser_inspector_treePanel_input.html";
const RESULT_URI = "http://mochi.test:8888/browser/browser/devtools/highlighter/test/browser_inspector_treePanel_result.html";

function tabFocused()
{
  xhr = new XMLHttpRequest();
  xhr.onreadystatechange = xhr_onReadyStateChange;
  xhr.open("GET", RESULT_URI, true);
  xhr.send(null);
}

function xhr_onReadyStateChange() {
  if (xhr.readyState != 4) {
    return;
  }

  is(xhr.status, 200, "xhr.status is 200");
  ok(!!xhr.responseText, "xhr.responseText is available");
  expectedResult = xhr.responseText.replace(/^\s+|\s+$/mg, '');
  xhr = null;

  Services.obs.addObserver(inspectorOpened,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function inspectorOpened()
{
  Services.obs.removeObserver(inspectorOpened,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);

  Services.obs.addObserver(treePanelOpened, InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY, false);
  InspectorUI.treePanel.open();
}

function treePanelOpened()
{
  Services.obs.removeObserver(treePanelOpened,
    InspectorUI.INSPECTOR_NOTIFICATIONS.TREEPANELREADY);

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.treePanel.isOpen(), "Inspector Tree Panel is open");
  InspectorUI.stopInspecting();
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");

  let elements = doc.querySelectorAll("meta, script, style, p[unknownAttribute]");
  for (let i = 0; i < elements.length; i++) {
    InspectorUI.inspectNode(elements[i]);
  }

  let iframe = doc.querySelector("iframe");
  ok(iframe, "Found the iframe tag");
  ok(iframe.contentDocument, "Found the iframe.contentDocument");

  let iframeDiv = iframe.contentDocument.querySelector("div");
  ok(iframeDiv, "Found the div element inside the iframe");
  InspectorUI.inspectNode(iframeDiv);

  ok(InspectorUI.treePanel.treePanelDiv, "InspectorUI.treePanelDiv is available");
  is(InspectorUI.treePanel.treePanelDiv.innerHTML.replace(/^\s+|\s+$/mg, ''),
    expectedResult, "treePanelDiv.innerHTML is correct");
  expectedResult = null;

  Services.obs.addObserver(inspectorClosed,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
  InspectorUI.closeInspectorUI();
}

function inspectorClosed()
{
  Services.obs.removeObserver(inspectorClosed,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.treePanel, "Inspector Tree Panel is not open");

  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee, true);
    doc = content.document;
    waitForFocus(tabFocused, content);
  }, true);

  content.location = TEST_URI;
}
