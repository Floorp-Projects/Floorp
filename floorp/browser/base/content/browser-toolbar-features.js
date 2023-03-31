/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { CustomizableUI } = ChromeUtils.import(
    "resource:///modules/CustomizableUI.jsm"
);
let { SessionStore } = ChromeUtils.import("resource:///modules/sessionstore/SessionStore.jsm")
async function UCTFirst(){
  let l10n = new Localization(["browser/floorp.ftl"])
  let l10n_text = await l10n.formatValue("undo-closed-tab") ?? "Undo close Tab"
  CustomizableUI.createWidget({
    id: 'undo-closed-tab',
    type: 'button',
    label: l10n_text,
    tooltiptext: l10n_text,
    onCreated(aNode) {
        aNode.appendChild(window.MozXULElement.parseXULToFragment(`<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`))

        let a = aNode.querySelector(".toolbarbutton-icon:not(stack > .toolbarbutton-icon)")
        if (a != null) a.remove()
    },
    onCommand() {
        let workingWindow = Services.wm.getMostRecentWindow("navigator:browser")
        if (SessionStore.getClosedTabCount(workingWindow)) SessionStore.undoCloseTab(workingWindow)
    }
  });
}
 UCTFirst();

/**************************************** clock ****************************************/


let locale = Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences).regionalPrefsLocales;
const options = { month: 'short', day: 'numeric', weekday: 'short'}

function ClockFirst() {
   let vanilla = "--:--"
   CustomizableUI.createWidget({
     id: 'toolbarItemClock',
     label: vanilla,
     tooltiptext: vanilla,
    })
    setInterval(setNowTime, 1000);
}
 
function checkBrowserLangForLabel() {
  let now = new Date();

  if(locale[0] == "ja-JP"){
    return now.toLocaleDateString('ja-JP', options);
  } else {
    return now.toLocaleDateString();
  }
}

function checkBrowserLangForToolTipText() {
  let now = new Date();
  let year = now.getFullYear();
  let JPYear = year - 2018;

  if(locale[0] == "ja-JP"){
    return `${year}年`+`(令和${JPYear}年) `+ now.toLocaleDateString('ja-JP', options);
  } else {
    return now.toLocaleDateString();
  }
}

function setNowTime() {
  let now = new Date();

  let hours = now.getHours();
  let minutes = now.getMinutes();
  let seconds = now.getSeconds();
  if (hours < 10) hours = "0" + hours;
  if (minutes < 10) minutes = "0" + minutes;
  let dateAndTime = `${checkBrowserLangForLabel()} ${hours}:${minutes}`;
  let withSeconds = `${checkBrowserLangForToolTipText()} ${hours}:${minutes}:${seconds}`;
  let clock = document.getElementById("toolbarItemClock");
  clock.setAttribute("label", dateAndTime);
  clock.setAttribute("tooltiptext", withSeconds);
}

if(Services.prefs.getBoolPref("floorp.browser.clock.enabled")) {
  ClockFirst();  
}
