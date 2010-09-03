/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Inspector iframe Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *   Rob Campbell <rcampbell@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let doc = null;
let xhr = null;
let expectedResult = "";

const TEST_URI = "http://mochi.test:8888/browser/browser/base/content/test/browser_inspector_treePanel_input.html";
const RESULT_URI = "http://mochi.test:8888/browser/browser/base/content/test/browser_inspector_treePanel_result.html";

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

  Services.obs.addObserver(inspectorOpened, "inspector-opened", false);
  InspectorUI.openInspectorUI();
}

function inspectorOpened()
{
  Services.obs.removeObserver(inspectorOpened, "inspector-opened", false);

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.isTreePanelOpen, "Inspector Tree Panel is open");
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

  ok(InspectorUI.treePanelDiv, "InspectorUI.treePanelDiv is available");
  is(InspectorUI.treePanelDiv.innerHTML.replace(/^\s+|\s+$/mg, ''),
    expectedResult, "treePanelDiv.innerHTML is correct");
  expectedResult = null;

  Services.obs.addObserver(inspectorClosed, "inspector-closed", false);
  InspectorUI.closeInspectorUI();
}

function inspectorClosed()
{
  Services.obs.removeObserver(inspectorClosed, "inspector-closed", false);

  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
  ok(!InspectorUI.isTreePanelOpen, "Inspector Tree Panel is not open");

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
