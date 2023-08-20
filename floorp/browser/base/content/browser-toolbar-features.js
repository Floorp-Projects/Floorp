/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { CustomizableUI } = ChromeUtils.import("resource:///modules/CustomizableUI.jsm");
const { SessionStore } = ChromeUtils.import("resource:///modules/sessionstore/SessionStore.jsm")

async function UCTFirst() {
  const widgetId = "undo-closed-tab";
  const widget = CustomizableUI.getWidget(widgetId);
  if (widget && widget.type != "custom") {
    return;
  }
  const l10n = new Localization(["browser/floorp.ftl"]);
  const l10nText = await l10n.formatValue("undo-closed-tab") ?? "Undo close tab";
  CustomizableUI.createWidget({
    id: widgetId,
    type: "button",
    label: l10nText,
    tooltiptext: l10nText,
    onCreated(aNode) {
      const fragment = window.MozXULElement.parseXULToFragment(`<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`);
      aNode.appendChild(fragment);
      const icon = aNode.querySelector(".toolbarbutton-icon:not(stack > .toolbarbutton-icon)");
      if (icon) {
        icon.remove();
      }
    },
    onCommand() {
      undoCloseTab();
    }
  });
  if (ChromeUtils.import("resource:///modules/FloorpStartup.jsm").isFirstRun) {
    CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR, -1);
  }
}

UCTFirst();

async function switchSidebarPositionButton() {
  const widgetId = "sidebar-reverse-position-toolbar";
  const widget = CustomizableUI.getWidget(widgetId);
  if (widget && widget.type !== "custom") {
    return;
  }
  const l10n = new Localization(["browser/floorp.ftl"]);
  const l10nText = await l10n.formatValue("sidebar-reverse-position-toolbar");
  CustomizableUI.createWidget({
    id: widgetId,
    type: "button",
    label: l10nText,
    tooltiptext: l10nText,
    onCreated(aNode) {
      const fragment = window.MozXULElement.parseXULToFragment(`<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`);
      aNode.appendChild(fragment);
    },
    onCommand() {
      SidebarUI.reversePosition();
    }
  });
  if (ChromeUtils.import("resource:///modules/FloorpStartup.jsm").isFirstRun) {
    CustomizableUI.addWidgetToArea("sidebar-button", CustomizableUI.AREA_NAVBAR, 3);
    CustomizableUI.addWidgetToArea(widgetId, CustomizableUI.AREA_NAVBAR, 4);
  }
}

switchSidebarPositionButton();

/**************************************** clock ****************************************/


const locale = Cc["@mozilla.org/intl/ospreferences;1"].getService(Ci.mozIOSPreferences).regionalPrefsLocales;
const options = { month: 'short', day: 'numeric', weekday: 'short'}

function ClockFirst() {
  const id = "toolbarItemClock";
  const widget = CustomizableUI.getWidget(id);
  if (!widget || widget.type === "custom") {
    const vanilla = "--:--";
    CustomizableUI.createWidget({
      id,
      label: vanilla,
      tooltiptext: vanilla,
    });
  }
  setInterval(setNowTime, 1000);
}

function checkBrowserLangForLabel() {
  const now = new Date();
  const options = { year: "numeric", month: "numeric", day: "numeric" };
  const locale = navigator.language.split("-");
  if (locale[0] === "ja") {
    return now.toLocaleDateString("ja-JP", options);
  } else {
    return now.toLocaleDateString();
  }
}

function checkBrowserLangForToolTipText() {
  const now = new Date();
  const year = now.getFullYear();
  const JPYear = year - 2018;
  const options = { year: "numeric", month: "numeric", day: "numeric" };
  const locale = navigator.language.split("-");
  if (locale[0] === "ja") {
    return `${year}年(令和${JPYear}年) ${now.toLocaleDateString("ja-JP", options)}`;
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
  const timeString = timeFormat.format(now);
  const clock = document.getElementById("toolbarItemClock");
  clock?.setAttribute("label", `${checkBrowserLangForLabel()} ${timeString}`);
  clock?.setAttribute("tooltiptext", `${checkBrowserLangForToolTipText()} ${timeString}`);
}

if (Services.prefs.getBoolPref("floorp.browser.clock.enabled")) {
  ClockFirst();
}
