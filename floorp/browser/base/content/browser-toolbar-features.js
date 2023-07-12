/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { CustomizableUI } = ChromeUtils.import("resource:///modules/CustomizableUI.jsm");
const { SessionStore } = ChromeUtils.import("resource:///modules/sessionstore/SessionStore.jsm")

async function UCTFirst(){
  const widgetId = "undo-closed-tab";
  if (CustomizableUI.getWidget(widgetId) &&
      CustomizableUI.getWidget(widgetId).type != "custom") return;
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
        undoCloseTab();
      }
  });
  if (ChromeUtils.import("resource:///modules/FloorpStartup.jsm").isFirstRun) {
    CustomizableUI.addWidgetToArea("undo-closed-tab", CustomizableUI.AREA_NAVBAR, -1);
  }
}
UCTFirst();

async function switchSidebarPositionButton() {
  const widgetId = "sidebar-reverse-position-toolbar";
  if (CustomizableUI.getWidget(widgetId) &&
      CustomizableUI.getWidget(widgetId).type != "custom") return;
  let l10n = new Localization(["browser/floorp.ftl"]);
  let l10n_text = await l10n.formatValue("sidebar-reverse-position-toolbar");
  CustomizableUI.createWidget({
    id: widgetId,
    type: "button",
    label: l10n_text,
    tooltiptext: l10n_text,
    onCreated(aNode) {
      aNode.appendChild(window.MozXULElement.parseXULToFragment(`<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`))
    },
    onCommand() {
      SidebarUI.reversePosition();
    }
  });
  if (ChromeUtils.import("resource:///modules/FloorpStartup.jsm").isFirstRun) {
    CustomizableUI.addWidgetToArea("sidebar-button", CustomizableUI.AREA_NAVBAR, 3);
    CustomizableUI.addWidgetToArea("sidebar-reverse-position-toolbar", CustomizableUI.AREA_NAVBAR, 4);
  }
}

switchSidebarPositionButton()

/**************************************** clock ****************************************/


const locale = Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences).regionalPrefsLocales;
const options = { month: 'short', day: 'numeric', weekday: 'short'}

function ClockFirst() {
  const id = "toolbarItemClock";
  if (!CustomizableUI.getWidget(id) ||
      CustomizableUI.getWidget(id).type == "custom") {
    let vanilla = "--:--";
    CustomizableUI.createWidget({
      id: id,
      label: vanilla,
      tooltiptext: vanilla,
    });
  }
  setInterval(setNowTime, 1000);
}

function checkBrowserLangForLabel() {
  const now = new Date();
  if(locale[0] == "ja-JP"){
    return now.toLocaleDateString('ja-JP', options);
  } else {
    return now.toLocaleDateString();
  }
}

function checkBrowserLangForToolTipText() {
  const now = new Date();
  const year = now.getFullYear();
  const JPYear = year - 2018;
  if(locale[0] == "ja-JP"){
    return `${year}年`+`(令和${JPYear}年) `+ now.toLocaleDateString('ja-JP', options);
  } else {
    return now.toLocaleDateString();
  }
}

function setNowTime() {
  const now = new Date();
  const timeFormat = new Intl.DateTimeFormat(navigator.language, {
    hour: "numeric",
    minute: "numeric",
    second: "numeric",
  });
  let timeString = timeFormat.format(now);
  let clock = document.getElementById("toolbarItemClock");
  clock?.setAttribute("label", checkBrowserLangForLabel() + " " + timeString);
  clock?.setAttribute("tooltiptext", checkBrowserLangForToolTipText() + " " + timeString);
}

if(Services.prefs.getBoolPref("floorp.browser.clock.enabled")) {
  ClockFirst();
}
