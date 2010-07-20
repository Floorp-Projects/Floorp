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
 * The Original Code is Inspector Initializationa and Shutdown Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

function startInspectorTests()
{
  ok(InspectorUI, "InspectorUI variable exists");
  document.addEventListener("popupshown", runInspectorTests, false);
  InspectorUI.toggleInspectorUI();
}

function runInspectorTests(evt)
{
  if (evt.target.id != "inspector-panel")
    return true;
  document.removeEventListener("popupshown", runInspectorTests, false);
  document.addEventListener("popuphidden", finishInspectorTests, false);
  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.isPanelOpen, "Inspector Tree Panel is open");
  ok(InspectorUI.isStylePanelOpen, "Inspector Style Panel is open");
  todo(InspectorUI.isDOMPanelOpen, "Inspector DOM Panel is open");
  InspectorUI.toggleInspectorUI();
}

function finishInspectorTests(evt)
{
  if (evt.target.id != "inspector-style-panel")
    return true;
  document.removeEventListener("popuphidden", finishInspectorTests, false);
  ok(!InspectorUI.isDOMPanelOpen, "Inspector DOM Panel is closed");
  ok(!InspectorUI.isStylePanelOpen, "Inspector Style Panel is closed");
  ok(!InspectorUI.isPanelOpen, "Inspector Tree Panel is closed");
  ok(!InspectorUI.inspecting, "Inspector is not highlighting");
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
    waitForFocus(startInspectorTests, content);
  }, true);
  
  content.location = "data:text/html,basic tests for inspector";
}

