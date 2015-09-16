/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Jennifer Fong <jfong@mozilla.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

const TEST_URI = "http://example.com/";

var test = asyncTest(function*() {
  yield loadTab(TEST_URI);
  Services.prefs.setIntPref("devtools.webconsole.fontSize", 10);
  let hud = yield HUDService.toggleBrowserConsole();

  let inputNode = hud.jsterm.inputNode;
  let outputNode = hud.jsterm.outputNode;
  outputNode.focus();

  EventUtils.synthesizeKey("-", { accelKey: true }, hud.iframeWindow);
  is(inputNode.style.fontSize, "10px",
     "input font stays at same size with ctrl+-");
  is(outputNode.style.fontSize, inputNode.style.fontSize,
     "output font stays at same size with ctrl+-");

  EventUtils.synthesizeKey("=", { accelKey: true }, hud.iframeWindow);
  is(inputNode.style.fontSize, "11px", "input font increased with ctrl+=");
  is(outputNode.style.fontSize, inputNode.style.fontSize,
     "output font stays at same size with ctrl+=");

  EventUtils.synthesizeKey("-", { accelKey: true }, hud.iframeWindow);
  is(inputNode.style.fontSize, "10px", "font decreased with ctrl+-");
  is(outputNode.style.fontSize, inputNode.style.fontSize,
     "output font stays at same size with ctrl+-");

  EventUtils.synthesizeKey("0", { accelKey: true }, hud.iframeWindow);
  is(inputNode.style.fontSize, "", "font reset with ctrl+0");
  is(outputNode.style.fontSize, inputNode.style.fontSize,
     "output font stays at same size with ctrl+0");
});
