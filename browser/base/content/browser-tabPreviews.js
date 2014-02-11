/*
#ifdef 0
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
#endif
 */

/**
 * Tab previews utility, produces thumbnails
 */
var tabPreviews = {
  aspectRatio: 0.5625, // 16:9

  get width() {
    delete this.width;
    return this.width = Math.ceil(screen.availWidth / 5.75);
  },

  get height() {
    delete this.height;
    return this.height = Math.round(this.width * this.aspectRatio);
  },

  init: function tabPreviews_init() {
    if (this._selectedTab)
      return;
    this._selectedTab = gBrowser.selectedTab;

    gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    gBrowser.tabContainer.addEventListener("SSTabRestored", this, false);
  },

  get: function tabPreviews_get(aTab) {
    let uri = aTab.linkedBrowser.currentURI.spec;

    if (aTab.__thumbnail_lastURI &&
        aTab.__thumbnail_lastURI != uri) {
      aTab.__thumbnail = null;
      aTab.__thumbnail_lastURI = null;
    }

    if (aTab.__thumbnail)
      return aTab.__thumbnail;

    if (aTab.getAttribute("pending") == "true") {
      let img = new Image;
      img.src = PageThumbs.getThumbnailURL(uri);
      return img;
    }

    return this.capture(aTab, !aTab.hasAttribute("busy"));
  },

  capture: function tabPreviews_capture(aTab, aStore) {
    var thumbnail = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    thumbnail.mozOpaque = true;
    thumbnail.height = this.height;
    thumbnail.width = this.width;

    var ctx = thumbnail.getContext("2d");
    var win = aTab.linkedBrowser.contentWindow;
    var snippetWidth = win.innerWidth * .6;
    var scale = this.width / snippetWidth;
    ctx.scale(scale, scale);
    ctx.drawWindow(win, win.scrollX, win.scrollY,
                   snippetWidth, snippetWidth * this.aspectRatio, "rgb(255,255,255)");

    if (aStore &&
        aTab.linkedBrowser /* bug 795608: the tab may got removed while drawing the thumbnail */) {
      aTab.__thumbnail = thumbnail;
      aTab.__thumbnail_lastURI = aTab.linkedBrowser.currentURI.spec;
    }

    return thumbnail;
  },

  handleEvent: function tabPreviews_handleEvent(event) {
    switch (event.type) {
      case "TabSelect":
        if (this._selectedTab &&
            this._selectedTab.parentNode &&
            !this._pendingUpdate) {
          // Generate a thumbnail for the tab that was selected.
          // The timeout keeps the UI snappy and prevents us from generating thumbnails
          // for tabs that will be closed. During that timeout, don't generate other
          // thumbnails in case multiple TabSelect events occur fast in succession.
          this._pendingUpdate = true;
          setTimeout(function (self, aTab) {
            self._pendingUpdate = false;
            if (aTab.parentNode &&
                !aTab.hasAttribute("busy") &&
                !aTab.hasAttribute("pending"))
              self.capture(aTab, true);
          }, 2000, this, this._selectedTab);
        }
        this._selectedTab = event.target;
        break;
      case "SSTabRestored":
        this.capture(event.target, true);
        break;
    }
  }
};

var tabPreviewPanelHelper = {
  opening: function (host) {
    host.panel.hidden = false;

    var handler = this._generateHandler(host);
    host.panel.addEventListener("popupshown", handler, false);
    host.panel.addEventListener("popuphiding", handler, false);

    host._prevFocus = document.commandDispatcher.focusedElement;
  },
  _generateHandler: function (host) {
    var self = this;
    return function (event) {
      if (event.target == host.panel) {
        host.panel.removeEventListener(event.type, arguments.callee, false);
        self["_" + event.type](host);
      }
    };
  },
  _popupshown: function (host) {
    if ("setupGUI" in host)
      host.setupGUI();
  },
  _popuphiding: function (host) {
    if ("suspendGUI" in host)
      host.suspendGUI();

    if (host._prevFocus) {
      Cc["@mozilla.org/focus-manager;1"]
        .getService(Ci.nsIFocusManager)
        .setFocus(host._prevFocus, Ci.nsIFocusManager.FLAG_NOSCROLL);
      host._prevFocus = null;
    } else
      gBrowser.selectedBrowser.focus();

    if (host.tabToSelect) {
      gBrowser.selectedTab = host.tabToSelect;
      host.tabToSelect = null;
    }
  }
};

/**
 * Ctrl-Tab panel
 */
