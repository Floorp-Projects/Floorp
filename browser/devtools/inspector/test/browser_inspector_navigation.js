/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


function test() {
  let inspector, toolbox;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(function() {
      let target = TargetFactory.forTab(gBrowser.selectedTab);
      gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
        startInspectorTests(toolbox);
      }).then(null, console.error);
    }, content);
  }, true);

  content.location = "http://test1.example.org/browser/browser/devtools/inspector/test/browser_inspector_breadcrumbs.html";

  function startInspectorTests(aToolbox)
  {
    toolbox = aToolbox;
    inspector = toolbox.getCurrentPanel();
    info("Inspector started");
    let node = content.document.querySelector("#i1");
    inspector.selection.setNode(node);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, node, "Node selected.");
      inspector.once("markuploaded", onSecondLoad);
      content.location = "http://test2.example.org/browser/browser/devtools/inspector/test/browser_inspector_breadcrumbs.html";
    });
  }

  function onSecondLoad() {
    info("New page loaded");
    let node = content.document.querySelector("#i1");
    inspector.selection.setNode(node);

    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, node, "Node re-selected.");
      inspector.once("markuploaded", onThirdLoad);
      content.history.go(-1);
    });
  }

  function onThirdLoad() {
    info("Old page loaded");
    is(content.location.href, "http://test1.example.org/browser/browser/devtools/inspector/test/browser_inspector_breadcrumbs.html");
    let node = content.document.querySelector("#i1");
    inspector.selection.setNode(node);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, node, "Node re-selected.");
      inspector.once("markuploaded", onThirdLoad);
      toolbox.destroy();
      gBrowser.removeCurrentTab();
      finish();
    });
  }
}


