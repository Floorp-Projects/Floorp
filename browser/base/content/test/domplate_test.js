/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let doc;
let div;
let plate;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource:///modules/domplate.jsm");

function createDocument()
{
  doc.body.innerHTML = '<div id="first">no</div>';
  doc.title = "Domplate Test";
  setupDomplateTests();
}

function setupDomplateTests()
{
  ok(domplate, "domplate is defined");
  plate = domplate({tag: domplate.DIV("Hello!")});
  ok(plate, "template is defined");
  div = doc.getElementById("first");
  ok(div, "we have our div");
  plate.tag.replace({}, div, template);
  is(div.innerText, "Hello!", "Is the div's innerText replaced?");
  finishUp();
}

function finishUp()
{
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

  content.location = "data:text/html,basic domplate tests";
}