var ctrlTab = {
  get panel () {
    delete this.panel;
    return this.panel = document.getElementById("ctrlTab-panel");
  },
  get showAllButton () {
    delete this.showAllButton;
    return this.showAllButton = document.getElementById("ctrlTab-showAll");
  },
  get previews () {
    delete this.previews;
    return this.previews = this.panel.getElementsByClassName("ctrlTab-preview");
  },
  get recentlyUsedLimit () {
    delete this.recentlyUsedLimit;
    return this.recentlyUsedLimit = gPrefService.getIntPref("browser.ctrlTab.recentlyUsedLimit");
  },
  get keys () {
    var keys = {};
    ["close", "find", "selectAll"].forEach(function (key) {
      keys[key] = document.getElementById("key_" + key)
                          .getAttribute("key")
                          .toLocaleLowerCase().charCodeAt(0);
    });
    delete this.keys;
    return this.keys = keys;
  },
  _selectedIndex: 0,
  get selected () this._selectedIndex < 0 ?
                    document.activeElement :
                    this.previews.item(this._selectedIndex),
  get isOpen   () this.panel.state == "open" || this.panel.state == "showing" || this._timer,
  get tabCount () this.tabList.length,
  get tabPreviewCount () Math.min(this.previews.length - 1, this.tabCount),
  get canvasWidth () Math.min(tabPreviews.width,
                              Math.ceil(screen.availWidth * .85 / this.tabPreviewCount)),
  get canvasHeight () Math.round(this.canvasWidth * tabPreviews.aspectRatio),

  get tabList () {
    if (this._tabList)
      return this._tabList;

    // Using gBrowser.tabs instead of gBrowser.visibleTabs, as the latter
    // exlcudes closing tabs, breaking the following loop in case the the
    // selected tab is closing.
    let list = Array.filter(gBrowser.tabs, function (tab) !tab.hidden);

    // Rotate the list until the selected tab is first
    while (!list[0].selected)
      list.push(list.shift());

    list = list.filter(function (tab) !tab.closing);

    if (this.recentlyUsedLimit != 0) {
      let recentlyUsedTabs = this._recentlyUsedTabs;
      if (this.recentlyUsedLimit > 0)
        recentlyUsedTabs = this._recentlyUsedTabs.slice(0, this.recentlyUsedLimit);
      for (let i = recentlyUsedTabs.length - 1; i >= 0; i--) {
        list.splice(list.indexOf(recentlyUsedTabs[i]), 1);
        list.unshift(recentlyUsedTabs[i]);
      }
    }

    return this._tabList = list;
  },

  init: function ctrlTab_init() {
    if (!this._recentlyUsedTabs) {
      tabPreviews.init();

      this._initRecentlyUsedTabs();
      this._init(true);
    }
  },

  uninit: function ctrlTab_uninit() {
    this._recentlyUsedTabs = null;
    this._init(false);
  },

  prefName: "browser.ctrlTab.previews",
  readPref: function ctrlTab_readPref() {
    var enable =
      gPrefService.getBoolPref(this.prefName) &&
      (!gPrefService.prefHasUserValue("browser.ctrlTab.disallowForScreenReaders") ||
       !gPrefService.getBoolPref("browser.ctrlTab.disallowForScreenReaders"));

    if (enable)
      this.init();
    else
      this.uninit();
  },
  observe: function (aSubject, aTopic, aPrefName) {
    this.readPref();
  },

  updatePreviews: function ctrlTab_updatePreviews() {
    for (let i = 0; i < this.previews.length; i++)
      this.updatePreview(this.previews[i], this.tabList[i]);

    var showAllLabel = gNavigatorBundle.getString("ctrlTab.showAll.label");
    this.showAllButton.label =
      PluralForm.get(this.tabCount, showAllLabel).replace("#1", this.tabCount);
    this.showAllButton.hidden = !allTabs.canOpen;
  },

  updatePreview: function ctrlTab_updatePreview(aPreview, aTab) {
    if (aPreview == this.showAllButton)
      return;

    aPreview._tab = aTab;

    if (aPreview.firstChild)
      aPreview.removeChild(aPreview.firstChild);
    if (aTab) {
      let canvasWidth = this.canvasWidth;
      let canvasHeight = this.canvasHeight;
      aPreview.appendChild(tabPreviews.get(aTab));
      aPreview.setAttribute("label", aTab.label);
      aPreview.setAttribute("tooltiptext", aTab.label);
      aPreview.setAttribute("crop", aTab.crop);
      aPreview.setAttribute("canvaswidth", canvasWidth);
      aPreview.setAttribute("canvasstyle",
                            "max-width:" + canvasWidth + "px;" +
                            "min-width:" + canvasWidth + "px;" +
                            "max-height:" + canvasHeight + "px;" +
                            "min-height:" + canvasHeight + "px;");
      if (aTab.image)
        aPreview.setAttribute("image", aTab.image);
      else
        aPreview.removeAttribute("image");
      aPreview.hidden = false;
    } else {
      aPreview.hidden = true;
      aPreview.removeAttribute("label");
      aPreview.removeAttribute("tooltiptext");
      aPreview.removeAttribute("image");
    }
  },

  advanceFocus: function ctrlTab_advanceFocus(aForward) {
    let selectedIndex = Array.indexOf(this.previews, this.selected);
    do {
      selectedIndex += aForward ? 1 : -1;
      if (selectedIndex < 0)
        selectedIndex = this.previews.length - 1;
      else if (selectedIndex >= this.previews.length)
        selectedIndex = 0;
    } while (this.previews[selectedIndex].hidden);

    if (this._selectedIndex == -1) {
      // Focus is already in the panel.
      this.previews[selectedIndex].focus();
    } else {
      this._selectedIndex = selectedIndex;
    }

    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
      this._openPanel();
    }
  },

  _mouseOverFocus: function ctrlTab_mouseOverFocus(aPreview) {
    if (this._trackMouseOver)
      aPreview.focus();
  },

  pick: function ctrlTab_pick(aPreview) {
    if (!this.tabCount)
      return;

    var select = (aPreview || this.selected);

    if (select == this.showAllButton)
      this.showAllTabs();
    else
      this.close(select._tab);
  },

  showAllTabs: function ctrlTab_showAllTabs(aPreview) {
    this.close();
    document.getElementById("Browser:ShowAllTabs").doCommand();
  },

  remove: function ctrlTab_remove(aPreview) {
    if (aPreview._tab)
      gBrowser.removeTab(aPreview._tab);
  },

  attachTab: function ctrlTab_attachTab(aTab, aPos) {
    if (aPos == 0)
      this._recentlyUsedTabs.unshift(aTab);
    else if (aPos)
      this._recentlyUsedTabs.splice(aPos, 0, aTab);
    else
      this._recentlyUsedTabs.push(aTab);
  },
  detachTab: function ctrlTab_detachTab(aTab) {
    var i = this._recentlyUsedTabs.indexOf(aTab);
    if (i >= 0)
      this._recentlyUsedTabs.splice(i, 1);
  },

  open: function ctrlTab_open() {
    if (this.isOpen)
      return;

    document.addEventListener("keyup", this, true);

    this.updatePreviews();
    this._selectedIndex = 1;

    // Add a slight delay before showing the UI, so that a quick
    // "ctrl-tab" keypress just flips back to the MRU tab.
    this._timer = setTimeout(function (self) {
      self._timer = null;
      self._openPanel();
    }, 200, this);
  },

  _openPanel: function ctrlTab_openPanel() {
    tabPreviewPanelHelper.opening(this);

    this.panel.width = Math.min(screen.availWidth * .99,
                                this.canvasWidth * 1.25 * this.tabPreviewCount);
    var estimateHeight = this.canvasHeight * 1.25 + 75;
    this.panel.openPopupAtScreen(screen.availLeft + (screen.availWidth - this.panel.width) / 2,
                                 screen.availTop + (screen.availHeight - estimateHeight) / 2,
                                 false);
  },

  close: function ctrlTab_close(aTabToSelect) {
    if (!this.isOpen)
      return;

    if (this._timer) {
      clearTimeout(this._timer);
      this._timer = null;
      this.suspendGUI();
      if (aTabToSelect)
        gBrowser.selectedTab = aTabToSelect;
      return;
    }

    this.tabToSelect = aTabToSelect;
    this.panel.hidePopup();
  },

  setupGUI: function ctrlTab_setupGUI() {
    this.selected.focus();
    this._selectedIndex = -1;

    // Track mouse movement after a brief delay so that the item that happens
    // to be under the mouse pointer initially won't be selected unintentionally.
    this._trackMouseOver = false;
    setTimeout(function (self) {
      if (self.isOpen)
        self._trackMouseOver = true;
    }, 0, this);
  },

  suspendGUI: function ctrlTab_suspendGUI() {
    document.removeEventListener("keyup", this, true);

    Array.forEach(this.previews, function (preview) {
      this.updatePreview(preview, null);
    }, this);

    this._tabList = null;
  },

  onKeyPress: function ctrlTab_onKeyPress(event) {
    var isOpen = this.isOpen;

    if (isOpen) {
      event.preventDefault();
      event.stopPropagation();
    }

    switch (event.keyCode) {
      case event.DOM_VK_TAB:
        if (event.ctrlKey && !event.altKey && !event.metaKey) {
          if (isOpen) {
            this.advanceFocus(!event.shiftKey);
          } else if (!event.shiftKey) {
            event.preventDefault();
            event.stopPropagation();
            let tabs = gBrowser.visibleTabs;
            if (tabs.length > 2) {
              this.open();
            } else if (tabs.length == 2) {
              let index = tabs[0].selected ? 1 : 0;
              gBrowser.selectedTab = tabs[index];
            }
          }
        }
        break;
      default:
        if (isOpen && event.ctrlKey) {
          if (event.keyCode == event.DOM_VK_DELETE) {
            this.remove(this.selected);
            break;
          }
          switch (event.charCode) {
            case this.keys.close:
              this.remove(this.selected);
              break;
            case this.keys.find:
            case this.keys.selectAll:
              this.showAllTabs();
              break;
          }
        }
    }
  },

  removeClosingTabFromUI: function ctrlTab_removeClosingTabFromUI(aTab) {
    if (this.tabCount == 2) {
      this.close();
      return;
    }

    this._tabList = null;
    this.updatePreviews();

    if (this.selected.hidden)
      this.advanceFocus(false);
    if (this.selected == this.showAllButton)
      this.advanceFocus(false);

    // If the current tab is removed, another tab can steal our focus.
    if (aTab.selected && this.panel.state == "open") {
      setTimeout(function (selected) {
        selected.focus();
      }, 0, this.selected);
    }
  },

  handleEvent: function ctrlTab_handleEvent(event) {
    switch (event.type) {
      case "SSWindowStateReady":
        this._initRecentlyUsedTabs();
        break;
      case "TabAttrModified":
        // tab attribute modified (e.g. label, crop, busy, image, selected)
        for (let i = this.previews.length - 1; i >= 0; i--) {
          if (this.previews[i]._tab && this.previews[i]._tab == event.target) {
            this.updatePreview(this.previews[i], event.target);
            break;
          }
        }
        break;
      case "TabSelect":
        this.detachTab(event.target);
        this.attachTab(event.target, 0);
        break;
      case "TabOpen":
        this.attachTab(event.target, 1);
        break;
      case "TabClose":
        this.detachTab(event.target);
        if (this.isOpen)
          this.removeClosingTabFromUI(event.target);
        break;
      case "keypress":
        this.onKeyPress(event);
        break;
      case "keyup":
        if (event.keyCode == event.DOM_VK_CONTROL)
          this.pick();
        break;
      case "popupshowing":
        if (event.target.id == "menu_viewPopup")
          document.getElementById("menu_showAllTabs").hidden = !allTabs.canOpen;
        break;
    }
  },

  _initRecentlyUsedTabs: function () {
    this._recentlyUsedTabs =
      Array.filter(gBrowser.tabs, tab => !tab.closing)
           .sort((tab1, tab2) => tab2.lastAccessed - tab1.lastAccessed);
  },

  _init: function ctrlTab__init(enable) {
    var toggleEventListener = enable ? "addEventListener" : "removeEventListener";

    window[toggleEventListener]("SSWindowStateReady", this, false);

    var tabContainer = gBrowser.tabContainer;
    tabContainer[toggleEventListener]("TabOpen", this, false);
    tabContainer[toggleEventListener]("TabAttrModified", this, false);
    tabContainer[toggleEventListener]("TabSelect", this, false);
    tabContainer[toggleEventListener]("TabClose", this, false);

    document[toggleEventListener]("keypress", this, false);
    gBrowser.mTabBox.handleCtrlTab = !enable;

    // If we're not running, hide the "Show All Tabs" menu item,
    // as Shift+Ctrl+Tab will be handled by the tab bar.
    document.getElementById("menu_showAllTabs").hidden = !enable;
    document.getElementById("menu_viewPopup")[toggleEventListener]("popupshowing", this);

    // Also disable the <key> to ensure Shift+Ctrl+Tab never triggers
    // Show All Tabs.
    var key_showAllTabs = document.getElementById("key_showAllTabs");
    if (enable)
      key_showAllTabs.removeAttribute("disabled");
    else
      key_showAllTabs.setAttribute("disabled", "true");
  }
};


/**
 * All Tabs menu
 */
var allTabs = {
  get toolbarButton() document.getElementById("alltabs-button"),
  get canOpen() isElementVisible(this.toolbarButton),

  open: function allTabs_open() {
    if (this.canOpen) {
      // Without setTimeout, the menupopup won't stay open when invoking
      // "View > Show All Tabs" and the menu bar auto-hides.
      setTimeout(function () {
        allTabs.toolbarButton.open = true;
      }, 0);
    }
  }
};
