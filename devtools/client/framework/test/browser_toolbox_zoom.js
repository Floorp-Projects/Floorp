/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var toolbox;

const {LocalizationHelper} = require("devtools/shared/l10n");
const L10N = new LocalizationHelper("devtools/locale/toolbox.properties");

function test() {
  addTab("about:blank").then(openToolbox);
}

function openToolbox() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  gDevTools.showToolbox(target).then((aToolbox) => {
    toolbox = aToolbox;
    toolbox.selectTool("styleeditor").then(testZoom);
  });
}

function testZoom() {
  info("testing zoom keys");

  testZoomLevel("In", 2, 1.2);
  testZoomLevel("Out", 3, 0.9);
  testZoomLevel("Reset", 1, 1);

  tidyUp();
}

function testZoomLevel(type, times, expected) {
  sendZoomKey("toolbox.zoom" + type + ".key", times);

  let zoom = getCurrentZoom(toolbox);
  is(zoom.toFixed(2), expected, "zoom level correct after zoom " + type);

  let savedZoom = parseFloat(Services.prefs.getCharPref(
    "devtools.toolbox.zoomValue"));
  is(savedZoom.toFixed(2), expected,
     "saved zoom level is correct after zoom " + type);
}

function sendZoomKey(shortcut, times) {
  for (let i = 0; i < times; i++) {
    synthesizeKeyShortcut(L10N.getStr(shortcut));
  }
}

function getCurrentZoom() {
  var contViewer = toolbox.frame.docShell.contentViewer;
  return contViewer.fullZoom;
}

function tidyUp() {
  toolbox.destroy().then(function () {
    gBrowser.removeCurrentTab();

    toolbox = null;
    finish();
  });
}
