function whenDelayedStartupFinished(aWindow, aCallback) {
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

function findChromeWindowByURI(aURI) {
  let windows = Services.wm.getEnumerator(null);
  while (windows.hasMoreElements()) {
    let win = windows.getNext();
    if (win.location.href == aURI)
      return win;
  }
  return null;
}

function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

function findToolbarCustomizationWindow(aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  let iframe = aBrowserWin.document.getElementById("customizeToolbarSheetIFrame");
  let win = iframe && iframe.contentWindow;
  if (win)
    return win;

  win = findChromeWindowByURI("chrome://global/content/customizeToolbar.xul");
  if (win && win.opener == aBrowserWin)
    return win;

  throw Error("Failed to find the customization window");
}

function openToolbarCustomizationUI(aCallback, aBrowserWin) {
  if (!aBrowserWin)
    aBrowserWin = window;

  aBrowserWin.document.getElementById("cmd_CustomizeToolbars").doCommand();

  aBrowserWin.gNavToolbox.addEventListener("beforecustomization", function UI_loaded() {
    aBrowserWin.gNavToolbox.removeEventListener("beforecustomization", UI_loaded);

    let win = findToolbarCustomizationWindow(aBrowserWin);
    waitForFocus(function () {
      aCallback(win);
    }, win);
  });
}

function closeToolbarCustomizationUI(aCallback, aBrowserWin) {
  let win = findToolbarCustomizationWindow(aBrowserWin);

  win.addEventListener("unload", function unloaded() {
    win.removeEventListener("unload", unloaded);
    executeSoon(aCallback);
  });

  let button = win.document.getElementById("donebutton");
  button.focus();
  button.doCommand();
}

function waitForCondition(condition, nextTest, errorMsg) {
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= 30) {
      ok(false, errorMsg);
      moveOn();
    }
    if (condition()) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}
