/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { CustomizableUI } = ChromeUtils.import("resource:///modules/CustomizableUI.jsm");
const { SessionStore } = ChromeUtils.import("resource:///modules/sessionstore/SessionStore.jsm")

async function UCTFirst(){
  const widgetId = "undo-closed-tab";
  if (CustomizableUI.getWidget(widgetId)) return;
  const l10n = new Localization(["browser/floorp.ftl"])
  const l10n_text = await l10n.formatValue("undo-closed-tab") ?? "Undo close tab"
  CustomizableUI.createWidget({
    id: widgetId,
    type: 'button',
    label: l10n_text,
    tooltiptext: l10n_text,
    onCreated(aNode) {
        aNode.appendChild(
          window.MozXULElement.parseXULToFragment(
            `<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`
            )
          )

        let a = aNode.querySelector(".toolbarbutton-icon:not(stack > .toolbarbutton-icon)")
        if (a != null) {
          a.remove()
        }
    },
    onCommand() {
        let workingWindow = Services.wm.getMostRecentWindow("navigator:browser")
        if (SessionStore.getClosedTabCount(workingWindow)){
          SessionStore.undoCloseTab(workingWindow)
        }
      }
  });
}

 UCTFirst();

/**************************************** clock ****************************************/

const regionalPrefsLocales = Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences).regionalPrefsLocales;

const options = { 
  month: 'short', 
  day: 'numeric', 
  weekday: 'short'
};

const checkBrowserLangForLabel = (locale, now) => {
  return (locale[0] === "ja-JP") 
    ? now.toLocaleDateString('ja-JP', options)
    : now.toLocaleDateString();
};

const checkBrowserLangForToolTipText = (locale, now) => {
  const year = now.getFullYear();
  const JPYear = year - 2018;
  return (locale[0] === "ja-JP")
    ? `${year}年(令和${JPYear}年) ${now.toLocaleDateString('ja-JP', options)}`
    : now.toLocaleDateString();
};

const setNowTime = () => {
  const now = new Date();
  const timeFormat = new Intl.DateTimeFormat(navigator.language, {
    hour: "numeric",
    minute: "numeric",
    second: "numeric",
  });
  const timeString = timeFormat.format(now);
  const clock = document.getElementById("toolbarItemClock");
  if (clock){
    clock.setAttribute("label", checkBrowserLangForLabel(regionalPrefsLocales, now) + " " + timeString);
    clock.setAttribute("tooltiptext", checkBrowserLangForToolTipText(regionalPrefsLocales, now) + " " + timeString);
  }
};

const createClockWidget = () => {
  const id = "toolbarItemClock";
  if (!CustomizableUI.getWidget(id)) {
    const vanilla = "--:--"
    CustomizableUI.createWidget({
      id: id,
      label: vanilla,
      tooltiptext: vanilla,
    });
  }
};

const init = () => {
  if(Services.prefs.getBoolPref("floorp.browser.clock.enabled")) {
    createClockWidget();
    setInterval(setNowTime, 1000);
  }
};init();