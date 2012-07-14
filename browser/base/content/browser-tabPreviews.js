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
  init: function tabPreviews_init() {
    if (this._selectedTab)
      return;
    this._selectedTab = gBrowser.selectedTab;

    this.width = Math.ceil(screen.availWidth / 5.75);
    this.height = Math.round(this.width * this.aspectRatio);

    window.addEventListener("unload", this, false);
    gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    gBrowser.tabContainer.addEventListener("SSTabRestored", this, false);
  },
  uninit: function tabPreviews_uninit() {
    window.removeEventListener("unload", this, false);
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", this, false);
    this._selectedTab = null;
  },
  get: function tabPreviews_get(aTab) {
    this.init();

    if (aTab.__thumbnail_lastURI &&
        aTab.__thumbnail_lastURI != aTab.linkedBrowser.currentURI.spec) {
      aTab.__thumbnail = null;
      aTab.__thumbnail_lastURI = null;
    }
    return aTab.__thumbnail || this.capture(aTab, !aTab.hasAttribute("busy"));
  },
  capture: function tabPreviews_capture(aTab, aStore) {
    this.init();

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

    if (aStore) {
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
            if (aTab.parentNode && !aTab.hasAttribute("busy"))
              self.capture(aTab, true);
          }, 2000, this, this._selectedTab);
        }
        this._selectedTab = event.target;
        break;
      case "SSTabRestored":
        this.capture(event.target, true);
        break;
      case "unload":
        this.uninit();
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

      this._recentlyUsedTabs = [gBrowser.selectedTab];
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
    if (this._selectedIndex == -1) {
      // No virtual selectedIndex, focus must be in the panel already.
      if (aForward)
        document.commandDispatcher.advanceFocus();
      else
        document.commandDispatcher.rewindFocus();
    } else {
      // Focus isn't in the panel yet, so we maintain a virtual selectedIndex.
      do {
        this._selectedIndex += aForward ? 1 : -1;
        if (this._selectedIndex < 0)
          this._selectedIndex = this.previews.length - 1;
        else if (this._selectedIndex >= this.previews.length)
          this._selectedIndex = 0;
      } while (this.selected.hidden);
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

    allTabs.close();

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
              let index = gBrowser.selectedTab == tabs[0] ? 1 : 0;
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
    if (aTab == gBrowser.selectedTab && this.panel.state == "open") {
      setTimeout(function (selected) {
        selected.focus();
      }, 0, this.selected);
    }
  },

  handleEvent: function ctrlTab_handleEvent(event) {
    switch (event.type) {
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
    }
  },

  _init: function ctrlTab__init(enable) {
    var toggleEventListener = enable ? "addEventListener" : "removeEventListener";

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
 * All Tabs panel
 */
var allTabs = {
  get panel () {
    delete this.panel;
    return this.panel = document.getElementById("allTabs-panel");
  },
  get filterField () {
    delete this.filterField;
    return this.filterField = document.getElementById("allTabs-filter");
  },
  get container () {
    delete this.container;
    return this.container = document.getElementById("allTabs-container");
  },
  get tabCloseButton () {
    delete this.tabCloseButton;
    return this.tabCloseButton = document.getElementById("allTabs-tab-close-button");
  },
  get toolbarButton() document.getElementById("alltabs-button"),
  get previews () this.container.getElementsByClassName("allTabs-preview"),
  get isOpen () this.panel.state == "open" || this.panel.state == "showing",

  init: function allTabs_init() {
    if (this._initiated)
      return;
    this._initiated = true;

    tabPreviews.init();

    Array.forEach(gBrowser.tabs, function (tab) {
      this._addPreview(tab);
    }, this);

    gBrowser.tabContainer.addEventListener("TabOpen", this, false);
    gBrowser.tabContainer.addEventListener("TabAttrModified", this, false);
    gBrowser.tabContainer.addEventListener("TabMove", this, false);
    gBrowser.tabContainer.addEventListener("TabClose", this, false);
  },

  uninit: function allTabs_uninit() {
    if (!this._initiated)
      return;

    gBrowser.tabContainer.removeEventListener("TabOpen", this, false);
    gBrowser.tabContainer.removeEventListener("TabAttrModified", this, false);
    gBrowser.tabContainer.removeEventListener("TabMove", this, false);
    gBrowser.tabContainer.removeEventListener("TabClose", this, false);

    while (this.container.hasChildNodes())
      this.container.removeChild(this.container.firstChild);

    this._initiated = false;
  },

  prefName: "browser.allTabs.previews",
  readPref: function allTabs_readPref() {
    var allTabsButton = this.toolbarButton;
    if (!allTabsButton)
      return;

    if (gPrefService.getBoolPref(this.prefName)) {
      allTabsButton.removeAttribute("type");
      allTabsButton.setAttribute("command", "Browser:ShowAllTabs");
    } else {
      allTabsButton.setAttribute("type", "menu");
      allTabsButton.removeAttribute("command");
      allTabsButton.removeAttribute("oncommand");
    }
  },
  observe: function (aSubject, aTopic, aPrefName) {
    this.readPref();
  },

  pick: function allTabs_pick(aPreview) {
    if (!aPreview)
      aPreview = this._firstVisiblePreview;
    if (aPreview)
      this.tabToSelect = aPreview._tab;

    this.close();
  },

  closeTab: function allTabs_closeTab(event) {
    this.filterField.focus();
    gBrowser.removeTab(event.currentTarget._targetPreview._tab);
  },

  filter: function allTabs_filter() {
    if (this._currentFilter == this.filterField.value)
      return;

    this._currentFilter = this.filterField.value;

    var filter = this._currentFilter.split(/\s+/g);
    this._visible = 0;
    Array.forEach(this.previews, function (preview) {
      var tab = preview._tab;
      var matches = 0;
      if (filter.length && !tab.hidden) {
        let tabstring = tab.linkedBrowser.currentURI.spec;
        try {
          tabstring = decodeURI(tabstring);
        } catch (e) {}
        tabstring = tab.label + " " + tab.label.toLocaleLowerCase() + " " + tabstring;
        for (let i = 0; i < filter.length; i++)
          matches += tabstring.indexOf(filter[i]) > -1;
      }
      if (matches < filter.length || tab.hidden) {
        preview.hidden = true;
      }
      else {
        this._visible++;
        this._updatePreview(preview);
        preview.hidden = false;
      }
    }, this);

    this._reflow();
  },

  open: function allTabs_open() {
    var allTabsButton = this.toolbarButton;
    if (allTabsButton &&
        allTabsButton.getAttribute("type") == "menu") {
      // Without setTimeout, the menupopup won't stay open when invoking
      // "View > Show All Tabs" and the menu bar auto-hides.
      setTimeout(function () {
        allTabsButton.open = true;
      }, 0);
      return;
    }

    this.init();

    if (this.isOpen)
      return;

    this._maxPanelHeight = Math.max(gBrowser.clientHeight, screen.availHeight / 2);
    this._maxPanelWidth = Math.max(gBrowser.clientWidth, screen.availWidth / 2);

    this.filter();

    tabPreviewPanelHelper.opening(this);

    this.panel.popupBoxObject.setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_NO_CONSUME);
    this.panel.openPopup(gBrowser, "overlap", 0, 0, false, true);
  },

  close: function allTabs_close() {
    this.panel.hidePopup();
  },

  setupGUI: function allTabs_setupGUI() {
    this.filterField.focus();
    this.filterField.placeholder = this.filterField.tooltipText;

    this.panel.addEventListener("keypress", this, false);
    this.panel.addEventListener("keypress", this, true);
    this._browserCommandSet.addEventListener("command", this, false);

    // When the panel is open, a second click on the all tabs button should
    // close the panel but not re-open it.
    document.getElementById("Browser:ShowAllTabs").setAttribute("disabled", "true");
  },

  suspendGUI: function allTabs_suspendGUI() {
    this.filterField.placeholder = "";
    this.filterField.value = "";
    this._currentFilter = null;

    this._updateTabCloseButton();

    this.panel.removeEventListener("keypress", this, false);
    this.panel.removeEventListener("keypress", this, true);
    this._browserCommandSet.removeEventListener("command", this, false);

    setTimeout(function () {
      document.getElementById("Browser:ShowAllTabs").removeAttribute("disabled");
    }, 300);
  },

  handleEvent: function allTabs_handleEvent(event) {
    if (/^Tab/.test(event.type)) {
      var tab = event.target;
      if (event.type != "TabOpen")
        var preview = this._getPreview(tab);
    }
    switch (event.type) {
      case "TabAttrModified":
        // tab attribute modified (e.g. label, crop, busy, image)
        if (!preview.hidden)
          this._updatePreview(preview);
        break;
      case "TabOpen":
        if (this.isOpen)
          this.close();
        this._addPreview(tab);
        break;
      case "TabMove":
        let siblingPreview = tab.nextSibling &&
                             this._getPreview(tab.nextSibling);
        if (siblingPreview)
          siblingPreview.parentNode.insertBefore(preview, siblingPreview);
        else
          this.container.lastChild.appendChild(preview);
        if (this.isOpen && !preview.hidden) {
          this._reflow();
          preview.focus();
        }
        break;
      case "TabClose":
        this._removePreview(preview);
        break;
      case "keypress":
        this._onKeyPress(event);
        break;
      case "command":
        if (event.target.id != "Browser:ShowAllTabs") {
          // Close the panel when there's a browser command executing in the background.
          this.close();
        }
        break;
    }
  },

  _visible: 0,
  _currentFilter: null,
  get _stack () {
    delete this._stack;
    return this._stack = document.getElementById("allTabs-stack");
  },
  get _browserCommandSet () {
    delete this._browserCommandSet;
    return this._browserCommandSet = document.getElementById("mainCommandSet");
  },
  get _previewLabelHeight () {
    delete this._previewLabelHeight;
    return this._previewLabelHeight = parseInt(getComputedStyle(this.previews[0], "").lineHeight);
  },

  get _visiblePreviews ()
    Array.filter(this.previews, function (preview) !preview.hidden),

  get _firstVisiblePreview () {
    if (this._visible == 0)
      return null;
    var previews = this.previews;
    for (let i = 0; i < previews.length; i++) {
      if (!previews[i].hidden)
        return previews[i];
    }
    return null;
  },

  _reflow: function allTabs_reflow() {
    this._updateTabCloseButton();

    const CONTAINER_MAX_WIDTH = this._maxPanelWidth * .95;
    const CONTAINER_MAX_HEIGHT = this._maxPanelHeight - 35;
    // the size of the whole preview relative to the thumbnail
    const REL_PREVIEW_THUMBNAIL = 1.2;
    const REL_PREVIEW_HEIGHT_WIDTH = tabPreviews.height / tabPreviews.width;
    const PREVIEW_MAX_WIDTH = tabPreviews.width * REL_PREVIEW_THUMBNAIL;

    var rows, previewHeight, previewWidth, outerHeight;
    this._columns = Math.floor(CONTAINER_MAX_WIDTH / PREVIEW_MAX_WIDTH);
    do {
      rows = Math.ceil(this._visible / this._columns);
      previewWidth = Math.min(PREVIEW_MAX_WIDTH,
                              Math.round(CONTAINER_MAX_WIDTH / this._columns));
      previewHeight = Math.round(previewWidth * REL_PREVIEW_HEIGHT_WIDTH);
      outerHeight = previewHeight + this._previewLabelHeight;
    } while (rows * outerHeight > CONTAINER_MAX_HEIGHT && ++this._columns);

    var outerWidth = previewWidth;
    {
      let innerWidth = Math.ceil(previewWidth / REL_PREVIEW_THUMBNAIL);
      let innerHeight = Math.ceil(previewHeight / REL_PREVIEW_THUMBNAIL);
      var canvasStyle = "max-width:" + innerWidth + "px;" +
                        "min-width:" + innerWidth + "px;" +
                        "max-height:" + innerHeight + "px;" +
                        "min-height:" + innerHeight + "px;";
    }

    var previews = Array.slice(this.previews);

    while (this.container.hasChildNodes())
      this.container.removeChild(this.container.firstChild);
    for (let i = rows || 1; i > 0; i--)
      this.container.appendChild(document.createElement("hbox"));

    var row = this.container.firstChild;
    var colCount = 0;
    previews.forEach(function (preview) {
      if (!preview.hidden &&
          ++colCount > this._columns) {
        row = row.nextSibling;
        colCount = 1;
      }
      preview.setAttribute("minwidth", outerWidth);
      preview.setAttribute("height", outerHeight);
      preview.setAttribute("canvasstyle", canvasStyle);
      preview.removeAttribute("closebuttonhover");
      row.appendChild(preview);
    }, this);

    this._stack.width = this._maxPanelWidth;
    this.container.width = Math.ceil(outerWidth * Math.min(this._columns, this._visible));
    this.container.left = Math.round((this._maxPanelWidth - this.container.width) / 2);
    this.container.maxWidth = this._maxPanelWidth - this.container.left;
    this.container.maxHeight = rows * outerHeight;
  },

  _addPreview: function allTabs_addPreview(aTab) {
    var preview = document.createElement("button");
    preview.className = "allTabs-preview";
    preview._tab = aTab;
    this.container.lastChild.appendChild(preview);
  },

  _removePreview: function allTabs_removePreview(aPreview) {
    var updateUI = (this.isOpen && !aPreview.hidden);
    aPreview._tab = null;
    aPreview.parentNode.removeChild(aPreview);
    if (updateUI) {
      this._visible--;
      this._reflow();
      this.filterField.focus();
    }
  },

  _getPreview: function allTabs_getPreview(aTab) {
    var previews = this.previews;
    for (let i = 0; i < previews.length; i++)
      if (previews[i]._tab == aTab)
        return previews[i];
    return null;
  },

  _updateTabCloseButton: function allTabs_updateTabCloseButton(event) {
    if (event && event.target == this.tabCloseButton)
      return;

    if (this.tabCloseButton._targetPreview) {
      if (event && event.target == this.tabCloseButton._targetPreview)
        return;

      this.tabCloseButton._targetPreview.removeAttribute("closebuttonhover");
    }

    if (event &&
        event.target.parentNode.parentNode == this.container &&
        (event.target._tab.previousSibling || event.target._tab.nextSibling)) {
      let canvas = event.target.firstChild.getBoundingClientRect();
      let container = this.container.getBoundingClientRect();
      let tabCloseButton = this.tabCloseButton.getBoundingClientRect();
      let alignLeft = getComputedStyle(this.panel, "").direction == "rtl";
#ifdef XP_MACOSX
      alignLeft = !alignLeft;
#endif
      this.tabCloseButton.left = canvas.left -
                                 container.left +
                                 parseInt(this.container.left) +
                                 (alignLeft ? 0 :
                                  canvas.width - tabCloseButton.width);
      this.tabCloseButton.top = canvas.top - container.top;
      this.tabCloseButton._targetPreview = event.target;
      this.tabCloseButton.style.visibility = "visible";
      event.target.setAttribute("closebuttonhover", "true");
    } else {
      this.tabCloseButton.style.visibility = "hidden";
      this.tabCloseButton.left = this.tabCloseButton.top = 0;
      this.tabCloseButton._targetPreview = null;
    }
  },

  _updatePreview: function allTabs_updatePreview(aPreview) {
    aPreview.setAttribute("label", aPreview._tab.label);
    aPreview.setAttribute("tooltiptext", aPreview._tab.label);
    aPreview.setAttribute("crop", aPreview._tab.crop);
    if (aPreview._tab.image)
      aPreview.setAttribute("image", aPreview._tab.image);
    else
      aPreview.removeAttribute("image");

    var thumbnail = tabPreviews.get(aPreview._tab);
    if (aPreview.firstChild) {
      if (aPreview.firstChild == thumbnail)
        return;
      aPreview.removeChild(aPreview.firstChild);
    }
    aPreview.appendChild(thumbnail);
  },

  _onKeyPress: function allTabs_onKeyPress(event) {
    if (event.eventPhase == event.CAPTURING_PHASE) {
      this._onCapturingKeyPress(event);
      return;
    }

    if (event.keyCode == event.DOM_VK_ESCAPE) {
      this.close();
      event.preventDefault();
      event.stopPropagation();
      return;
    }

    if (event.target == this.filterField) {
      switch (event.keyCode) {
        case event.DOM_VK_UP:
          if (this._visible) {
            let previews = this._visiblePreviews;
            let columns = Math.min(previews.length, this._columns);
            previews[Math.floor(previews.length / columns) * columns - 1].focus();
            event.preventDefault();
            event.stopPropagation();
          }
          break;
        case event.DOM_VK_DOWN:
          if (this._visible) {
            this._firstVisiblePreview.focus();
            event.preventDefault();
            event.stopPropagation();
          }
          break;
      }
    }
  },

  _onCapturingKeyPress: function allTabs_onCapturingKeyPress(event) {
    switch (event.keyCode) {
      case event.DOM_VK_UP:
      case event.DOM_VK_DOWN:
        if (event.target != this.filterField)
          this._advanceFocusVertically(event);
        break;
      case event.DOM_VK_RETURN:
        if (event.target == this.filterField) {
          this.filter();
          this.pick();
          event.preventDefault();
          event.stopPropagation();
        }
        break;
    }
  },

  _advanceFocusVertically: function allTabs_advanceFocusVertically(event) {
    var preview = document.activeElement;
    if (!preview || preview.parentNode.parentNode != this.container)
      return;

    event.stopPropagation();

    var up = (event.keyCode == event.DOM_VK_UP);
    var previews = this._visiblePreviews;

    if (up && preview == previews[0]) {
      this.filterField.focus();
      return;
    }

    var i       = previews.indexOf(preview);
    var columns = Math.min(previews.length, this._columns);
    var column  = i % columns;
    var row     = Math.floor(i / columns);

    function newIndex()    row * columns + column;
    function outOfBounds() newIndex() >= previews.length;

    if (up) {
      row--;
      if (row < 0) {
        let rows = Math.ceil(previews.length / columns);
        row = rows - 1;
        column--;
        if (outOfBounds())
          row--;
      }
    } else {
      row++;
      if (outOfBounds()) {
        if (column == columns - 1) {
          this.filterField.focus();
          return;
        }
        row = 0;
        column++;
      }
    }
    previews[newIndex()].focus();
  }
};
