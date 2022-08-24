/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function bosshascomming() {
    var Tag = document.createElement("style");
    Tag.innerText = `*{display:none !important;}`
    document.getElementsByTagName("head")[0].insertAdjacentElement('beforeend',Tag);
    Tag.setAttribute("id", "none");
  
    var prompts = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
    .getService(Components.interfaces.nsIPromptService);
  
    let l10n = new Localization(["browser/browser.ftl"], true);
    prompts.alert(null, l10n.formatValueSync("rest-mode"),
    l10n.formatValueSync("rest-mode-description"));
  
    const none =document.getElementById("none");
    none.remove();
  }
  
  function reloadAllOtherTabs() {
    let ourTab = BrowserWindowTracker.getTopWindow().gBrowser.selectedTab;
    BrowserWindowTracker.orderedWindows.forEach(win => {
      let otherGBrowser = win.gBrowser;
      for (let tab of otherGBrowser.tabs) {
        if (tab == ourTab) {
          continue;
        }
  
        if (tab.pinned || tab.selected) {
          otherGBrowser.reloadTab(tab);
        } else {
          otherGBrowser.discardBrowser(tab);
        }
      }
    });
  
    for (let notification of document.querySelectorAll(".reload-tabs")) {
      notification.hidden = true;
    }
  }

  function OpenChromeDirectory() {
    let currProfDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    let profileDir = currProfDir.path;
    let nsLocalFile = Components.Constructor("@mozilla.org/file/local;1", "nsIFile", "initWithPath");
    new nsLocalFile(profileDir,).reveal();
  }

  function restartbrowser(){
  Services.obs.notifyObservers(null, "startupcache-invalidate");

    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");

    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  }