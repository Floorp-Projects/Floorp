/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

let menuitems = [];
let menupopups = [];
let huds = [];
let tabs = [];
let runCount = 0;

const TEST_URI1 = "data:text/html;charset=utf-8,Web Console test for " +
                  "bug 602572: log bodies checkbox. tab 1";
const TEST_URI2 = "data:text/html;charset=utf-8,Web Console test for " +
                  "bug 602572: log bodies checkbox. tab 2";

function test() {
  if (runCount == 0) {
    requestLongerTimeout(2);
  }

  // open tab 2
  function openTab() {
    loadTab(TEST_URI2).then((tab) => {
      tabs.push(tab.tab);
      openConsole().then((hud) => {
        hud.iframeWindow.requestAnimationFrame(startTest);
      });
    });
  }

  // open tab 1
  loadTab(TEST_URI1).then((tab) => {
    tabs.push(tab.tab);
    openConsole().then((hud) => {
      hud.iframeWindow.requestAnimationFrame(() => {
        info("iframe1 root height " + hud.ui.rootElement.clientHeight);

        openTab();
      });
    });
  });
}

function startTest() {
  // Find the relevant elements in the Web Console of tab 2.
  let win2 = tabs[runCount * 2 + 1].linkedBrowser.contentWindow;
  huds[1] = HUDService.getHudByWindow(win2);
  info("startTest: iframe2 root height " + huds[1].ui.rootElement.clientHeight);

  if (runCount == 0) {
    menuitems[1] = huds[1].ui.rootElement.querySelector("#saveBodies");
  } else {
    menuitems[1] = huds[1].ui.rootElement
                             .querySelector("#saveBodiesContextMenu");
  }
  menupopups[1] = menuitems[1].parentNode;

  // Open the context menu from tab 2.
  menupopups[1].addEventListener("popupshown", onpopupshown2, false);
  executeSoon(function() {
    menupopups[1].openPopup();
  });
}

function onpopupshown2(evt) {
  menupopups[1].removeEventListener(evt.type, onpopupshown2, false);

  // By default bodies are not logged.
  isnot(menuitems[1].getAttribute("checked"), "true",
        "menuitems[1] is not checked");

  ok(!huds[1].ui._saveRequestAndResponseBodies, "bodies are not logged");

  // Enable body logging.
  huds[1].ui.setSaveRequestAndResponseBodies(true).then(() => {
    menupopups[1].hidePopup();
  });

  menupopups[1].addEventListener("popuphidden", function _onhidden(evtPopup) {
    menupopups[1].removeEventListener(evtPopup.type, _onhidden, false);

    info("menupopups[1] hidden");

    // Reopen the context menu.
    huds[1].ui.once("save-bodies-ui-toggled", () => testpopup2b(evtPopup));
    menupopups[1].openPopup();
  }, false);
}

function testpopup2b() {
  is(menuitems[1].getAttribute("checked"), "true", "menuitems[1] is checked");

  menupopups[1].addEventListener("popuphidden", function _onhidden(evtPopup) {
    menupopups[1].removeEventListener(evtPopup.type, _onhidden, false);

    info("menupopups[1] hidden");

    // Switch to tab 1 and open the Web Console context menu from there.
    gBrowser.selectedTab = tabs[runCount * 2];
    waitForFocus(function() {
      // Find the relevant elements in the Web Console of tab 1.
      let win1 = tabs[runCount * 2].linkedBrowser.contentWindow;
      huds[0] = HUDService.getHudByWindow(win1);

      info("iframe1 root height " + huds[0].ui.rootElement.clientHeight);

      menuitems[0] = huds[0].ui.rootElement.querySelector("#saveBodies");
      menupopups[0] = huds[0].ui.rootElement.querySelector("menupopup");

      menupopups[0].addEventListener("popupshown", onpopupshown1, false);
      executeSoon(() => menupopups[0].openPopup());
    }, tabs[runCount * 2].linkedBrowser.contentWindow);
  }, false);

  executeSoon(function() {
    menupopups[1].hidePopup();
  });
}

function onpopupshown1(evt) {
  menupopups[0].removeEventListener(evt.type, onpopupshown1, false);

  // The menuitem checkbox must not be in sync with the other tabs.
  isnot(menuitems[0].getAttribute("checked"), "true",
        "menuitems[0] is not checked");

  // Enable body logging for tab 1 as well.
  huds[0].ui.setSaveRequestAndResponseBodies(true).then(() => {
    menupopups[0].hidePopup();
  });

  // Close the menu, and switch back to tab 2.
  menupopups[0].addEventListener("popuphidden", function _onhidden(evtPopup) {
    menupopups[0].removeEventListener(evtPopup.type, _onhidden, false);

    info("menupopups[0] hidden");

    gBrowser.selectedTab = tabs[runCount * 2 + 1];
    waitForFocus(function() {
      // Reopen the context menu from tab 2.
      huds[1].ui.once("save-bodies-ui-toggled", () => testpopup2c(evtPopup));
      menupopups[1].openPopup();
    }, tabs[runCount * 2 + 1].linkedBrowser.contentWindow);
  }, false);
}

function testpopup2c() {
  is(menuitems[1].getAttribute("checked"), "true", "menuitems[1] is checked");

  menupopups[1].addEventListener("popuphidden", function _onhidden(evtPopup) {
    menupopups[1].removeEventListener(evtPopup.type, _onhidden, false);

    info("menupopups[1] hidden");

    // Done if on second run
    closeConsole(gBrowser.selectedTab).then(function() {
      if (runCount == 0) {
        runCount++;
        info("start second run");
        executeSoon(test);
      } else {
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
