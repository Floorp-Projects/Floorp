/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["CustomizableWidgets"];

const { CustomizableUI } = ChromeUtils.import(
  "resource:///modules/CustomizableUI.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  RecentlyClosedTabsAndWindowsMenuUtils:
    "resource:///modules/sessionstore/RecentlyClosedTabsAndWindowsMenuUtils.jsm",
  ShortcutUtils: "resource://gre/modules/ShortcutUtils.jsm",
  Sanitizer: "resource:///modules/Sanitizer.jsm",
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

ChromeUtils.defineModuleGetter(
  lazy,
  "PanelMultiView",
  "resource:///modules/PanelMultiView.jsm"
);

const kPrefCustomizationDebug = "browser.uiCustomization.debug";
const kPrefScreenshots = "extensions.screenshots.disabled";

XPCOMUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  let debug = Services.prefs.getBoolPref(kPrefCustomizationDebug, false);
  let consoleOptions = {
    maxLogLevel: debug ? "all" : "log",
    prefix: "CustomizableWidgets",
  };
  return new ConsoleAPI(consoleOptions);
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "screenshotsDisabled",
  kPrefScreenshots,
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "SCREENSHOT_BROWSER_COMPONENT",
  "screenshots.browser.component.enabled",
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
            additionalArgs.push(lazy.ShortcutUtils.prettifyShortcut(shortcut));
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

      lazy.PanelMultiView.getViewNode(
        document,
        "appMenuRecentlyClosedTabs"
      ).disabled = lazy.SessionStore.getClosedTabCount(window) == 0;
      lazy.PanelMultiView.getViewNode(
        document,
        "appMenuRecentlyClosedWindows"
      ).disabled = lazy.SessionStore.getClosedWindowCount(window) == 0;

      lazy.PanelMultiView.getViewNode(
        document,
        "appMenu-restoreSession"
      ).hidden = !lazy.SessionStore.canRestoreLastSession;

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
      lazy.PanelMultiView.getViewNode(
        document,
        this.recentlyClosedTabsPanel
      ).addEventListener("ViewShowing", this);
      lazy.PanelMultiView.getViewNode(
        document,
        this.recentlyClosedWindowsPanel
      ).addEventListener("ViewShowing", this);
      // When the popup is hidden (thus the panelmultiview node as well), make
      // sure to stop listening to PlacesDatabase updates.
      panelview.panelMultiView.addEventListener("PanelMultiViewHidden", this);
      window.addEventListener("unload", this);
    },
    onViewHiding(event) {
      lazy.log.debug("History view is being hidden!");
    },
    onPanelMultiViewHidden(event) {
      let panelMultiView = event.target;
      let document = panelMultiView.ownerDocument;
      if (this._panelMenuView) {
        this._panelMenuView.uninit();
        delete this._panelMenuView;
        lazy.PanelMultiView.getViewNode(
          document,
          this.recentlyClosedTabsPanel
        ).removeEventListener("ViewShowing", this);
        lazy.PanelMultiView.getViewNode(
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

      this._panelMenuView.clearAllContents(panelview);

      let getFragment =
        panelview.id == this.recentlyClosedTabsPanel
          ? lazy.RecentlyClosedTabsAndWindowsMenuUtils.getTabsFragment
          : lazy.RecentlyClosedTabsAndWindowsMenuUtils.getWindowsFragment;

      let fragment = getFragment(window, "toolbarbutton", true);
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
          element.classList.add("panel-subview-footer-button");
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
    l10nId: "toolbar-button-save-page",
    shortcutId: "key_savePage",
    onCommand(aEvent) {
      let win = aEvent.target.ownerGlobal;
      win.saveBrowser(win.gBrowser.selectedBrowser);
    },
  },
  {
    id: "print-button",
    l10nId: "navbar-print",
    shortcutId: "printKb",
    keepBroadcastAttributesWhenCustomizing: true,
    onCreated(aNode) {
      aNode.setAttribute("command", "cmd_printPreviewToggle");
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
    l10nId: "toolbar-button-open-file",
    shortcutId: "openFileKb",
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
    onBeforeCreated() {
      // If the pref is set to `true`, we won't create this widget.
      return !Services.prefs.getBoolPref(
        "extensions.unifiedExtensions.enabled"
      );
    },
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
    l10nId: "repair-text-encoding-button",
    onCommand(aEvent) {
      aEvent.view.BrowserForceEncodingDetection();
    },
  },
  {
    id: "email-link-button",
    l10nId: "toolbar-button-email-link",
    onCommand(aEvent) {
      let win = aEvent.view;
      win.MailIntegration.sendLinkForBrowser(win.gBrowser.selectedBrowser);
    },
  },
];

if (Services.prefs.getBoolPref("identity.fxaccounts.enabled")) {
  CustomizableWidgets.push({
    id: "sync-button",
    l10nId: "toolbar-button-synced-tabs",
    type: "view",
    viewId: "PanelUI-remotetabs",
    onViewShowing(aEvent) {
      let panelview = aEvent.target;
      let doc = panelview.ownerDocument;

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
        lazy.PanelMultiView.getViewNode(doc, "PanelUI-remotetabs-deck"),
        lazy.PanelMultiView.getViewNode(doc, "PanelUI-remotetabs-tabslist")
      );
    },
    onViewHiding(aEvent) {
      aEvent.target.syncedTabsPanelList.destroy();
      aEvent.target.syncedTabsPanelList = null;
    },
  });
}

if (!lazy.screenshotsDisabled) {
  CustomizableWidgets.push({
    id: "screenshot-button",
    shortcutId: "key_screenshot",
    l10nId: "screenshot-toolbarbutton",
    onCommand(aEvent) {
      if (lazy.SCREENSHOT_BROWSER_COMPONENT) {
        Services.obs.notifyObservers(
          aEvent.currentTarget.ownerGlobal,
          "menuitem-screenshot"
        );
      } else {
        Services.obs.notifyObservers(
          null,
          "menuitem-screenshot-extension",
          "toolbar"
        );
      }
    },
    onCreated(aNode) {
      aNode.ownerGlobal.MozXULElement.insertFTLIfNeeded(
        "browser/screenshots.ftl"
      );
      Services.obs.addObserver(this, "toggle-screenshot-disable");
    },
    observe(subj, topic, data) {
      let document = subj.document;
      let button = document.getElementById("screenshot-button");

      if (!button) {
        return;
      }

      if (data == "true") {
        button.setAttribute("disabled", "true");
      } else {
        button.removeAttribute("disabled");
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
      let promise = lazy.Sanitizer.sanitize(itemsToClear, {
        ignoreTimespan: false,
        range: lazy.Sanitizer.getClearRange(+group.value),
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
    l10nId: "toolbar-button-new-private-window",
    shortcutId: "key_privatebrowsing",
    onCommand(e) {
      let win = e.target.ownerGlobal;
      win.OpenBrowserWindow({ private: true });
    },
  });
}
