/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let div1;
let div2;
let iframe1;
let iframe2;

function createDocument()
{
  doc.title = "Inspector iframe Tests";

  iframe1 = doc.createElement('iframe');

  iframe1.addEventListener("load", function () {
    iframe1.removeEventListener("load", arguments.callee, false);

    div1 = iframe1.contentDocument.createElement('div');
    div1.textContent = 'little div';
    iframe1.contentDocument.body.appendChild(div1);

    iframe2 = iframe1.contentDocument.createElement('iframe');

    iframe2.addEventListener('load', function () {
      iframe2.removeEventListener("load", arguments.callee, false);

      div2 = iframe2.contentDocument.createElement('div');
      div2.textContent = 'nested div';
      iframe2.contentDocument.body.appendChild(div2);

      openInspector(runIframeTests);
    }, false);

    iframe2.src = 'data:text/html,nested iframe';
    iframe1.contentDocument.body.appendChild(iframe2);
  }, false);

  iframe1.src = 'data:text/html,little iframe';
  doc.body.appendChild(iframe1);
}

function moveMouseOver(aElement)
{
  EventUtils.synthesizeMouse(aElement, 2, 2, {type: "mousemove"},
    aElement.ownerDocument.defaultView);
}

function runIframeTests()
{
  getActiveInspector().selection.once("new-node", performTestComparisons1);
  moveMouseOver(div1)
}

function performTestComparisons1()
{
  let i = getActiveInspector();
  is(i.selection.node, div1, "selection matches div1 node");
  is(getHighlitNode(), div1, "highlighter matches selection");

  i.selection.once("new-node", performTestComparisons2);
  executeSoon(function() {
    moveMouseOver(div2);
  });
}

function performTestComparisons2()
{
  let i = getActiveInspector();

  is(i.selection.node, div2, "selection matches div2 node");
  is(getHighlitNode(), div2, "highlighter matches selection");

  selectRoot();
}

function selectRoot()
{
  // Select the root document element to clear the breadcrumbs.
  let i = getActiveInspector();
  i.selection.setNode(doc.documentElement);
  i.once("inspector-updated", selectIframe);
}

function selectIframe()
{
  // Directly select an element in an iframe (without navigating to it
  // with mousemoves).
  let i = getActiveInspector();
  i.selection.setNode(div2);
  i.once("inspector-updated", () => {
    let breadcrumbs = i.breadcrumbs;
    is(breadcrumbs.nodeHierarchy.length, 9, "Should have 9 items");
    finish();
  });
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function() {
    gBrowser.selectedBrowser.removeEventListener("load", arguments.callee, true);
    doc = content.document;
    gBrowser.selectedBrowser.focus();
    createDocument();
  }, true);

  content.location = "data:text/html,iframe tests for inspector";

  registerCleanupFunction(function () {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
    gBrowser.removeCurrentTab();
  });
}

