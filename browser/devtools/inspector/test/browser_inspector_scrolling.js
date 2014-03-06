/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let div;
let iframe;
let inspector;

function createDocument()
{
  doc.title = "Inspector scrolling Tests";

  iframe = doc.createElement("iframe");

  iframe.addEventListener("load", function () {
    iframe.removeEventListener("load", arguments.callee, false);

    div = iframe.contentDocument.createElement("div");
    div.textContent = "big div";
    div.setAttribute("style", "height:500px; width:500px; border:1px solid gray;");
    iframe.contentDocument.body.appendChild(div);
    openInspector(inspectNode);
  }, false);

  iframe.src = "data:text/html,foo bar";
  doc.body.appendChild(iframe);
}

function inspectNode(aInspector)
{
  inspector = aInspector;

  inspector.once("inspector-updated", performScrollingTest);
  executeSoon(function() {
    inspector.selection.setNode(div, "");
  });
}

function performScrollingTest()
{
  executeSoon(function() {
    // FIXME: this will fail on retina displays. EventUtils will only scroll
    // 25px down instead of 50.
    EventUtils.synthesizeWheel(div, 10, 10,
      { deltaY: 50.0, deltaMode: WheelEvent.DOM_DELTA_PIXEL },
      iframe.contentWindow);
  });

  gBrowser.selectedBrowser.addEventListener("scroll", function() {
    gBrowser.selectedBrowser.removeEventListener("scroll", arguments.callee,
      false);

    is(iframe.contentDocument.body.scrollTop, 50, "inspected iframe scrolled");

    inspector = div = iframe = doc = null;
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.closeToolbox(target);
    gBrowser.removeCurrentTab();
    finish();
  }, false);
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

  content.location = "data:text/html,mouse scrolling test for inspector";
}
