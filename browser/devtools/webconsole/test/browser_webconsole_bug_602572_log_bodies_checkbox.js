/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

let menuitems = [], menupopups = [], huds = [], tabs = [];

function test()
{
  // open tab 1
  addTab("data:text/html;charset=utf-8,Web Console test for bug 602572: log bodies checkbox. tab 1");
  tabs.push(tab);

  browser.addEventListener("load", function onLoad1(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad1, true);

    openConsole(null, function() {
      // open tab 2
      addTab("data:text/html;charset=utf-8,Web Console test for bug 602572: log bodies checkbox. tab 2");
      tabs.push(tab);

      browser.addEventListener("load", function onLoad2(aEvent) {
        browser.removeEventListener(aEvent.type, onLoad2, true);

        openConsole(null, startTest);
      }, true);
    });
  }, true);
}

function startTest()
{
  // Find the relevant elements in the Web Console of tab 2.
  let win2 = tabs[1].linkedBrowser.contentWindow;
  let hudId2 = HUDService.getHudIdByWindow(win2);
  huds[1] = HUDService.hudReferences[hudId2];
  HUDService.disableAnimation(hudId2);

  menuitems[1] = huds[1].HUDBox.querySelector("menuitem[buttonType=saveBodies]");
  menupopups[1] = huds[1].HUDBox.querySelector("menupopup");

  // Open the context menu from tab 2.
  menupopups[1].addEventListener("popupshown", onpopupshown2, false);
  menupopups[1].openPopup(huds[1].outputNode, "overlap", 10, 10, true, false);
}

function onpopupshown2(aEvent)
{
  menupopups[1].removeEventListener(aEvent.type, onpopupshown2, false);

  // By default bodies are not logged.
  isnot(menuitems[1].getAttribute("checked"), "true",
        "menuitems[1] is not checked");

  ok(!huds[1].saveRequestAndResponseBodies, "bodies are not logged");

  // Enable body logging.
  huds[1].saveRequestAndResponseBodies = true;

  menupopups[1].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[1].removeEventListener(aEvent.type, _onhidden, false);

    // Reopen the context menu.
    menupopups[1].addEventListener("popupshown", onpopupshown2b, false);
    menupopups[1].openPopup(huds[1].outputNode, "overlap", 11, 11, true, false);
  }, false);
  menupopups[1].hidePopup();
}

function onpopupshown2b(aEvent)
{
  menupopups[1].removeEventListener(aEvent.type, onpopupshown2b, false);
  is(menuitems[1].getAttribute("checked"), "true", "menuitems[1] is checked");

  menupopups[1].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[1].removeEventListener(aEvent.type, _onhidden, false);

    // Switch to tab 1 and open the Web Console context menu from there.
    gBrowser.selectedTab = tabs[0];
    waitForFocus(function() {
      // Find the relevant elements in the Web Console of tab 1.
      let win1 = tabs[0].linkedBrowser.contentWindow;
      let hudId1 = HUDService.getHudIdByWindow(win1);
      huds[0] = HUDService.hudReferences[hudId1];
      HUDService.disableAnimation(hudId1);

      menuitems[0] = huds[0].HUDBox.querySelector("menuitem[buttonType=saveBodies]");
      menupopups[0] = huds[0].HUDBox.querySelector("menupopup");

      menupopups[0].addEventListener("popupshown", onpopupshown1, false);
      menupopups[0].openPopup(huds[0].outputNode, "overlap", 12, 12, true, false);
    }, tabs[0].linkedBrowser.contentWindow);
  }, false);
  menupopups[1].hidePopup();
}

function onpopupshown1(aEvent)
{
  menupopups[0].removeEventListener(aEvent.type, onpopupshown1, false);

  // The menuitem checkbox must not be in sync with the other tabs.
  isnot(menuitems[0].getAttribute("checked"), "true",
        "menuitems[0] is not checked");

  // Enable body logging for tab 1 as well.
  huds[0].saveRequestAndResponseBodies = true;

  // Close the menu, and switch back to tab 2.
  menupopups[0].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[0].removeEventListener(aEvent.type, _onhidden, false);

    gBrowser.selectedTab = tabs[1];
    waitForFocus(function() {
      // Reopen the context menu from tab 2.
      menupopups[1].addEventListener("popupshown", onpopupshown2c, false);
      menupopups[1].openPopup(huds[1].outputNode, "overlap", 13, 13, true, false);
    }, tabs[1].linkedBrowser.contentWindow);
  }, false);
  menupopups[0].hidePopup();
}

function onpopupshown2c(aEvent)
{
  menupopups[1].removeEventListener(aEvent.type, onpopupshown2c, false);

  is(menuitems[1].getAttribute("checked"), "true", "menuitems[1] is checked");

  menupopups[1].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[1].removeEventListener(aEvent.type, _onhidden, false);

    // Done!
    huds = menuitems = menupopups = tabs = null;
    closeConsole(gBrowser.selectedTab, function() {
      gBrowser.removeCurrentTab();
      executeSoon(finishTest);
    });
  }, false);
  menupopups[1].hidePopup();
}
