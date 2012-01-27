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
 * The Original Code is Inspector Tab Switch Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *   Dave Camp <dcamp@mozilla.com>
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

/**
 * Tests that properties disabled in the rule view survive a tab switch.
 */

let div;
let tab1;

function waitForRuleView(aCallback)
{
  if (InspectorUI.ruleView) {
    aCallback();
    return;
  }

  let ruleViewFrame = InspectorUI.getToolIframe(InspectorUI.ruleViewObject);
  ruleViewFrame.addEventListener("load", function(evt) {
    ruleViewFrame.removeEventListener(evt.type, arguments.callee, true);
    executeSoon(function() {
      aCallback();
    });
  }, true);
}

function inspectorTabOpen1()
{
  Services.obs.addObserver(inspectorUIOpen1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function inspectorUIOpen1()
{
  Services.obs.removeObserver(inspectorUIOpen1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Highlight a node.
  div = content.document.getElementsByTagName("div")[0];
  InspectorUI.inspectNode(div);

  // Open the rule view sidebar.
  waitForRuleView(ruleViewOpened1);

  InspectorUI.showSidebar();
  InspectorUI.ruleButton.click();
}

function ruleViewOpened1()
{
  let prop = InspectorUI.ruleView._elementStyle.rules[0].textProps[0];
  is(prop.name, "background-color", "First prop is the background color prop.");
  prop.setEnabled(false);

  // Open second tab and switch to it
  gBrowser.selectedTab = gBrowser.addTab();

  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
                                                 true);
    waitForFocus(inspectorTabOpen2, content);
  }, true);
  content.location = "data:text/html,<p>tab 2: the inspector should close now";
}

function inspectorTabOpen2()
{
  // Switch back to tab 1.
  executeSoon(function() {
    Services.obs.addObserver(inspectorFocusTab1,
      InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
    gBrowser.removeCurrentTab();
    gBrowser.selectedTab = tab1;
  });
}

function inspectorFocusTab1()
{
  Services.obs.removeObserver(inspectorFocusTab1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  // Now wait for the rule view to load again...
  waitForRuleView(ruleViewOpened2);
}

function ruleViewOpened2()
{
  let prop = InspectorUI.ruleView._elementStyle.rules[0].textProps[0];
  is(prop.name, "background-color", "First prop is the background color prop.");
  ok(!prop.enabled, "First prop should be disabled.");

  InspectorUI.closeInspectorUI();
  gBrowser.removeCurrentTab();
  finish();
}

function test()
{
  waitForExplicitFinish();

  tab1 = gBrowser.addTab();
  gBrowser.selectedTab = tab1;
  gBrowser.selectedBrowser.addEventListener("load", function(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, arguments.callee,
      true);
    waitForFocus(inspectorTabOpen1, content);
  }, true);

  content.location = "data:text/html,<p>tab switching tests for inspector" +
    '<div style="background-color: green;">tab 1</div>';
}

