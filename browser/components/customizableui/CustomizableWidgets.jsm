/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CustomizableWidgets"];

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  RecentlyClosedTabsAndWindowsMenuUtils:
    "resource:///modules/sessionstore/RecentlyClosedTabsAndWindowsMenuUtils.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
  CharsetMenu: "resource://gre/modules/CharsetMenu.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

ChromeUtils.defineModuleGetter(
  this,
  "PanelMultiView",
  "resource:///modules/PanelMultiView.jsm"
);

const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kPrefScreenshots = "extensions.screenshots.disabled";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let scope = {};
  ChromeUtils.import("resource://gre/modules/Console.jsm", scope);
  let debug = Services.prefs.getBoolPref(kPrefCustomizationDebug, false);
  let consoleOptions = {
    maxLogLevel: debug ? "all" : "log",
    prefix: "CustomizableWidgets",
  };
  return new scope.ConsoleAPI(consoleOptions);
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "screenshotsDisabled",
  kPrefScreenshots,
  false
);

function setAttributes(aNode, aAttrs) {
  let doc = aNode.ownerDocument;
  for (let [name, value] of Object.entries(aAttrs)) {
    if (!value) {
      if (aNode.hasAttribute(name)) {
        aNode.removeAttribute(name);
      }
    } else {
      if (name == "shortcutId") {
        continue;
      }
      if (name == "label" || name == "tooltiptext") {
        let stringId = typeof value == "string" ? value : name;
        let additionalArgs = [];
        if (aAttrs.shortcutId) {
          let shortcut = doc.getElementById(aAttrs.shortcutId);
          if (shortcut) {
            additionalArgs.push(ShortcutUtils.prettifyShortcut(shortcut));
          }
        }
        value = CustomizableUI.getLocalizedProperty(
          { id: aAttrs.id },
          stringId,
          additionalArgs
        );
      }
      aNode.setAttribute(name, value);
    }
  }
}

