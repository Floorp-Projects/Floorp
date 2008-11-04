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
  init: function () {
    this.width = Math.ceil(screen.availWidth / 5);
    this.height = Math.round(this.width * this.aspectRatio);

    gBrowser.tabContainer.addEventListener("TabSelect", this, false);
    gBrowser.tabContainer.addEventListener("SSTabRestored", this, false);
  },
  uninit: function () {
    gBrowser.tabContainer.removeEventListener("TabSelect", this, false);
    gBrowser.tabContainer.removeEventListener("SSTabRestored", this, false);
    this._selectedTab = null;
  },
  get: function (aTab) {
    if (aTab.__thumbnail_lastURI &&
        aTab.__thumbnail_lastURI != aTab.linkedBrowser.currentURI.spec) {
      aTab.__thumbnail = null;
      aTab.__thumbnail_lastURI = null;
    }
    return aTab.__thumbnail || this.capture(aTab, !aTab.hasAttribute("busy"));
  },
  capture: function (aTab, aStore) {
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
  handleEvent: function (event) {
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
  get label () {
    delete this.label;
    return this.label = document.getElementById("ctrlTab-label");
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
                                        .toLowerCase().charCodeAt(0);
  },
  get recentlyUsedLimit () {
    delete this.recentlyUsedLimit;
    return this.recentlyUsedLimit = gPrefService.getIntPref("browser.ctrlTab.recentlyUsedLimit");
  },
  selectedIndex: 0,
  get selected () this.thumbnails.item(this.selectedIndex),
  get isOpen   () this.panel.state == "open" || this.panel.state == "showing",
  get tabCount () gBrowser.mTabs.length - (this._closing ? 1 : 0),
  get pages    () Math.ceil(this.tabCount / this.thumbnails.length),
  get page     () this._page || 0,
  set page (page) {
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
    return list;
  },
  init: function () {
    if (this._recentlyUsedTabs)
      return;
    this._recentlyUsedTabs = [gBrowser.selectedTab];

    var tabContainer = gBrowser.tabContainer;
    tabContainer.addEventListener("TabOpen", this, false);
    tabContainer.addEventListener("TabSelect", this, false);
    tabContainer.addEventListener("TabClose", this, false);

    gBrowser.mTabBox.handleCtrlTab = false;
    document.addEventListener("keypress", this, false);
  },
  uninit: function () {
    this._recentlyUsedTabs = null;

    var tabContainer = gBrowser.tabContainer;
    tabContainer.removeEventListener("TabOpen", this, false);
    tabContainer.removeEventListener("TabSelect", this, false);
    tabContainer.removeEventListener("TabClose", this, false);

    this.panel.removeEventListener("popuphiding", this, false);
    document.removeEventListener("keypress", this, false);
    gBrowser.mTabBox.handleCtrlTab = true;
  },
  buildPagesBar: function () {
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
  goToPage: function (aPage, aIndex) {
    if (this.page != aPage)
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
  updatePreviews: function () {
    var tabs = this.tabList;
    var offset = this.page * this.thumbnails.length;
    for (let i = 0; i < this.thumbnails.length; i++)
      this.updatePreview(this.thumbnails[i], tabs[i + offset]);
  },
  updatePreview: function (aThumbnail, aTab) {
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
    } else {
      aThumbnail.removeAttribute("valid");
    }
    aThumbnail.width = tabPreviews.width;
    aThumbnail.height = tabPreviews.height;
  },
  tabAttrModified: function (aTab, aAttrName) {
    switch (aAttrName) {
      case "busy":
        for (let i = this.thumbnails.length - 1; i >= 0; i--) {
          if (this.thumbnails[i]._tab == aTab) {
            this.updatePreview(this.thumbnails[i], aTab);
            break;
          }
        }
        break;
      case "label":
      case "crop":
        if (this.selected._tab == aTab)
          this.label[aAttrName == "label" ? "value" : aAttrName] = aTab[aAttrName];
        break;
    }
  },
  advanceSelected: function () {
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
  updateSelected: function () {
    var thumbnail = this.selected;
    thumbnail.setAttribute("selected", "true");
    this.label.value = thumbnail._tab.label;
    this.label.crop = thumbnail._tab.crop;
    var url = thumbnail._tab.linkedBrowser.currentURI.spec;
    if (url == "about:blank") {
      // XXXhack: Passing a space here (and not "")
      // to make sure the browser implementation would
      // still consider it a hovered link.
      url = " ";
    } else {
      try {
        url = decodeURI(url);
      } catch (e) {}
    }
    XULBrowserWindow.setOverLink(url, null);
  },
  thumbnailClick: function (event) {
    switch (event.button) {
      case 0:
        this.selectThumbnail(event.currentTarget);
        break;
      case 1:
        gBrowser.removeTab(event.currentTarget._tab);
        break;
    }
  },
  selectThumbnail: function (aThumbnail) {
    var selectedTab = (aThumbnail || this.selected)._tab;
    this.panel.hidePopup();
    gBrowser.selectedTab = selectedTab;
  },
  attachTab: function (aTab, aPos) {
    if (aPos == 0)
      this._recentlyUsedTabs.unshift(aTab);
    else if (aPos)
      this._recentlyUsedTabs.splice(aPos, 0, aTab);
    else
      this._recentlyUsedTabs.push(aTab);
  },
  detachTab: function (aTab, aTabs) {
    var tabs = aTabs || this._recentlyUsedTabs;
    var i = tabs.indexOf(aTab);
    if (i >= 0)
      tabs.splice(i, 1);
  },
  open: function () {
    if (window.allTabs)
      allTabs.close();

    this._deferOnTabSelect = [];
    if (this.invertDirection)
      this._useTabBarOrder = true;

    this._tabBarHandlesCtrlPageUpDown = gBrowser.mTabBox.handleCtrlPageUpDown;
    gBrowser.mTabBox.handleCtrlPageUpDown = false;

    document.addEventListener("keyup", this, false);
    document.addEventListener("keydown", this, false);
    this.panel.addEventListener("popuphiding", this, false);
    this.panel.hidden = false;
    this.panel.width = screen.availWidth * .9;
    this.panel.openPopupAtScreen(screen.availLeft + (screen.availWidth - this.panel.width) / 2,
                                 screen.availTop + screen.availHeight * .1,
                                 false);
    this.buildPagesBar();
    this.selectedIndex = 0;
    this.page = 0;
    this.advanceSelected();
  },
  onKeyPress: function (event) {
    var isOpen = this.isOpen;
    var propagate = !isOpen;
    switch (event.keyCode) {
      case event.DOM_VK_TAB:
        if (event.ctrlKey && !event.altKey && !event.metaKey) {
          propagate = false;
          this.invertDirection = event.shiftKey;
          if (isOpen)
            this.advanceSelected();
          else if (this.tabCount == 2)
            gBrowser.selectedTab = this.tabList[1];
          else if (this.tabCount > 2)
            this.open();
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
      case event.DOM_VK_ESCAPE:
        if (isOpen)
          this.panel.hidePopup();
        break;
      default:
        if (isOpen && event.charCode == this.closeCharCode)
          gBrowser.removeTab(this.selected._tab);
    }
    if (!propagate) {
      event.stopPropagation();
      event.preventDefault();
    }
  },
  onPopupHiding: function () {
    gBrowser.mTabBox.handleCtrlPageUpDown = this._tabBarHandlesCtrlPageUpDown;
    document.removeEventListener("keyup", this, false);
    document.removeEventListener("keydown", this, false);

    this.selected.removeAttribute("selected");
    if (this.pagesBar.childNodes.length)
      this.pagesBar.childNodes[this.page].removeAttribute("selected");

    Array.forEach(this.thumbnails, function (thumbnail) {
      this.updatePreview(thumbnail, null);
    }, this);

    this.invertDirection = false;
    this._useTabBarOrder = false;
    this.label.value = "";
    XULBrowserWindow.setOverLink("", null);
    this._page = null;

    this._deferOnTabSelect.forEach(this.onTabSelect, this);
    this._deferOnTabSelect = null;
  },
  onTabSelect: function (aTab) {
    if (aTab.parentNode) {
      this.detachTab(aTab);
      this.attachTab(aTab, 0);
    }
  },
  removeClosingTabFromUI: function (aTab) {
    this._closing = aTab;
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
  handleEvent: function (event) {
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
        // the panel is open; don't propagate any key events
        event.stopPropagation();
        event.preventDefault();
        if (event.type == "keyup" && event.keyCode == event.DOM_VK_CONTROL)
          this.selectThumbnail();
        break;
      case "popuphiding":
        this.onPopupHiding();
        break;
    }
  }
};
