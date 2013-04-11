/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

let menuitems = [], menupopups = [], huds = [], tabs = [], runCount = 0;

function test()
{
  // open tab 1
  addTab("data:text/html;charset=utf-8,Web Console test for bug 602572: log bodies checkbox. tab 1");
  tabs.push(tab);

  browser.addEventListener("load", function onLoad1(aEvent) {
    browser.removeEventListener(aEvent.type, onLoad1, true);

    openConsole(null, function(aHud) {
      info("iframe1 root height " + aHud.ui.rootElement.clientHeight);

      // open tab 2
      addTab("data:text/html;charset=utf-8,Web Console test for bug 602572: log bodies checkbox. tab 2");
      tabs.push(tab);

      browser.addEventListener("load", function onLoad2(aEvent) {
        browser.removeEventListener(aEvent.type, onLoad2, true);

        openConsole(null, function(aHud) {
          info("iframe2 root height " + aHud.ui.rootElement.clientHeight);
          waitForFocus(startTest, aHud.iframeWindow);
        });
      }, true);
    });
  }, true);
}

function startTest()
{
  // Find the relevant elements in the Web Console of tab 2.
  let win2 = tabs[runCount*2 + 1].linkedBrowser.contentWindow;
  let hudId2 = HUDService.getHudIdByWindow(win2);
  huds[1] = HUDService.hudReferences[hudId2];

  if (runCount == 0) {
    menuitems[1] = huds[1].ui.rootElement.querySelector("#saveBodies");
  }
  else {
    menuitems[1] = huds[1].ui.rootElement.querySelector("#saveBodiesContextMenu");
  }
  menupopups[1] = menuitems[1].parentNode;

  // Open the context menu from tab 2.
  menupopups[1].addEventListener("popupshown", onpopupshown2, false);
  executeSoon(function() {
    menupopups[1].openPopup();
  });
}

function onpopupshown2(aEvent)
{
  menupopups[1].removeEventListener(aEvent.type, onpopupshown2, false);

  // By default bodies are not logged.
  isnot(menuitems[1].getAttribute("checked"), "true",
        "menuitems[1] is not checked");

  ok(!huds[1].ui.saveRequestAndResponseBodies, "bodies are not logged");

  // Enable body logging.
  huds[1].ui.saveRequestAndResponseBodies = true;

  menupopups[1].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[1].removeEventListener(aEvent.type, _onhidden, false);

    // Reopen the context menu.
    menupopups[1].addEventListener("popupshown", onpopupshown2b, false);
    executeSoon(function() {
      menupopups[1].openPopup();
    });
  }, false);

  waitForSuccess({
    name: "saveRequestAndResponseBodies update",
    validatorFn: function()
    {
      return huds[1].ui.saveRequestAndResponseBodies;
    },
    successFn: function()
    {
      menupopups[1].hidePopup();
    },
    failureFn: finishTest,
  });
}

function onpopupshown2b(aEvent)
{
  menupopups[1].removeEventListener(aEvent.type, onpopupshown2b, false);
  is(menuitems[1].getAttribute("checked"), "true", "menuitems[1] is checked");

  menupopups[1].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[1].removeEventListener(aEvent.type, _onhidden, false);

    // Switch to tab 1 and open the Web Console context menu from there.
    gBrowser.selectedTab = tabs[runCount*2];
    waitForFocus(function() {
      // Find the relevant elements in the Web Console of tab 1.
      let win1 = tabs[runCount*2].linkedBrowser.contentWindow;
      let hudId1 = HUDService.getHudIdByWindow(win1);
      huds[0] = HUDService.hudReferences[hudId1];

      info("iframe1 root height " + huds[0].ui.rootElement.clientHeight);

      menuitems[0] = huds[0].ui.rootElement.querySelector("#saveBodies");
      menupopups[0] = huds[0].ui.rootElement.querySelector("menupopup");

      menupopups[0].addEventListener("popupshown", onpopupshown1, false);
      menupopups[0].openPopup();
    }, tabs[runCount*2].linkedBrowser.contentWindow);
  }, false);

  executeSoon(function() {
    menupopups[1].hidePopup();
  });
}

function onpopupshown1(aEvent)
{
  menupopups[0].removeEventListener(aEvent.type, onpopupshown1, false);

  // The menuitem checkbox must not be in sync with the other tabs.
  isnot(menuitems[0].getAttribute("checked"), "true",
        "menuitems[0] is not checked");

  // Enable body logging for tab 1 as well.
  huds[0].ui.saveRequestAndResponseBodies = true;

  // Close the menu, and switch back to tab 2.
  menupopups[0].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[0].removeEventListener(aEvent.type, _onhidden, false);

    gBrowser.selectedTab = tabs[runCount*2 + 1];
    waitForFocus(function() {
      // Reopen the context menu from tab 2.
      menupopups[1].addEventListener("popupshown", onpopupshown2c, false);
      menupopups[1].openPopup();
    }, tabs[runCount*2 + 1].linkedBrowser.contentWindow);
  }, false);

  waitForSuccess({
    name: "saveRequestAndResponseBodies update",
    validatorFn: function()
    {
      return huds[0].ui.saveRequestAndResponseBodies;
    },
    successFn: function()
    {
      menupopups[0].hidePopup();
    },
    failureFn: finishTest,
  });
}

function onpopupshown2c(aEvent)
{
  menupopups[1].removeEventListener(aEvent.type, onpopupshown2c, false);

  is(menuitems[1].getAttribute("checked"), "true", "menuitems[1] is checked");

  menupopups[1].addEventListener("popuphidden", function _onhidden(aEvent) {
    menupopups[1].removeEventListener(aEvent.type, _onhidden, false);

    // Done if on second run
    closeConsole(gBrowser.selectedTab, function() {
      if (runCount == 0) {
        runCount++;
        executeSoon(test);
      }
      else {
        gBrowser.removeCurrentTab();
        gBrowser.selectedTab = tabs[2];
        gBrowser.removeCurrentTab();
        gBrowser.selectedTab = tabs[1];
        gBrowser.removeCurrentTab();
        gBrowser.selectedTab = tabs[0];
        gBrowser.removeCurrentTab();
        huds = menuitems = menupopups = tabs = null;
        executeSoon(finishTest);
      }
    });

  }, false);

  executeSoon(function() {
    menupopups[1].hidePopup();
  });
}