const CustomizableWidgets = [
  {
    id: "history-panelmenu",
    type: "view",
    viewId: "PanelUI-history",
    shortcutId: "key_gotoHistory",
    tooltiptext: "history-panelmenu.tooltiptext2",
    recentlyClosedTabsPanel: "appMenu-library-recentlyClosedTabs",
    recentlyClosedWindowsPanel: "appMenu-library-recentlyClosedWindows",
    handleEvent(event) {
      switch (event.type) {
        case "PanelMultiViewHidden":
          this.onPanelMultiViewHidden(event);
          break;
        case "ViewShowing":
          this.onSubViewShowing(event);
          break;
        case "unload":
          this.onWindowUnload(event);
          break;
        default:
          throw new Error(`Unsupported event for '${this.id}'`);
      }
    },
    onViewShowing(event) {
      if (this._panelMenuView) {
        return;
      }

      let panelview = event.target;
      let document = panelview.ownerDocument;
      let window = document.defaultView;

      // While we support this panel for both Proton and non-Proton versions
      // of the AppMenu, we only want to show icons for the non-Proton
      // version. When Proton ships and we remove the non-Proton variant,
      // we can remove the subviewbutton-iconic classes from the markup.
      if (window.PanelUI.protonAppMenuEnabled) {
        let toolbarbuttons = panelview.querySelectorAll("toolbarbutton");
        for (let toolbarbutton of toolbarbuttons) {
          toolbarbutton.classList.remove("subviewbutton-iconic");
        }
      }

      PanelMultiView.getViewNode(
        document,
        "appMenuRecentlyClosedTabs"
      ).disabled = SessionStore.getClosedTabCount(window) == 0;
      PanelMultiView.getViewNode(
        document,
        "appMenuRecentlyClosedWindows"
      ).disabled = SessionStore.getClosedWindowCount(window) == 0;

      PanelMultiView.getViewNode(
        document,
        "appMenuRestoreSession"
      ).hidden = !SessionStore.canRestoreLastSession;

      // We restrict the amount of results to 42. Not 50, but 42. Why? Because 42.
      let query =
        "place:queryType=" +
        Ci.nsINavHistoryQueryOptions.QUERY_TYPE_HISTORY +
        "&sort=" +
        Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING +
        "&maxResults=42&excludeQueries=1";

      this._panelMenuView = new window.PlacesPanelview(
        document.getElementById("appMenu_historyMenu"),
        panelview,
        query
      );
      // When either of these sub-subviews show, populate them with recently closed
      // objects data.
      PanelMultiView.getViewNode(
        document,
        this.recentlyClosedTabsPanel
      ).addEventListener("ViewShowing", this);
      PanelMultiView.getViewNode(
        document,
        this.recentlyClosedWindowsPanel
      ).addEventListener("ViewShowing", this);
      // When the popup is hidden (thus the panelmultiview node as well), make
      // sure to stop listening to PlacesDatabase updates.
      panelview.panelMultiView.addEventListener("PanelMultiViewHidden", this);
      window.addEventListener("unload", this);
    },
    onViewHiding(event) {
      log.debug("History view is being hidden!");
    },
    onPanelMultiViewHidden(event) {
      let panelMultiView = event.target;
      let document = panelMultiView.ownerDocument;
      if (this._panelMenuView) {
        this._panelMenuView.uninit();
        delete this._panelMenuView;
        PanelMultiView.getViewNode(
          document,
          this.recentlyClosedTabsPanel
        ).removeEventListener("ViewShowing", this);
        PanelMultiView.getViewNode(
          document,
          this.recentlyClosedWindowsPanel
        ).removeEventListener("ViewShowing", this);
      }
      panelMultiView.removeEventListener("PanelMultiViewHidden", this);
    },
    onWindowUnload(event) {
      if (this._panelMenuView) {
        delete this._panelMenuView;
      }
    },
    onSubViewShowing(event) {
      let panelview = event.target;
      let document = event.target.ownerDocument;
      let window = document.defaultView;
      let viewType =
        panelview.id == this.recentlyClosedTabsPanel ? "Tabs" : "Windows";

      this._panelMenuView.clearAllContents(panelview);

      let utils = RecentlyClosedTabsAndWindowsMenuUtils;
      let method = `get${viewType}Fragment`;
      let fragment = utils[method](window, "toolbarbutton", true);
      let elementCount = fragment.childElementCount;
      this._panelMenuView._setEmptyPopupStatus(panelview, !elementCount);
      if (!elementCount) {
        return;
      }

      let body = document.createXULElement("vbox");
      body.className = "panel-subview-body";
      body.appendChild(fragment);
      let separator = document.createXULElement("toolbarseparator");
      let footer;
      while (--elementCount >= 0) {
        let element = body.children[elementCount];
        CustomizableUI.addShortcut(element);
        element.classList.add("subviewbutton");
        if (element.classList.contains("restoreallitem")) {
          footer = element;
          element.classList.add(
            "subviewbutton-iconic",
            "panel-subview-footer-button"
          );
        } else {
          element.classList.add("subviewbutton-iconic", "bookmark-item");
        }
      }
      panelview.appendChild(body);
      panelview.appendChild(separator);
      panelview.appendChild(footer);
    },
  },
  {
    id: "save-page-button",
    shortcutId: "key_savePage",
    tooltiptext: "save-page-button.tooltiptext3",
    onCommand(aEvent) {
      let win = aEvent.target.ownerGlobal;
      win.saveBrowser(win.gBrowser.selectedBrowser);
    },
  },
  {
    id: "find-button",
    shortcutId: "key_find",
    tooltiptext: "find-button.tooltiptext3",
    onCommand(aEvent) {
      let win = aEvent.target.ownerGlobal;
      if (win.gLazyFindCommand) {
        win.gLazyFindCommand("onFindCommand");
      }
    },
  },
  {
    id: "open-file-button",
    shortcutId: "openFileKb",
    tooltiptext: "open-file-button.tooltiptext3",
    onCommand(aEvent) {
      let win = aEvent.target.ownerGlobal;
      win.BrowserOpenFileWindow();
    },
  },
  {
    id: "sidebar-button",
    tooltiptext: "sidebar-button.tooltiptext2",
    onCommand(aEvent) {
      let win = aEvent.target.ownerGlobal;
      win.SidebarUI.toggle();
    },
    onCreated(aNode) {
      // Add an observer so the button is checked while the sidebar is open
      let doc = aNode.ownerDocument;
      let obChecked = doc.createXULElement("observes");
      obChecked.setAttribute("element", "sidebar-box");
      obChecked.setAttribute("attribute", "checked");
      let obPosition = doc.createXULElement("observes");
      obPosition.setAttribute("element", "sidebar-box");
      obPosition.setAttribute("attribute", "positionend");

      aNode.appendChild(obChecked);
      aNode.appendChild(obPosition);
    },
  },
  {
    id: "add-ons-button",
    shortcutId: "key_openAddons",
    l10nId: "toolbar-addons-themes-button",
    onCommand(aEvent) {
      let win = aEvent.target.ownerGlobal;
      win.BrowserOpenAddonsMgr();
    },
  },
  {
    id: "zoom-controls",
    type: "custom",
    tooltiptext: "zoom-controls.tooltiptext2",
    onBuild(aDocument) {
      let buttons = [
        {
          id: "zoom-out-button",
          command: "cmd_fullZoomReduce",
          label: true,
          closemenu: "none",
          tooltiptext: "tooltiptext2",
          shortcutId: "key_fullZoomReduce",
          class: "toolbarbutton-1 toolbarbutton-combined",
        },
        {
          id: "zoom-reset-button",
          command: "cmd_fullZoomReset",
          closemenu: "none",
          tooltiptext: "tooltiptext2",
          shortcutId: "key_fullZoomReset",
          class: "toolbarbutton-1 toolbarbutton-combined",
        },
        {
          id: "zoom-in-button",
          command: "cmd_fullZoomEnlarge",
          closemenu: "none",
          label: true,
          tooltiptext: "tooltiptext2",
          shortcutId: "key_fullZoomEnlarge",
          class: "toolbarbutton-1 toolbarbutton-combined",
        },
      ];

      let node = aDocument.createXULElement("toolbaritem");
      node.setAttribute("id", "zoom-controls");
      node.setAttribute(
        "label",
        CustomizableUI.getLocalizedProperty(this, "label")
      );
      node.setAttribute(
        "title",
        CustomizableUI.getLocalizedProperty(this, "tooltiptext")
      );
      // Set this as an attribute in addition to the property to make sure we can style correctly.
      node.setAttribute("removable", "true");
      node.classList.add("chromeclass-toolbar-additional");
      node.classList.add("toolbaritem-combined-buttons");

      buttons.forEach(function(aButton, aIndex) {
        if (aIndex != 0) {
          node.appendChild(aDocument.createXULElement("separator"));
        }
        let btnNode = aDocument.createXULElement("toolbarbutton");
        setAttributes(btnNode, aButton);
        node.appendChild(btnNode);
      });
      return node;
    },
  },
  {
    id: "edit-controls",
    type: "custom",
    tooltiptext: "edit-controls.tooltiptext2",
    onBuild(aDocument) {
      let buttons = [
        {
          id: "cut-button",
          command: "cmd_cut",
          label: true,
          tooltiptext: "tooltiptext2",
          shortcutId: "key_cut",
          class: "toolbarbutton-1 toolbarbutton-combined",
        },
        {
          id: "copy-button",
          command: "cmd_copy",
          label: true,
          tooltiptext: "tooltiptext2",
          shortcutId: "key_copy",
          class: "toolbarbutton-1 toolbarbutton-combined",
        },
        {
          id: "paste-button",
          command: "cmd_paste",
          label: true,
          tooltiptext: "tooltiptext2",
          shortcutId: "key_paste",
          class: "toolbarbutton-1 toolbarbutton-combined",
        },
      ];

      let node = aDocument.createXULElement("toolbaritem");
      node.setAttribute("id", "edit-controls");
      node.setAttribute(
        "label",
        CustomizableUI.getLocalizedProperty(this, "label")
      );
      node.setAttribute(
        "title",
        CustomizableUI.getLocalizedProperty(this, "tooltiptext")
      );
      // Set this as an attribute in addition to the property to make sure we can style correctly.
      node.setAttribute("removable", "true");
      node.classList.add("chromeclass-toolbar-additional");
      node.classList.add("toolbaritem-combined-buttons");

      buttons.forEach(function(aButton, aIndex) {
        if (aIndex != 0) {
          node.appendChild(aDocument.createXULElement("separator"));
        }
        let btnNode = aDocument.createXULElement("toolbarbutton");
        setAttributes(btnNode, aButton);
        node.appendChild(btnNode);
      });

      let listener = {
        onWidgetInstanceRemoved: (aWidgetId, aDoc) => {
          if (aWidgetId != this.id || aDoc != aDocument) {
            return;
          }
          CustomizableUI.removeListener(listener);
        },
        onWidgetOverflow(aWidgetNode) {
          if (aWidgetNode == node) {
            node.ownerGlobal.updateEditUIVisibility();
          }
        },
        onWidgetUnderflow(aWidgetNode) {
          if (aWidgetNode == node) {
            node.ownerGlobal.updateEditUIVisibility();
          }
        },
      };
      CustomizableUI.addListener(listener);

      return node;
    },
  },
  {
    id: "characterencoding-button",
    label: "characterencoding-button2.label",
    type: "view",
    viewId: "PanelUI-characterEncodingView",
    tooltiptext: "characterencoding-button2.tooltiptext",
    maybeDisableMenu(aDocument) {
      let window = aDocument.defaultView;
      return !(
        window.gBrowser &&
        window.gBrowser.selectedBrowser.mayEnableCharacterEncodingMenu
      );
    },
    populateList(aDocument, aContainerId, aSection) {
      let containerElem = aDocument.getElementById(aContainerId);

      containerElem.addEventListener("command", this.onCommand);

      let list = this.charsetInfo[aSection];

      for (let item of list) {
        let elem = aDocument.createXULElement("toolbarbutton");
        elem.setAttribute("label", item.label);
        elem.setAttribute("type", "checkbox");
        elem.section = aSection;
        elem.value = item.value;
        elem.setAttribute("class", "subviewbutton");
        containerElem.appendChild(elem);
      }
    },
    updateCurrentCharset(aDocument) {
      let currentCharset =
        aDocument.defaultView.gBrowser.selectedBrowser.characterSet;
      let {
        charsetAutodetected,
      } = aDocument.defaultView.gBrowser.selectedBrowser;
      currentCharset = CharsetMenu.foldCharset(
        currentCharset,
        charsetAutodetected
      );

      let pinnedContainer = aDocument.getElementById(
        "PanelUI-characterEncodingView-pinned"
      );
      let charsetContainer = aDocument.getElementById(
        "PanelUI-characterEncodingView-charsets"
      );
      let elements = [
        ...pinnedContainer.children,
        ...charsetContainer.children,
      ];

      this._updateElements(elements, currentCharset);
    },
    _updateElements(aElements, aCurrentItem) {
      if (!aElements.length) {
        return;
      }
      let disabled = this.maybeDisableMenu(aElements[0].ownerDocument);
      for (let elem of aElements) {
        if (disabled) {
          elem.setAttribute("disabled", "true");
        } else {
          elem.removeAttribute("disabled");
        }
        if (elem.value.toLowerCase() == aCurrentItem.toLowerCase()) {
          elem.setAttribute("checked", "true");
        } else {
          elem.removeAttribute("checked");
        }
      }
    },
    onViewShowing(aEvent) {
      if (!this._inited) {
        this.onInit();
      }
      let document = aEvent.target.ownerDocument;

      if (
        !document.getElementById("PanelUI-characterEncodingView-pinned")
          .firstChild
      ) {
        this.populateList(
          document,
          "PanelUI-characterEncodingView-pinned",
          "pinnedCharsets"
        );
        this.populateList(
          document,
          "PanelUI-characterEncodingView-charsets",
          "otherCharsets"
        );
      }

      this.updateCurrentCharset(document);
    },
    onCommand(aEvent) {
      let node = aEvent.target;
      if (!node.hasAttribute || !node.section) {
        return;
      }

      let window = node.ownerGlobal;
      let value = node.value;

      window.BrowserSetForcedCharacterSet(value);
    },
    onCreated(aNode) {
      let document = aNode.ownerDocument;

      let updateButton = () => {
        if (this.maybeDisableMenu(document)) {
          aNode.setAttribute("disabled", "true");
        } else {
          aNode.removeAttribute("disabled");
        }
      };

      let getPanel = () => {
        let { PanelUI } = document.ownerGlobal;
        return PanelUI.overflowPanel;
      };

      if (
        CustomizableUI.getAreaType(this.currentArea) ==
        CustomizableUI.TYPE_MENU_PANEL
      ) {
        getPanel().addEventListener("popupshowing", updateButton);
      }

      let listener = {
        onWidgetAdded: (aWidgetId, aArea) => {
          if (aWidgetId != this.id) {
            return;
          }
          if (
            CustomizableUI.getAreaType(aArea) == CustomizableUI.TYPE_MENU_PANEL
          ) {
            getPanel().addEventListener("popupshowing", updateButton);
          }
        },
        onWidgetRemoved: (aWidgetId, aPrevArea) => {
          if (aWidgetId != this.id) {
            return;
          }
          aNode.removeAttribute("disabled");
          if (
            CustomizableUI.getAreaType(aPrevArea) ==
            CustomizableUI.TYPE_MENU_PANEL
          ) {
            getPanel().removeEventListener("popupshowing", updateButton);
          }
        },
        onWidgetInstanceRemoved: (aWidgetId, aDoc) => {
          if (aWidgetId != this.id || aDoc != document) {
            return;
          }

          CustomizableUI.removeListener(listener);
          getPanel().removeEventListener("popupshowing", updateButton);
        },
      };
      CustomizableUI.addListener(listener);
      this.onInit();
    },
    onInit() {
      this._inited = true;
      if (!this.charsetInfo) {
        this.charsetInfo = CharsetMenu.getData();
      }
    },
  },
  {
    id: "email-link-button",
    tooltiptext: "email-link-button.tooltiptext3",
    onCommand(aEvent) {
      let win = aEvent.view;
      win.MailIntegration.sendLinkForBrowser(win.gBrowser.selectedBrowser);
    },
  },
];

