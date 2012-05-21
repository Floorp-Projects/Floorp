/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that properties disabled in the rule view survive a tab switch.
 */

let div;
let tab1;

function waitForRuleView(aCallback)
{
  InspectorUI.currentInspector.once("sidebaractivated-ruleview", aCallback);
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
  InspectorUI.stopInspecting();

  // Open the rule view sidebar.
  waitForRuleView(ruleViewOpened1);
  InspectorUI.sidebar.show();
  InspectorUI.sidebar.activatePanel("ruleview");
}

function ruleViewOpened1()
{
  let prop = ruleView()._elementStyle.rules[0].textProps[0];
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
  let prop = ruleView()._elementStyle.rules[0].textProps[0];
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

