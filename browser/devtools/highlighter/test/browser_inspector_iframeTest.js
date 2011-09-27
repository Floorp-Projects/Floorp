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
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Mihai Șucan <mihai.sucan@gmail.com>
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

      setupIframeTests();
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

function setupIframeTests()
{
  Services.obs.addObserver(runIframeTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);
  InspectorUI.openInspectorUI();
}

function runIframeTests()
{
  Services.obs.removeObserver(runIframeTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, false);

  Services.obs.addObserver(performTestComparisons1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

  executeSoon(moveMouseOver.bind(this, div1));
}

function performTestComparisons1()
{
  Services.obs.removeObserver(performTestComparisons1,
    InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);
  Services.obs.addObserver(performTestComparisons2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

  is(InspectorUI.selection, div1, "selection matches div1 node");
  is(InspectorUI.highlighter.highlitNode, div1, "highlighter matches selection");

  executeSoon(moveMouseOver.bind(this, div2));
}

function performTestComparisons2()
{
  Services.obs.removeObserver(performTestComparisons2,
    InspectorUI.INSPECTOR_NOTIFICATIONS.HIGHLIGHTING, false);

  is(InspectorUI.selection, div2, "selection matches div2 node");
  is(InspectorUI.highlighter.highlitNode, div2, "highlighter matches selection");

  finish();
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
    InspectorUI.closeInspectorUI(true);
    gBrowser.removeCurrentTab();
  });
}

