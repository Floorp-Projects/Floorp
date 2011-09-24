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
 * The Original Code is Inspector Highlighter Tests.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Ratcliffe <mratcliffe@mozilla.com>
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
let h1;
let tool1;
let tool2;
let tool3;

function createDocument()
{
  let div = doc.createElement("div");
  let h1 = doc.createElement("h1");
  let p1 = doc.createElement("p");
  let p2 = doc.createElement("p");
  let div2 = doc.createElement("div");
  let p3 = doc.createElement("p");
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
  p3.textContent = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
  div.appendChild(h1);
  div.appendChild(p1);
  div.appendChild(p2);
  div2.appendChild(p3);
  doc.body.appendChild(div);
  doc.body.appendChild(div2);
  setupHighlighterTests();
}

function setupHighlighterTests()
{
  h1 = doc.querySelectorAll("h1")[0];
  ok(h1, "we have the header node");
  Services.obs.addObserver(inspectorOpen, "inspector-opened", false);
  InspectorUI.toggleInspectorUI();
}

function inspectorOpen()
{
  info("we received the inspector-opened notification");
  Services.obs.removeObserver(inspectorOpen, "inspector-opened", false);
  Services.obs.addObserver(startToolTests, "inspector-highlighting", false);
  let rect = h1.getBoundingClientRect();
  executeSoon(function() {
    EventUtils.synthesizeMouse(h1, 2, 2, {type: "mousemove"}, content);
  });
}

function startToolTests(evt)
{
  info("we received the inspector-highlighting notification");
  Services.obs.removeObserver(startToolTests, "inspector-highlighting", false);
  InspectorUI.stopInspecting();

  info("Getting InspectorUI.tools");
  let tools = InspectorUI.tools;
  tool1 = InspectorUI.tools["tool_1"];
  tool2 = InspectorUI.tools["tool_2"];
  tool3 = InspectorUI.tools["tool_3"];

  info("Checking panel states 1");
  ok(tool1.context.panelIsClosed, "Panel 1 is closed");
  ok(tool2.context.panelIsClosed, "Panel 2 is closed");
  ok(tool3.context.panelIsClosed, "Panel 3 is closed");

  info("Calling show method for all tools");
  tool1.onShow.apply(tool1.context, [h1]);
  tool2.onShow.apply(tool2.context, [h1]);
  tool3.onShow.apply(tool3.context, [h1]);

  info("Checking panel states 2");
  ok(tool1.context.panelIsOpen, "Panel 1 is open");
  ok(tool2.context.panelIsOpen, "Panel 2 is open");
  ok(tool3.context.panelIsOpen, "Panel 3 is open");

  info("Calling selectNode method for all tools");
  tool1.onSelect.apply(tool1.context, [h1]);
  tool2.onSelect.apply(tool2.context, [h1]);
  tool3.onSelect.apply(tool3.context, [h1]);

  info("Calling hide method for all tools");
  tool1.onHide.apply(tool1.context, [h1]);
  tool2.onHide.apply(tool2.context, [h1]);
  tool3.onHide.apply(tool3.context, [h1]);

  info("Checking panel states 3");
  ok(tool1.context.panelIsClosed, "Panel 1 is closed");
  ok(tool2.context.panelIsClosed, "Panel 2 is closed");
  ok(tool3.context.panelIsClosed, "Panel 3 is closed");

  info("Showing tools 1 & 3");
  tool1.onShow.apply(tool1.context, [h1]);
  tool3.onShow.apply(tool3.context, [h1]);

  info("Checking panel states 4");
  ok(tool1.context.panelIsOpen, "Panel 1 is open");
  ok(tool2.context.panelIsClosed, "Panel 2 is closed");
  ok(tool3.context.panelIsOpen, "Panel 3 is open");

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    waitForFocus(testSecondTab, content);
  }, true);

  content.location = "data:text/html,registertool new tab test for inspector";
}

function testSecondTab()
{
  info("Opened second tab");
  info("Checking panel states 5");
  ok(tool1.context.panelIsClosed, "Panel 1 is closed");
  ok(tool2.context.panelIsClosed, "Panel 2 is closed");
  ok(tool3.context.panelIsClosed, "Panel 3 is closed");

  info("Closing current tab");
  gBrowser.removeCurrentTab();

  info("Checking panel states 6");
  ok(tool1.context.panelIsOpen, "Panel 1 is open");
  ok(tool2.context.panelIsClosed, "Panel 2 is closed");
  ok(tool3.context.panelIsOpen, "Panel 3 is open");

  executeSoon(finishUp);
}

function finishUp() {
  InspectorUI.closeInspectorUI(true);
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
    waitForFocus(registerTools, content);
  }, true);
  
  content.location = "data:text/html,registertool tests for inspector";
}

function registerTools()
{
  createDocument();
  registerTool(new testTool("tool_1", "Tool 1", "Tool 1 tooltip", "I"));
  registerTool(new testTool("tool_2", "Tool 2", "Tool 2 tooltip", "J"));
  registerTool(new testTool("tool_3", "Tool 3", "Tool 3 tooltip", "K"));
}

function registerTool(aTool)
{
  InspectorUI.registerTool({
    id: aTool.id,
    label: aTool.label,
    tooltiptext: aTool.tooltip,
    accesskey: aTool.accesskey,
    context: aTool,
    onSelect: aTool.selectNode,
    onShow: aTool.show,
    onHide: aTool.hide,
    panel: aTool.panel
  });
}

// Tool Object
function testTool(aToolId, aLabel, aTooltip, aAccesskey)
{
  this.id = aToolId;
  this.label = aLabel;
  this.tooltip = aTooltip;
  this.accesskey = aAccesskey
  this.panel = this.createPanel();
}

testTool.prototype = {
  get panelIsOpen()
  {
    return this.panel.state == "open" || this.panel.state == "showing";
  },

  get panelIsClosed()
  {
    return this.panel.state == "closed" || this.panel.state == "hiding";
  },

  selectNode: function BIR_selectNode(aNode) {
    is(InspectorUI.selection, aNode,
       "selectNode: currently selected node was passed: " + this.id);
  },

  show: function BIR_show(aNode) {
    this.panel.openPopup(gBrowser.selectedBrowser,
                         "end_before", 0, 20, false, false);
    is(InspectorUI.selection, aNode,
       "show: currently selected node was passed: " + this.id);
  },

  hide: function BIR_hide() {
    info(this.id + " hide");
    this.panel.hidePopup();
  },

  createPanel: function BIR_createPanel() {
    let popupSet = document.getElementById("mainPopupSet");
    let ns = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    let panel = this.panel = document.createElementNS(ns, "panel");
    panel.setAttribute("orient", "vertical");
    panel.setAttribute("noautofocus", "true");
    panel.setAttribute("noautohide", "true");
    panel.setAttribute("titlebar", "normal");
    panel.setAttribute("close", "true");
    panel.setAttribute("label", "Panel for " + this.id);
    panel.setAttribute("width", 200);
    panel.setAttribute("height", 400);
    popupSet.appendChild(panel);

    ok(panel.parentNode == popupSet, "Panel created and appended successfully");
    return panel;
  },
};
