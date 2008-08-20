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
  aspectRatio: 0.6875, // 16:11
  init: function () {
    this.width = Math.ceil(screen.availWidth / 7);
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
    return aTab.__thumbnail || this.capture(aTab, !aTab.hasAttribute("busy"));
  },
  capture: function (aTab, aStore) {
    var win = aTab.linkedBrowser.contentWindow;
    var thumbnail = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    thumbnail.mozOpaque = true;
    thumbnail.height = this.height;
    thumbnail.width = this.width;
    var ctx = thumbnail.getContext("2d");
    var widthScale = this.width / win.innerWidth;
    ctx.scale(widthScale, widthScale);
    ctx.drawWindow(win, win.scrollX, win.scrollY,
                   win.innerWidth, win.innerWidth * this.aspectRatio, "rgb(255,255,255)");
    var data = thumbnail.toDataURL("image/jpeg", "quality=60");
    if (aStore)
      aTab.__thumbnail = data;
    return data;
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
  tabs: null,
  visibleCount: 3,
  _uniqid: 0,
  get panel () {
    delete this.panel;
    return this.panel = document.getElementById("ctrlTab-panel");
  },
  get label () {
    delete this.label;
    return this.label = document.getElementById("ctrlTab-label");
  },
  get svgRoot () {
    delete this.svgRoot;

    let (groundFade = document.getElementById("ctrlTab-groundFade")) {
      groundFade.setAttribute("height", Math.ceil(tabPreviews.height * .25) + 1);
      groundFade.setAttribute("y", tabPreviews.height + 1);
    }

    this.svgRoot = document.getElementById("ctrlTab-svgRoot");
    this.svgRoot.setAttribute("height", tabPreviews.height * 1.25 + 2);
    return this.svgRoot;
  },
  get container () {
    delete this.container;
    return this.container = document.getElementById("ctrlTab-container");
  },
  get rtl () {
    delete this.rtl;
    return this.rtl = getComputedStyle(this.panel, "").direction == "rtl";
  },
  get iconSize () {
    delete this.iconSize;
    return this.iconSize = Math.max(16, Math.round(tabPreviews.height / 5));
  },
  get closeCharCode () {
    delete this.closeCharCode;
    return this.closeCharCode = document.getElementById("key_close")
                                        .getAttribute("key")
                                        .toLowerCase().charCodeAt(0);
  },
  get smoothScroll () {
    delete this.smoothScroll;
    return this.smoothScroll = gPrefService.getBoolPref("browser.ctrlTab.smoothScroll");
  },
  get offscreenStart () {
    return Array.indexOf(this.container.childNodes, this.selected) - 1;
  },
  get offscreenEnd () {
    return this.container.childNodes.length - this.visibleCount - this.offscreenStart;
  },
  get offsetX () {
    return - tabPreviews.width * (this.rtl ? this.offscreenEnd : this.offscreenStart);
  },
  get isOpen () {
    return this.panel.state == "open" || this.panel.state == "showing";
  },
  init: function () {
    if (this.tabs)
      return;

    var tabContainer = gBrowser.tabContainer;

    this.tabs = [];
    Array.forEach(tabContainer.childNodes, function (tab) {
      this.attachTab(tab, tab == gBrowser.selectedTab);
    }, this);

    tabContainer.addEventListener("TabOpen", this, false);
    tabContainer.addEventListener("TabSelect", this, false);
    tabContainer.addEventListener("TabClose", this, false);

    gBrowser.mTabBox.handleCtrlTab = false;
    document.addEventListener("keypress", this, false);
  },
  uninit: function () {
    this.tabs = null;

    var tabContainer = gBrowser.tabContainer;
    tabContainer.removeEventListener("TabOpen", this, false);
    tabContainer.removeEventListener("TabSelect", this, false);
    tabContainer.removeEventListener("TabClose", this, false);

    this.panel.removeEventListener("popuphiding", this, false);
    document.removeEventListener("keypress", this, false);
  },
  addBox: function (aAtStart) {
    const SVGNS = "http://www.w3.org/2000/svg";

    var thumbnail = document.createElementNS(SVGNS, "image");
    thumbnail.setAttribute("class", "ctrlTab-thumbnail");
    thumbnail.setAttribute("height", tabPreviews.height);
    thumbnail.setAttribute("width", tabPreviews.width);

    var thumbnail_border = document.createElementNS(SVGNS, "rect");
    thumbnail_border.setAttribute("class", "ctrlTab-thumbnailborder");
    thumbnail_border.setAttribute("height", tabPreviews.height);
    thumbnail_border.setAttribute("width", tabPreviews.width);

    var icon = document.createElementNS(SVGNS, "image");
    icon.setAttribute("class", "ctrlTab-icon");
    icon.setAttribute("height", this.iconSize);
    icon.setAttribute("width", this.iconSize);
    icon.setAttribute("x", - this.iconSize * .2);
    icon.setAttribute("y", tabPreviews.height - this.iconSize * 1.2);

    var thumbnail_and_icon = document.createElementNS(SVGNS, "g");
    thumbnail_and_icon.appendChild(thumbnail);
    thumbnail_and_icon.appendChild(thumbnail_border);
    thumbnail_and_icon.appendChild(icon);

    var reflection = document.createElementNS(SVGNS, "use");
    reflection.setAttribute("class", "ctrlTab-reflection");
    var ref_scale = .5;
    reflection.setAttribute("transform", "scale(1,-" + ref_scale + ")");
    reflection.setAttribute("y", - ((1 / ref_scale + 1) * tabPreviews.height +
                                    (1 / ref_scale) * 2));

    var box = document.createElementNS(SVGNS, "g");
    box.setAttribute("class", "ctrlTab-box");
    box.setAttribute("onclick", "ctrlTab.pick(this);");
    box.appendChild(thumbnail_and_icon);
    box.appendChild(reflection);

    if (aAtStart)
      this.container.insertBefore(box, this.container.firstChild);
    else
      this.container.appendChild(box);
    return box;
  },
  removeBox: function (aBox) {
    this.container.removeChild(aBox);
    if (!Array.some(this.container.childNodes, function (box) box._tab == aBox._tab))
      aBox._tab.removeEventListener("DOMAttrModified", this, false);
    aBox._tab = null;
  },
  addPreview: function (aBox, aTab) {
    const XLinkNS = "http://www.w3.org/1999/xlink";

    aBox._tab = aTab;
    let (thumbnail = aBox.firstChild.firstChild)
      thumbnail.setAttributeNS(XLinkNS, "href", tabPreviews.get(aTab));
    this.updateIcon(aBox);

    aTab.addEventListener("DOMAttrModified", this, false);

    if (!aBox.firstChild.hasAttribute("id")) {
      // set up reflection
      this._uniqid++;
      aBox.firstChild.setAttribute("id", "ctrlTab-preview-" + this._uniqid);
      aBox.lastChild.setAttributeNS(XLinkNS, "href", "#ctrlTab-preview-" + this._uniqid);
    }
  },
  updateIcon: function (aBox) {
    const XLinkNS = "http://www.w3.org/1999/xlink";
    var url = aBox._tab.hasAttribute("busy") ?
              "chrome://global/skin/icons/loading_16.png" :
              aBox._tab.getAttribute("image");
    var icon = aBox.firstChild.lastChild;
    if (url)
      icon.setAttributeNS(XLinkNS, "href", url);
    else
      icon.removeAttributeNS(XLinkNS, "href");
  },
  tabAttrModified: function (aTab, aAttrName) {
    switch (aAttrName) {
      case "busy":
      case "image":
        Array.forEach(this.container.childNodes, function (box) {
          if (box._tab == aTab) {
            if (aAttrName == "busy")
              this.addPreview(box, aTab);
            else
              this.updateIcon(box);
          }
        }, this);
        break;
      case "label":
      case "crop":
        if (!this._scrollTimer) {
          let boxes = this.container.childNodes;
          for (let i = boxes.length - 1; i >= 0; i--) {
            if (boxes[i]._tab == aTab && boxes[i] == this.selected) {
              this.label[aAttrName == "label" ? "textContent" : aAttrName] =
                aTab.getAttribute(aAttrName);
              break;
            }
          }
        }
        break;
    }
  },
  scroll: function () {
    if (!this.smoothScroll) {
      this.advanceSelected();
      this.arrangeBoxes();
      return;
    }

    this.stopScroll();
    let (next = this.invertDirection ? this.selected.previousSibling : this.selected.nextSibling) {
      this.setStatusbarValue(next);
      this.label.textContent = next._tab.label;
      //this.label.crop = next._tab.crop;
    }

    const FRAME_LENGTH = 40;
    var x = this.offsetX;
    var scrollAmounts = let (tenth = tabPreviews.width / (this.invertDirection == this.rtl ? -10 : 10))
                        [3 * tenth, 4 * tenth, 2 * tenth, tenth];

    function processFrame(self, lateness) {
      lateness += FRAME_LENGTH / 2;
      do {
        x += scrollAmounts.shift();
        lateness -= FRAME_LENGTH;
      } while (lateness > 0 && scrollAmounts.length);
      self.container.setAttribute("transform", "translate("+ x +",0)");
      self.svgRoot.forceRedraw();
      if (!scrollAmounts.length)
        self.stopScroll();
    }

    this._scrollTimer = setInterval(processFrame, FRAME_LENGTH, this);
    processFrame(this, 0);
  },
  stopScroll: function () {
    if (this._scrollTimer) {
      clearInterval(this._scrollTimer);
      this._scrollTimer = 0;
      this.advanceSelected();
      this.arrangeBoxes();
    }
  },
  advanceSelected: function () {
    // regardless of visibleCount, the new highlighted tab will be
    // the first or third-visible tab, depending on whether Shift is pressed
    var index = ((this.invertDirection ? 0 : 2) + this.offscreenStart + this.tabs.length)
                % this.tabs.length;
    if (index < 2)
      index += this.tabs.length;
    if (index > this.container.childNodes.length - this.visibleCount + 1)
      index -= this.tabs.length;
    this.selected = this.container.childNodes[index];
  },
  arrangeBoxes: function () {
    this.addOffscreenBox(this.invertDirection);
    this.addOffscreenBox(!this.invertDirection);

    // having lots of off-screen boxes reduces the scrolling speed, remove some
    for (let i = this.offscreenStart; i > 1; i--)
      this.removeBox(this.container.firstChild);
    for (let i = this.offscreenEnd; i > 1; i--)
      this.removeBox(this.container.lastChild);

    this.container.setAttribute("transform", "translate("+ this.offsetX +", 0)");

    for (let i = 0, l = this.container.childNodes.length; i < l; i++)
      this.arrange(i);
  },
  addOffscreenBox: function (aAtStart) {
    if (this.container.childNodes.length < this.tabs.length + this.visibleCount + 1 &&
        !(aAtStart ? this.offscreenStart : this.offscreenEnd)) {
      let i = aAtStart ?
              this.tabs.indexOf(this.container.firstChild._tab) - 1:
              this.tabs.indexOf(this.container.lastChild._tab) + 1;
      i = (i + this.tabs.length) % this.tabs.length;
      this.addPreview(this.addBox(aAtStart), this.tabs[i]);
    }
  },
  arrange: function (aIndex) {
    var box = this.container.childNodes[aIndex];
    var selected = box == this.selected;
    if (selected) {
      box.setAttribute("selected", "true");
      this.setStatusbarValue(box);
      this.label.textContent = box._tab.label;
      //this.label.crop = box._tab.crop;
    } else {
      box.removeAttribute("selected");
    }
    var scale = selected ? 1 : .75;
    var pos = this.rtl ? this.container.childNodes.length - 1 - aIndex : aIndex;
    var trans_x = tabPreviews.width * (pos + (1 - scale) / 2) / scale;
    var trans_y = (tabPreviews.height + 1) * (1 / scale - 1);
    box.setAttribute("transform", "scale(" + scale + "," + scale + ") " +
                                  "translate("+ trans_x + "," + trans_y + ")");
  },
  pick: function (aBox) {
    this.stopScroll();
    var selectedTab = (aBox || this.selected)._tab;
    this.panel.hidePopup();
    gBrowser.selectedTab = selectedTab;
  },
  setStatusbarValue: function (aBox) {
    var value = "";
    if (aBox) {
      value = aBox._tab.linkedBrowser.currentURI.spec;
      if (value == "about:blank") {
        // XXXhack: Passing a space here (and not "")
        // to make sure the browser implementation would
        // still consider it a hovered link.
        value = " ";
      } else {
        try {
          value = decodeURI(value);
        } catch (e) {}
      }
    }
    XULBrowserWindow.setOverLink(value, null);
  },
  attachTab: function (aTab, aSelected) {
    if (aSelected)
      this.tabs.unshift(aTab);
    else
      this.tabs.push(aTab);
  },
  detachTab: function (aTab) {
    var i = this.tabs.indexOf(aTab);
    if (i >= 0)
      this.tabs.splice(i, 1);
  },
  open: function () {
    this._deferOnTabSelect = [];

    document.addEventListener("keyup", this, false);
    document.addEventListener("keydown", this, false);
    this.panel.addEventListener("popuphiding", this, false);
    this.panel.hidden = false;
    this.panel.width = tabPreviews.width * this.visibleCount;
    this.panel.openPopupAtScreen(screen.availLeft + (screen.availWidth - this.panel.width) / 2,
                                 screen.availTop + (screen.availHeight - this.svgRoot.getAttribute("height")) / 2,
                                 false);

    // display $visibleCount tabs, starting with the first or
    // the second to the last tab, depending on whether Shift is pressed
    for (let index = this.invertDirection ? this.tabs.length - 2 : 0,
             i = this.visibleCount; i > 0; i--)
      this.addPreview(this.addBox(), this.tabs[index++ % this.tabs.length]);

    // regardless of visibleCount, highlight the second-visible tab
    this.selected = this.container.childNodes[1];
    this.arrangeBoxes();
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
            this.scroll();
          else if (this.tabs.length == 2)
            gBrowser.selectedTab = this.tabs[1];
          else if (this.tabs.length > 2)
            this.open();
        }
        break;
      case event.DOM_VK_ESCAPE:
        if (isOpen)
          this.panel.hidePopup();
        break;
      default:
        if (isOpen && event.charCode == this.closeCharCode) {
          this.stopScroll();
          gBrowser.removeTab(this.selected._tab);
        }
    }
    if (!propagate) {
      event.stopPropagation();
      event.preventDefault();
    }
  },
  onPopupHiding: function () {
    this.stopScroll();
    document.removeEventListener("keyup", this, false);
    document.removeEventListener("keydown", this, false);
    while (this.container.childNodes.length)
      this.removeBox(this.container.lastChild);
    this.selected = null;
    this.invertDirection = false;
    this._uniqid = 0;
    this.label.textContent = "";
    this.setStatusbarValue();
    this.container.removeAttribute("transform");
    this.svgRoot.forceRedraw();

    this._deferOnTabSelect.forEach(this.onTabSelect, this);
    this._deferOnTabSelect = null;
  },
  onTabSelect: function (aTab) {
    if (aTab.parentNode) {
      this.detachTab(aTab);
      this.attachTab(aTab, true);
    }
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
        this.attachTab(event.target);
        break;
      case "TabClose":
        if (this.isOpen) {
          if (this.tabs.length == 2) {
            // we have two tabs, one is being closed, so the panel isn't needed anymore
            this.panel.hidePopup();
          } else {
            if (event.target == this.selected._tab)
              this.advanceSelected();
            this.detachTab(event.target);
            Array.slice(this.container.childNodes).forEach(function (box) {
              if (box._tab == event.target) {
                this.removeBox(box);
                this.arrangeBoxes();
              }
            }, this);
          }
        }
        this.detachTab(event.target);
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
          this.pick();
        break;
      case "popuphiding":
        this.onPopupHiding();
        break;
    }
  }
};
