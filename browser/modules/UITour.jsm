// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

this.EXPORTED_SYMBOLS = ["UITour"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "LightweightThemeManager",
  "resource://gre/modules/LightweightThemeManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PermissionsUtils",
  "resource://gre/modules/PermissionsUtils.jsm");


const UITOUR_PERMISSION   = "uitour";
const PREF_PERM_BRANCH    = "browser.uitour.";


this.UITour = {
  originTabs: new WeakMap(),
  pinnedTabs: new WeakMap(),
  urlbarCapture: new WeakMap(),

  highlightEffects: ["wobble", "zoom", "color"],
  targets: new Map([
    ["backforward", "#unified-back-forward-button"],
    ["appmenu", "#appmenu-button"],
    ["home", "#home-button"],
    ["urlbar", "#urlbar"],
    ["bookmarks", "#bookmarks-menu-button"],
    ["search", "#searchbar"],
    ["searchprovider", function UITour_target_searchprovider(aDocument) {
      let searchbar = aDocument.getElementById("searchbar");
      return aDocument.getAnonymousElementByAttribute(searchbar,
                                                     "anonid",
                                                     "searchbar-engine-button");
    }],
  ]),

  onPageEvent: function(aEvent) {
    let contentDocument = null;
    if (aEvent.target instanceof Ci.nsIDOMHTMLDocument)
      contentDocument = aEvent.target;
    else if (aEvent.target instanceof Ci.nsIDOMHTMLElement)
      contentDocument = aEvent.target.ownerDocument;
    else
      return false;

    // Ignore events if they're not from a trusted origin.
    if (!this.ensureTrustedOrigin(contentDocument))
      return false;

    if (typeof aEvent.detail != "object")
      return false;

    let action = aEvent.detail.action;
    if (typeof action != "string" || !action)
      return false;

    let data = aEvent.detail.data;
    if (typeof data != "object")
      return false;

    let window = this.getChromeWindow(contentDocument);

    switch (action) {
      case "showHighlight": {
        let target = this.getTarget(window, data.target);
        if (!target)
          return false;
        this.showHighlight(target);
        break;
      }

      case "hideHighlight": {
        this.hideHighlight(window);
        break;
      }

      case "showInfo": {
        let target = this.getTarget(window, data.target, true);
        if (!target)
          return false;
        this.showInfo(target, data.title, data.text);
        break;
      }

      case "hideInfo": {
        this.hideInfo(window);
        break;
      }

      case "previewTheme": {
        this.previewTheme(data.theme);
        break;
      }

      case "resetTheme": {
        this.resetTheme();
        break;
      }

      case "addPinnedTab": {
        this.ensurePinnedTab(window, true);
        break;
      }

      case "removePinnedTab": {
        this.removePinnedTab(window);
        break;
      }

      case "showMenu": {
        this.showMenu(window, data.name);
        break;
      }

      case "startUrlbarCapture": {
        if (typeof data.text != "string" || !data.text ||
            typeof data.url != "string" || !data.url) {
          return false;
        }

        let uri = null;
        try {
          uri = Services.io.newURI(data.url, null, null);
        } catch (e) {
          return false;
        }

        let secman = Services.scriptSecurityManager;
        let principal = contentDocument.nodePrincipal;
        let flags = secman.DISALLOW_INHERIT_PRINCIPAL;
        try {
          secman.checkLoadURIWithPrincipal(principal, uri, flags);
        } catch (e) {
          return false;
        }

        this.startUrlbarCapture(window, data.text, data.url);
        break;
      }

      case "endUrlbarCapture": {
        this.endUrlbarCapture(window);
        break;
      }
    }

    let tab = window.gBrowser._getTabForContentWindow(contentDocument.defaultView);
    if (!this.originTabs.has(window))
      this.originTabs.set(window, new Set());
    this.originTabs.get(window).add(tab);

    tab.addEventListener("TabClose", this);
    window.gBrowser.tabContainer.addEventListener("TabSelect", this);
    window.addEventListener("SSWindowClosing", this);

    return true;
  },

  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "pagehide": {
        let window = this.getChromeWindow(aEvent.target);
        this.teardownTour(window);
        break;
      }

      case "TabClose": {
        let window = aEvent.target.ownerDocument.defaultView;
        this.teardownTour(window);
        break;
      }

      case "TabSelect": {
        let window = aEvent.target.ownerDocument.defaultView;
        let pinnedTab = this.pinnedTabs.get(window);
        if (pinnedTab && pinnedTab.tab == window.gBrowser.selectedTab)
          break;
        let originTabs = this.originTabs.get(window);
        if (originTabs && originTabs.has(window.gBrowser.selectedTab))
          break;

        this.teardownTour(window);
        break;
      }

      case "SSWindowClosing": {
        let window = aEvent.target;
        this.teardownTour(window, true);
        break;
      }

      case "input": {
        if (aEvent.target.id == "urlbar") {
          let window = aEvent.target.ownerDocument.defaultView;
          this.handleUrlbarInput(window);
        }
        break;
      }
    }
  },

  teardownTour: function(aWindow, aWindowClosing = false) {
    aWindow.gBrowser.tabContainer.removeEventListener("TabSelect", this);
    aWindow.removeEventListener("SSWindowClosing", this);

    let originTabs = this.originTabs.get(aWindow);
    if (originTabs) {
      for (let tab of originTabs)
        tab.removeEventListener("TabClose", this);
    }
    this.originTabs.delete(aWindow);

    if (!aWindowClosing) {
      this.hideHighlight(aWindow);
      this.hideInfo(aWindow);
    }

    this.endUrlbarCapture(aWindow);
    this.removePinnedTab(aWindow);
    this.resetTheme();
  },

  getChromeWindow: function(aContentDocument) {
    return aContentDocument.defaultView
                           .window
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIWebNavigation)
                           .QueryInterface(Ci.nsIDocShellTreeItem)
                           .rootTreeItem
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindow)
                           .wrappedJSObject;
  },

  importPermissions: function() {
    try {
      PermissionsUtils.importFromPrefs(PREF_PERM_BRANCH, UITOUR_PERMISSION);
    } catch (e) {
      Cu.reportError(e);
    }
  },

  ensureTrustedOrigin: function(aDocument) {
    if (aDocument.defaultView.top != aDocument.defaultView)
      return false;

    let uri = aDocument.documentURIObject;

    if (uri.schemeIs("chrome"))
      return true;

    if (!uri.schemeIs("https"))
      return false;

    this.importPermissions();
    let permission = Services.perms.testPermission(uri, UITOUR_PERMISSION);
    return permission == Services.perms.ALLOW_ACTION;
  },

  getTarget: function(aWindow, aTargetName, aSticky = false) {
    if (typeof aTargetName != "string" || !aTargetName)
      return null;

    if (aTargetName == "pinnedtab")
      return this.ensurePinnedTab(aWindow, aSticky);

    let targetQuery = this.targets.get(aTargetName);
    if (!targetQuery)
      return null;

    if (typeof targetQuery == "function")
      return targetQuery(aWindow.document);

    return aWindow.document.querySelector(targetQuery);
  },

  previewTheme: function(aTheme) {
    let origin = Services.prefs.getCharPref("browser.uitour.themeOrigin");
    let data = LightweightThemeManager.parseTheme(aTheme, origin);
    if (data)
      LightweightThemeManager.previewTheme(data);
  },

  resetTheme: function() {
    LightweightThemeManager.resetPreview();
  },

  ensurePinnedTab: function(aWindow, aSticky = false) {
    let tabInfo = this.pinnedTabs.get(aWindow);

    if (tabInfo) {
      tabInfo.sticky = tabInfo.sticky || aSticky;
    } else {
      let url = Services.urlFormatter.formatURLPref("browser.uitour.pinnedTabUrl");

      let tab = aWindow.gBrowser.addTab(url);
      aWindow.gBrowser.pinTab(tab);
      tab.addEventListener("TabClose", () => {
        this.pinnedTabs.delete(aWindow);
      });

      tabInfo = {
        tab: tab,
        sticky: aSticky
      };
      this.pinnedTabs.set(aWindow, tabInfo);
    }

    return tabInfo.tab;
  },

  removePinnedTab: function(aWindow) {
    let tabInfo = this.pinnedTabs.get(aWindow);
    if (tabInfo)
      aWindow.gBrowser.removeTab(tabInfo.tab);
  },

  showHighlight: function(aTarget) {
    let highlighter = aTarget.ownerDocument.getElementById("UITourHighlight");

    let randomEffect = Math.floor(Math.random() * this.highlightEffects.length);
    if (randomEffect == this.highlightEffects.length)
      randomEffect--; // On the order of 1 in 2^62 chance of this happening.
    highlighter.setAttribute("active", this.highlightEffects[randomEffect]);

    let targetRect = aTarget.getBoundingClientRect();

    highlighter.style.height = targetRect.height + "px";
    highlighter.style.width = targetRect.width + "px";

    let highlighterRect = highlighter.getBoundingClientRect();

    let top = targetRect.top + (targetRect.height / 2) - (highlighterRect.height / 2);
    highlighter.style.top = top + "px";
    let left = targetRect.left + (targetRect.width / 2) - (highlighterRect.width / 2);
    highlighter.style.left = left + "px";
  },

  hideHighlight: function(aWindow) {
    let tabData = this.pinnedTabs.get(aWindow);
    if (tabData && !tabData.sticky)
      this.removePinnedTab(aWindow);

    let highlighter = aWindow.document.getElementById("UITourHighlight");
    highlighter.removeAttribute("active");
  },

  showInfo: function(aAnchor, aTitle, aDescription) {
    aAnchor.focus();

    let document = aAnchor.ownerDocument;
    let tooltip = document.getElementById("UITourTooltip");
    let tooltipTitle = document.getElementById("UITourTooltipTitle");
    let tooltipDesc = document.getElementById("UITourTooltipDescription");

    tooltip.hidePopup();

    tooltipTitle.textContent = aTitle;
    tooltipDesc.textContent = aDescription;

    let alignment = "bottomcenter topright";
    let anchorRect = aAnchor.getBoundingClientRect();

    tooltip.hidden = false;
    tooltip.openPopup(aAnchor, alignment);
  },

  hideInfo: function(aWindow) {
    let tooltip = aWindow.document.getElementById("UITourTooltip");
    tooltip.hidePopup();
  },

  showMenu: function(aWindow, aMenuName) {
    function openMenuButton(aId) {
      let menuBtn = aWindow.document.getElementById(aId);
      if (menuBtn && menuBtn.boxObject)
        menuBtn.boxObject.QueryInterface(Ci.nsIMenuBoxObject).openMenu(true);
    }

    if (aMenuName == "appmenu")
      openMenuButton("appmenu-button");
    else if (aMenuName == "bookmarks")
      openMenuButton("bookmarks-menu-button");
  },

  startUrlbarCapture: function(aWindow, aExpectedText, aUrl) {
    let urlbar = aWindow.document.getElementById("urlbar");
    this.urlbarCapture.set(aWindow, {
      expected: aExpectedText.toLocaleLowerCase(),
      url: aUrl
    });
    urlbar.addEventListener("input", this);
  },

  endUrlbarCapture: function(aWindow) {
    let urlbar = aWindow.document.getElementById("urlbar");
    urlbar.removeEventListener("input", this);
    this.urlbarCapture.delete(aWindow);
  },

  handleUrlbarInput: function(aWindow) {
    if (!this.urlbarCapture.has(aWindow))
      return;

    let urlbar = aWindow.document.getElementById("urlbar");

    let {expected, url} = this.urlbarCapture.get(aWindow);

    if (urlbar.value.toLocaleLowerCase().localeCompare(expected) != 0)
      return;

    urlbar.handleRevert();

    let tab = aWindow.gBrowser.addTab(url, {
      owner: aWindow.gBrowser.selectedTab,
      relatedToCurrent: true
    });
    aWindow.gBrowser.selectedTab = tab;
  },
};