if (Services.prefs.getBoolPref("identity.fxaccounts.enabled")) {
  CustomizableWidgets.push({
    id: "sync-button",
    label: "remotetabs-panelmenu.label",
    tooltiptext: "remotetabs-panelmenu.tooltiptext2",
    type: "view",
    viewId: "PanelUI-remotetabs",
    onViewShowing(aEvent) {
      let panelview = aEvent.target;
      let doc = panelview.ownerDocument;
      let window = doc.defaultView;

      // While we support this panel for both Proton and non-Proton versions
      // of the AppMenu, we only want to show icons for the non-Proton
      // version. When Proton ships and we remove the non-Proton variant,
      // we can remove the subviewbutton-iconic classes from the markup.
      if (window.PanelUI.protonAppMenuEnabled) {
        let toolbarbuttons = panelview.querySelectorAll("toolbarbutton");
        for (let toolbarbutton of toolbarbuttons) {
          toolbarbutton.classList.remove("subviewbutton-iconic");
        }
      }

      let syncNowBtn = panelview.querySelector(".syncnow-label");
      let l10nId = syncNowBtn.getAttribute(
        panelview.ownerGlobal.gSync._isCurrentlySyncing
          ? "syncing-data-l10n-id"
          : "sync-now-data-l10n-id"
      );
      syncNowBtn.setAttribute("data-l10n-id", l10nId);

      let SyncedTabsPanelList = doc.defaultView.SyncedTabsPanelList;
      panelview.syncedTabsPanelList = new SyncedTabsPanelList(
        panelview,
        PanelMultiView.getViewNode(doc, "PanelUI-remotetabs-deck"),
        PanelMultiView.getViewNode(doc, "PanelUI-remotetabs-tabslist")
      );
    },
    onViewHiding(aEvent) {
      aEvent.target.syncedTabsPanelList.destroy();
      aEvent.target.syncedTabsPanelList = null;
    },
  });
}

