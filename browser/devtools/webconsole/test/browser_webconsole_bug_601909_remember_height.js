/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Minimum console height, in pixels.
const MINIMUM_CONSOLE_HEIGHT = 150;

// Minimum page height, in pixels. This prevents the Web Console from
// remembering a height that covers the whole page.
const MINIMUM_PAGE_HEIGHT = 50;
const HEIGHT_PREF = "devtools.hud.height";

let hud, newHeight, height;

function performTests(aEvent)
{
  browser.removeEventListener(aEvent, arguments.callee, true);

  let innerHeight = content.innerHeight;

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  hud = HUDService.hudReferences[hudId].HUDBox;
  height = parseInt(hud.style.height);

  toggleConsole();

  is(newHeight, height, "same height after reopening the console");
  is(Services.prefs.getIntPref(HEIGHT_PREF), HUDService.lastConsoleHeight,
    "pref is correct");

  setHeight(Math.ceil(innerHeight * 0.5));
  toggleConsole();

  is(newHeight, height, "same height after reopening the console");
  is(Services.prefs.getIntPref(HEIGHT_PREF), HUDService.lastConsoleHeight,
    "pref is correct");

  setHeight(MINIMUM_CONSOLE_HEIGHT - 1);
  toggleConsole();

  is(newHeight, MINIMUM_CONSOLE_HEIGHT, "minimum console height is respected");
  is(Services.prefs.getIntPref(HEIGHT_PREF), HUDService.lastConsoleHeight,
    "pref is correct");

  setHeight(innerHeight - MINIMUM_PAGE_HEIGHT + 1);
  toggleConsole();

  is(newHeight, innerHeight - MINIMUM_PAGE_HEIGHT,
    "minimum page height is respected");
  is(Services.prefs.getIntPref(HEIGHT_PREF), HUDService.lastConsoleHeight,
    "pref is correct");

  setHeight(Math.ceil(innerHeight * 0.6));
  Services.prefs.setIntPref(HEIGHT_PREF, -1);
  toggleConsole();

  is(newHeight, height, "same height after reopening the console");
  is(Services.prefs.getIntPref(HEIGHT_PREF), -1, "pref is not updated");

  closeConsole();
  HUDService.lastConsoleHeight = 0;
  Services.prefs.setIntPref(HEIGHT_PREF, 0);

  executeSoon(finishTest);
}

function toggleConsole()
{
  closeConsole();
  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  hud = HUDService.hudReferences[hudId].HUDBox;
  newHeight = parseInt(hud.style.height);
}

function setHeight(aHeight)
{
  height = aHeight;
  hud.style.height = height + "px";
}

function test()
{
  addTab("data:text/html;charset=utf-8,Web Console test for bug 601909");
  browser.addEventListener("load", performTests, true);
}

