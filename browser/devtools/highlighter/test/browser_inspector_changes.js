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
 * The Original Code is Inspect Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
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
let doc;
let testDiv;

function createDocument()
{
  doc.body.innerHTML = '<div id="testdiv">Test div!</div>';
  doc.title = "Inspector Change Test";
  startInspectorTests();
}


function getInspectorProp(aName)
{
  for each (let view in computedViewTree().propertyViews) {
    if (view.name == aName) {
      return view;
    }
  }
  return null;
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
  Services.obs.removeObserver(runInspectorTests, InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED);
  testDiv = doc.getElementById("testdiv");

  testDiv.style.fontSize = "10px";

  InspectorUI.inspectNode(testDiv);
  InspectorUI.stopInspecting();

  // Start up the style inspector panel...
  Services.obs.addObserver(stylePanelTests, "StyleInspector-populated", false);

  InspectorUI.sidebar.show();
  InspectorUI.sidebar.activatePanel("computedview");
}

function stylePanelTests()
{
  Services.obs.removeObserver(stylePanelTests, "StyleInspector-populated");

  ok(InspectorUI.sidebar.visible, "Inspector Sidebar is open");
  ok(computedViewTree(), "Style Panel has a cssHtmlTree");

  let propView = getInspectorProp("font-size");
  is(propView.value, "10px", "Style inspector should be showing the correct font size.");

  Services.obs.addObserver(stylePanelAfterChange, "StyleInspector-populated", false);

  testDiv.style.fontSize = "15px";
  InspectorUI.nodeChanged();
}

function stylePanelAfterChange()
{
  Services.obs.removeObserver(stylePanelAfterChange, "StyleInspector-populated");

  let propView = getInspectorProp("font-size");
  is(propView.value, "15px", "Style inspector should be showing the new font size.");

  stylePanelNotActive();
}

function stylePanelNotActive()
{
  // Tests changes made while the style panel is not active.
  InspectorUI.sidebar.activatePanel("ruleview");

  executeSoon(function() {
    Services.obs.addObserver(stylePanelAfterSwitch, "StyleInspector-populated", false);
    testDiv.style.fontSize = "20px";
    InspectorUI.nodeChanged();
    InspectorUI.sidebar.activatePanel("computedview");
  });
}

function stylePanelAfterSwitch()
{
  Services.obs.removeObserver(stylePanelAfterSwitch, "StyleInspector-populated");

  let propView = getInspectorProp("font-size");
  is(propView.value, "20px", "Style inspector should be showing the newest font size.");

  Services.obs.addObserver(finishTest, InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);
  executeSoon(function() {
    InspectorUI.closeInspectorUI(true);
  });
}

function finishTest()
{
  Services.obs.removeObserver(finishTest,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);
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

  content.location = "data:text/html,basic tests for inspector";
}

