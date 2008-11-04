/*
#ifdef 0
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Tab Previews.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dão Gottwald <dao@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
#endif
 */

/**
 * Tab previews utility, produces thumbnails
 */
var tabPreviews = {
  aspectRatio: 0.5625, // 16:9
  init: function tabPreviews__init() {
    this.width = Math.ceil(screen.availWidth / 5);
    this.height = Math.round(this.width * this.aspectRatio);

    gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    gBrowser.tabContainer.addEventListener("SSTabRestored", this, false);
  },
  uninit: function tabPreviews__uninit() {
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", this, false);
    this._selectedTab = null;
  },
  get: function tabPreviews__get(aTab) {
    if (aTab.__thumbnail_lastURI &&
        aTab.__thumbnail_lastURI != aTab.linkedBrowser.currentURI.spec) {
      aTab.__thumbnail = null;
      aTab.__thumbnail_lastURI = null;
    }
    return aTab.__thumbnail || this.capture(aTab, !aTab.hasAttribute("busy"));
  },
  capture: function tabPreviews__capture(aTab, aStore) {
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
  handleEvent: function tabPreviews__handleEvent(event) {
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
  get searchField () {
    delete this.searchField;
    return this.searchField = document.getElementById("ctrlTab-search");
  },
  get pagesBar () {
    delete this.pagesBar;
    return this.pagesBar = document.getElementById("ctrlTab-pages");
  },
  get thumbnails () {
    delete this.thumbnails;
    return this.thumbnails = this.panel.getElementsByClassName("ctrlTab-thumbnail");
  },
  get columns () {
    delete this.columns;
    return this.columns = this.thumbnails.length /
                          this.panel.getElementsByClassName("ctrlTab-row").length;
  },
  get closeCharCode () {
    delete this.closeCharCode;
    return this.closeCharCode = document.getElementById("key_close")
                                        .getAttribute("key")
                                        .toLocaleLowerCase().charCodeAt(0);
  },
  get findCharCode () {
    delete this.findCharCode;
    return this.findCharCode = document.getElementById("key_find")
                                       .getAttribute("key")
                                       .toLocaleLowerCase().charCodeAt(0);
  },
  get recentlyUsedLimit () {
    delete this.recentlyUsedLimit;
    return this.recentlyUsedLimit = gPrefService.getIntPref("browser.ctrlTab.recentlyUsedLimit");
  },
  selectedIndex: 0,
  get selected () this.thumbnails.item(this.selectedIndex),
  get isOpen   () this.panel.state == "open" || this.panel.state == "showing",
  get tabCount () this.tabList.length,

  get sticky () this.panel.hasAttribute("sticky"),
  set sticky (val) {
    if (val)
      this.panel.setAttribute("sticky", "true");
    else
      this.panel.removeAttribute("sticky");
    return val;
  },

  get pages () Math.ceil(this.tabCount / this.thumbnails.length),
  get page  () this._page || 0,
  set page  (page) {
    if (page < 0)
      page = this.pages - 1;
    else if (page >= this.pages)
      page = 0;

    if (this.pagesBar.childNodes.length) {
      this.pagesBar.childNodes[this.page].removeAttribute("selected");
      this.pagesBar.childNodes[page].setAttribute("selected", "true");
    }

    this._page = page;
    this.updatePreviews();
    return page;
  },

  get tabList () {
    if (this._tabList)
      return this._tabList;

    var list = Array.slice(gBrowser.mTabs);

    if (this._closing)
      this.detachTab(this._closing, list);

    for (let i = 0; i < gBrowser.tabContainer.selectedIndex; i++)
      list.push(list.shift());

    if (!this._useTabBarOrder && this.recentlyUsedLimit != 0) {
      let recentlyUsedTabs = this._recentlyUsedTabs;
      if (this.recentlyUsedLimit > 0)
        recentlyUsedTabs = this._recentlyUsedTabs.slice(0, this.recentlyUsedLimit);
      for (let i = recentlyUsedTabs.length - 1; i >= 0; i--) {
        list.splice(list.indexOf(recentlyUsedTabs[i]), 1);
        list.unshift(recentlyUsedTabs[i]);
      }
    }

    if (this.searchField.value) {
      list = list.filter(function (tab) {
        let lowerCaseLabel, uri;
        for (let i = 0; i < this.length; i++) {
          if (tab.label.indexOf(this[i]) != -1)
            continue;

          if (!lowerCaseLabel)
            lowerCaseLabel = tab.label.toLocaleLowerCase();
          if (lowerCaseLabel.indexOf(this[i]) != -1)
            continue;

          if (!uri) {
            uri = tab.linkedBrowser.currentURI.spec;
            try {
              uri = decodeURI(uri);
            } catch (e) {}
          }
          if (uri.indexOf(this[i]) != -1)
            continue;

          return false;
        }
        return true;
      }, this.searchField.value.split(/\s+/g));
    }

    return this._tabList = list;
  },

  init: function ctrlTab__init() {
    if (this._recentlyUsedTabs)
      return;
    this._recentlyUsedTabs = [gBrowser.selectedTab];

    var tabContainer = gBrowser.tabContainer;
    tabContainer.addEventListener("TabOpen", this, false);
    tabContainer.addEventListener("TabSelect", this, false);
    tabContainer.addEventListener("TabClose", this, false);

    this._handleCtrlTab =
      gPrefService.getBoolPref("browser.ctrlTab.previews") &&
      (!gPrefService.prefHasUserValue("browser.ctrlTab.disallowForScreenReaders") ||
       !gPrefService.getBoolPref("browser.ctrlTab.disallowForScreenReaders"));
    if (this._handleCtrlTab)
      gBrowser.mTabBox.handleCtrlTab = false;
    document.addEventListener("keypress", this, false);
  },

  uninit: function ctrlTab__uninit() {
    this._recentlyUsedTabs = null;

    var tabContainer = gBrowser.tabContainer;
    tabContainer.removeEventListener("TabOpen", this, false);
    tabContainer.removeEventListener("TabSelect", this, false);
    tabContainer.removeEventListener("TabClose", this, false);

    this.panel.removeEventListener("popuphiding", this, false);
    this.panel.removeEventListener("popupshown", this, false);
    this.panel.removeEventListener("popuphidden", this, false);
    document.removeEventListener("keypress", this, false);
    if (this._handleCtrlTab)
      gBrowser.mTabBox.handleCtrlTab = true;
  },

  search: function ctrlTab__search() {
    if (this.isOpen) {
      this._tabList = null;
      this.buildPagesBar();
      this.goToPage(0, 0);
      this.updatePreviews();
    }
  },

  buildPagesBar: function ctrlTab__buildPagesBar() {
    var pages = this.pages;
    if (pages == 1)
      pages = 0;
    while (this.pagesBar.childNodes.length > pages)
      this.pagesBar.removeChild(this.pagesBar.lastChild);
    while (this.pagesBar.childNodes.length < pages) {
      let pointer = document.createElement("spacer");
      pointer.setAttribute("onclick", "ctrlTab.goToPage(" + this.pagesBar.childNodes.length + ");");
      pointer.setAttribute("class", "ctrlTab-pagePointer");
      this.pagesBar.appendChild(pointer);
    }
  },

  goToPage: function ctrlTab__goToPage(aPage, aIndex) {
    this.page = aPage;
    this.selected.removeAttribute("selected");
    if (aIndex) {
      this.selectedIndex = aIndex;
      while (!this.selected || !this.selected.hasAttribute("valid"))
        this.selectedIndex--;
    } else {
      this.selectedIndex = 0;
    }
    this.updateSelected();
  },

  updatePreviews: function ctrlTab__updatePreviews() {
    var tabs = this.tabList;
    var offset = this.page * this.thumbnails.length;
    for (let i = 0; i < this.thumbnails.length; i++)
      this.updatePreview(this.thumbnails[i], tabs[i + offset]);
  },
  updatePreview: function ctrlTab__updatePreview(aThumbnail, aTab) {
    do {
      if (aThumbnail._tab) {
        if (aThumbnail._tab == aTab)
          break;
        aThumbnail._tab.removeEventListener("DOMAttrModified", this, false);
      }
      aThumbnail._tab = aTab;
      if (aTab)
        aTab.addEventListener("DOMAttrModified", this, false);
    } while (false);

    if (aThumbnail.firstChild)
      aThumbnail.removeChild(aThumbnail.firstChild);
    if (aTab) {
      aThumbnail.appendChild(tabPreviews.get(aTab));
      aThumbnail.setAttribute("valid", "true");
      aThumbnail.setAttribute("label", aTab.label);
      aThumbnail.setAttribute("crop", aTab.crop);
    } else {
      let placeholder = document.createElement("hbox");
      placeholder.height = tabPreviews.height;
      aThumbnail.appendChild(placeholder);
      aThumbnail.removeAttribute("valid");
      aThumbnail.setAttribute("label", "placeholder");
    }
    aThumbnail.width = tabPreviews.width;
  },

  tabAttrModified: function ctrlTab__tabAttrModified(aTab, aAttrName) {
    switch (aAttrName) {
      case "label":
      case "crop":
      case "busy":
        for (let i = this.thumbnails.length - 1; i >= 0; i--) {
          if (this.thumbnails[i]._tab == aTab) {
            this.updatePreview(this.thumbnails[i], aTab);
            break;
          }
        }
        break;
    }
  },

  advanceSelected: function ctrlTab__advanceSelected() {
    this.selected.removeAttribute("selected");

    this.selectedIndex += this.invertDirection ? -1 : 1;
    if (this.selectedIndex < 0) {
      this.page--;
      this.selectedIndex = this.thumbnails.length - 1;
      while (!this.selected.hasAttribute("valid"))
        this.selectedIndex--;
    } else if (this.selectedIndex >= this.thumbnails.length || !this.selected.hasAttribute("valid")) {
      this.page++;
      this.selectedIndex = 0;
    }
    this.updateSelected();
  },

  updateSelected: function ctrlTab__updateSelected() {
    if (this.tabCount)
      this.selected.setAttribute("selected", "true");
  },

  selectThumbnail: function ctrlTab__selectThumbnail(aThumbnail) {
    if (this.tabCount) {
      this._tabToSelect = (aThumbnail || this.selected)._tab;
      this.panel.hidePopup();
    }
  },

  attachTab: function ctrlTab__attachTab(aTab, aPos) {
    if (aPos == 0)
      this._recentlyUsedTabs.unshift(aTab);
    else if (aPos)
      this._recentlyUsedTabs.splice(aPos, 0, aTab);
    else
      this._recentlyUsedTabs.push(aTab);
  },
  detachTab: function ctrlTab__detachTab(aTab, aTabs) {
    var tabs = aTabs || this._recentlyUsedTabs;
    var i = tabs.indexOf(aTab);
    if (i >= 0)
      tabs.splice(i, 1);
  },

  open: function ctrlTab__open(aSticky) {
    if (this.isOpen && this.sticky) {
      this.panel.hidePopup();
      return;
    }
    this.sticky = !!aSticky;

    this._deferOnTabSelect = [];
    if (this.invertDirection)
      this._useTabBarOrder = true;

    this._tabBarHandlesCtrlPageUpDown = gBrowser.mTabBox.handleCtrlPageUpDown;
    gBrowser.mTabBox.handleCtrlPageUpDown = false;

    document.addEventListener("keyup", this, false);
    document.addEventListener("keydown", this, false);
    this.panel.addEventListener("popupshown", this, false);
    this.panel.addEventListener("popuphiding", this, false);
    this.panel.addEventListener("popuphidden", this, false);
    this._prevFocus = document.commandDispatcher.focusedElement ||
                      document.commandDispatcher.focusedWindow;
    this.panel.hidden = false;
    this.panel.width = screen.availWidth * .85;
    this.panel.popupBoxObject.setConsumeRollupEvent(Ci.nsIPopupBoxObject.ROLLUP_CONSUME);
    this.panel.openPopupAtScreen(screen.availLeft + (screen.availWidth - this.panel.width) / 2,
                                 screen.availTop + screen.availHeight * .12,
                                 false);
    this.buildPagesBar();
    this.selectedIndex = 0;
    this.page = 0;
    this.advanceSelected();
  },

  onKeyPress: function ctrlTab__onKeyPress(event) {
    var isOpen = this.isOpen;

    if (isOpen && event.target == this.searchField)
      return;

    if (isOpen) {
      event.preventDefault();
      event.stopPropagation();
    }

    switch (event.keyCode) {
      case event.DOM_VK_TAB:
        if ((event.ctrlKey || this.sticky) && !event.altKey && !event.metaKey) {
          this.invertDirection = event.shiftKey;
          if (isOpen) {
            this.advanceSelected();
          } else if (this._handleCtrlTab) {
            event.preventDefault();
            event.stopPropagation();
            if (gBrowser.mTabs.length > 2) {
              this.open();
            } else if (gBrowser.mTabs.length == 2) {
              gBrowser.selectedTab = gBrowser.selectedTab.nextSibling ||
                                     gBrowser.selectedTab.previousSibling;
            }
          }
        }
        break;
      case event.DOM_VK_UP:
        if (isOpen) {
          let index = this.selectedIndex - this.columns;
          if (index < 0) {
            this.goToPage(this.page - 1, this.thumbnails.length + index);
          } else {
            this.selected.removeAttribute("selected");
            this.selectedIndex = index;
            this.updateSelected();
          }
        }
        break;
      case event.DOM_VK_DOWN:
        if (isOpen) {
          let index = this.selectedIndex + this.columns;
          if (index >= this.thumbnails.length || !this.thumbnails[index].hasAttribute("valid")) {
            this.goToPage(this.page + 1, this.selectedIndex % this.columns);
          } else {
            this.selected.removeAttribute("selected");
            this.selectedIndex = index;
            while (!this.selected.hasAttribute("valid"))
              this.selectedIndex--;
            this.updateSelected();
          }
        }
        break;
      case event.DOM_VK_LEFT:
        if (isOpen) {
          this.invertDirection = true;
          this.advanceSelected();
        }
        break;
      case event.DOM_VK_RIGHT:
        if (isOpen) {
          this.invertDirection = false;
          this.advanceSelected();
        }
        break;
      case event.DOM_VK_HOME:
        if (isOpen)
          this.goToPage(0);
        break;
      case event.DOM_VK_END:
        if (isOpen)
          this.goToPage(this.pages - 1, this.thumbnails.length - 1);
        break;
      case event.DOM_VK_PAGE_UP:
        if (isOpen)
          this.goToPage(this.page - 1);
        break;
      case event.DOM_VK_PAGE_DOWN:
        if (isOpen)
          this.goToPage(this.page + 1);
        break;
      case event.DOM_VK_RETURN:
        if (isOpen && this.sticky)
          this.selectThumbnail();
        break;
      case event.DOM_VK_ESCAPE:
        if (isOpen)
          this.panel.hidePopup();
        break;
      default:
        if (isOpen && event.ctrlKey) {
          switch (event.charCode) {
            case this.closeCharCode:
              gBrowser.removeTab(this.selected._tab);
              break;
            case this.findCharCode:
              this.searchField.focus();
              break;
          }
        }
    }
  },
  onPopupHiding: function ctrlTab__onPopupHiding() {
    gBrowser.mTabBox.handleCtrlPageUpDown = this._tabBarHandlesCtrlPageUpDown;
    document.removeEventListener("keyup", this, false);
    document.removeEventListener("keydown", this, false);

    this.selected.removeAttribute("selected");
    if (this.pagesBar.childNodes.length)
      this.pagesBar.childNodes[this.page].removeAttribute("selected");

    Array.forEach(this.thumbnails, function (thumbnail) {
      this.updatePreview(thumbnail, null);
    }, this);

    this.searchField.value = "";
    this.invertDirection = false;
    this._useTabBarOrder = false;
    this._page = null;
    this._tabList = null;

    this._deferOnTabSelect.forEach(this.onTabSelect, this);
    this._deferOnTabSelect = null;
  },
  onTabSelect: function ctrlTab__onTabSelect(aTab) {
    if (aTab.parentNode) {
      this.detachTab(aTab);
      this.attachTab(aTab, 0);
    }
  },

  removeClosingTabFromUI: function ctrlTab__removeClosingTabFromUI(aTab) {
    this._closing = aTab;
    this._tabList = null;
    if (this.tabCount == 1) {
      this.panel.hidePopup();
    } else {
      this.buildPagesBar();
      this.updatePreviews();
      if (!this.selected.hasAttribute("valid"))
        this.advanceSelected();
      else
        this.updateSelected();
    }
    this._closing = null;
  },

  handleEvent: function ctrlTab__handleEvent(event) {
    switch (event.type) {
      case "DOMAttrModified":
        this.tabAttrModified(event.target, event.attrName);
        break;
      case "TabSelect":
        if (this.isOpen)
          // don't change the tab order while the panel is open
          this._deferOnTabSelect.push(event.target);
        else
          this.onTabSelect(event.target);
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
      case "keydown":
      case "keyup":
        if (event.target == this.searchField &&
            (event.keyCode == event.DOM_VK_RETURN ||
             event.keyCode == event.DOM_VK_ESCAPE))
          this.panel.focus();
        if (!this.sticky &&
            event.type == "keyup" &&
            event.keyCode == event.DOM_VK_CONTROL)
          this.selectThumbnail();
        break;
      case "popupshown":
        if (this.sticky)
          this.searchField.focus();
        else
          this.panel.focus();
        break;
      case "popuphiding":
        this.onPopupHiding();
        break;
      case "popuphidden":
        this._prevFocus.focus();
        this._prevFocus = null;
        if (this._tabToSelect) {
          gBrowser.selectedTab = this._tabToSelect;
          this._tabToSelect = null;
        }
        // Destroy the widget in order to prevent outdated content
        // when re-opening the panel.
        this.panel.hidden = true;
        break;
    }
  }
};