if (!screenshotsDisabled) {
  CustomizableWidgets.push({
    id: "screenshot-button",
    l10nId: "screenshot-toolbarbutton",
    onCommand(aEvent) {
      Services.obs.notifyObservers(null, "menuitem-screenshot");
    },
    onCreated(aNode) {
      this.screenshotNode = aNode;
      this.screenshotNode.ownerGlobal.MozXULElement.insertFTLIfNeeded(
        "browser/screenshots.ftl"
      );
      Services.obs.addObserver(this, "toggle-screenshot-disable");
    },
    observe(subj, topic, data) {
      if (data == "true") {
        this.screenshotNode.setAttribute("disabled", "true");
      } else {
        this.screenshotNode.removeAttribute("disabled");
      }
    },
  });
}

let preferencesButton = {
  id: "preferences-button",
  l10nId: "toolbar-settings-button",
  onCommand(aEvent) {
    let win = aEvent.target.ownerGlobal;
    win.openPreferences(undefined);
  },
};
if (AppConstants.platform == "macosx") {
  preferencesButton.shortcutId = "key_preferencesCmdMac";
}
CustomizableWidgets.push(preferencesButton);

if (Services.prefs.getBoolPref("privacy.panicButton.enabled")) {
  CustomizableWidgets.push({
    id: "panic-button",
    type: "view",
    viewId: "PanelUI-panicView",

    forgetButtonCalled(aEvent) {
      let doc = aEvent.target.ownerDocument;
      let group = doc.getElementById("PanelUI-panic-timeSpan");
      let itemsToClear = [
        "cookies",
        "history",
        "openWindows",
        "formdata",
        "sessions",
        "cache",
        "downloads",
        "offlineApps",
      ];
      let newWindowPrivateState = PrivateBrowsingUtils.isWindowPrivate(
        doc.defaultView
      )
        ? "private"
        : "non-private";
      let promise = Sanitizer.sanitize(itemsToClear, {
        ignoreTimespan: false,
        range: Sanitizer.getClearRange(+group.value),
        privateStateForNewWindow: newWindowPrivateState,
      });
      promise.then(function() {
        let otherWindow = Services.wm.getMostRecentWindow("navigator:browser");
        if (otherWindow.closed) {
          Cu.reportError("Got a closed window!");
        }
        if (otherWindow.PanicButtonNotifier) {
          otherWindow.PanicButtonNotifier.notify();
        } else {
          otherWindow.PanicButtonNotifierShouldNotify = true;
        }
      });
    },
    handleEvent(aEvent) {
      switch (aEvent.type) {
        case "command":
          this.forgetButtonCalled(aEvent);
          break;
      }
    },
    onViewShowing(aEvent) {
      let win = aEvent.target.ownerGlobal;
      let doc = win.document;
      let eventBlocker = null;
      eventBlocker = doc.l10n.translateElements([aEvent.target]);

      let forgetButton = aEvent.target.querySelector(
        "#PanelUI-panic-view-button"
      );
      let group = doc.getElementById("PanelUI-panic-timeSpan");
      group.selectedItem = doc.getElementById("PanelUI-panic-5min");
      forgetButton.addEventListener("command", this);

      if (eventBlocker) {
        aEvent.detail.addBlocker(eventBlocker);
      }
    },
    onViewHiding(aEvent) {
      let forgetButton = aEvent.target.querySelector(
        "#PanelUI-panic-view-button"
      );
      forgetButton.removeEventListener("command", this);
    },
  });
}

if (PrivateBrowsingUtils.enabled) {
  CustomizableWidgets.push({
    id: "privatebrowsing-button",
    shortcutId: "key_privatebrowsing",
    onCommand(e) {
      let win = e.target.ownerGlobal;
      win.OpenBrowserWindow({ private: true });
    },
  });
}
