/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const POPUP_OPTIONS = {
  position: "bottomleft topleft",
  x: 0,
  y: -2,
};

/**
 * Detailed preview card that displays when hovering a tab
 */
export default class TabPreviewPanel {
  constructor(panel) {
    this._panel = panel;
    this._win = panel.ownerGlobal;
    this._tab = null;
    this._thumbnailElement = null;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefDisableAutohide",
      "ui.popup.disable_autohide",
      false
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefPreviewDelay",
      "ui.tooltip.delay_ms"
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefDisplayThumbnail",
      "browser.tabs.cardPreview.showThumbnails",
      false
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefShowPidAndActiveness",
      "browser.tabs.tooltipsShowPidAndActiveness",
      false
    );
    this._timer = null;
  }

  getPrettyURI(uri) {
    try {
      const url = new URL(uri);
      return `${url.hostname}`.replace(/^w{3}\./, "");
    } catch {
      return uri;
    }
  }

  _needsThumbnailFor(tab) {
    return !tab.selected;
  }

  _maybeRequestThumbnail() {
    if (!this._prefDisplayThumbnail) {
      return;
    }
    if (!this._needsThumbnailFor(this._tab)) {
      return;
    }
    let tab = this._tab;
    this._win.tabPreviews.get(tab).then(el => {
      if (this._tab == tab && this._needsThumbnailFor(tab)) {
        this._thumbnailElement = el;
        this._updatePreview();
      }
    });
  }

  activate(tab) {
    this._tab = tab;
    this._thumbnailElement = null;
    this._maybeRequestThumbnail();
    if (this._panel.state == "open") {
      this._updatePreview();
    }
    if (this._timer) {
      return;
    }
    this._timer = this._win.setTimeout(() => {
      this._timer = null;
      this._panel.openPopup(this._tab, POPUP_OPTIONS);
    }, this._prefPreviewDelay);
    this._win.addEventListener("TabSelect", this);
    this._panel.addEventListener("popupshowing", this);
  }

  deactivate(leavingTab = null) {
    if (leavingTab) {
      if (this._tab != leavingTab) {
        return;
      }
      this._win.requestAnimationFrame(() => {
        if (this._tab == leavingTab) {
          this.deactivate();
        }
      });
      return;
    }
    this._tab = null;
    this._thumbnailElement = null;
    this._panel.removeEventListener("popupshowing", this);
    this._win.removeEventListener("TabSelect", this);
    if (!this._prefDisableAutohide) {
      this._panel.hidePopup();
    }
    if (this._timer) {
      this._win.clearTimeout(this._timer);
      this._timer = null;
    }
  }

  handleEvent(e) {
    switch (e.type) {
      case "popupshowing":
        this._updatePreview();
        break;
      case "TabSelect":
        if (this._thumbnailElement && !this._needsThumbnailFor(this._tab)) {
          this._thumbnailElement.remove();
          this._thumbnailElement = null;
        }
        break;
    }
  }

  _updatePreview() {
    this._panel.querySelector(".tab-preview-title").textContent =
      this._displayTitle;
    this._panel.querySelector(".tab-preview-uri").textContent =
      this._displayURI;

    if (this._prefShowPidAndActiveness) {
      this._panel.querySelector(".tab-preview-pid").textContent =
        this._displayPids;
      this._panel.querySelector(".tab-preview-activeness").textContent =
        this._displayActiveness;
    } else {
      this._panel.querySelector(".tab-preview-pid").textContent = "";
      this._panel.querySelector(".tab-preview-activeness").textContent = "";
    }

    let thumbnailContainer = this._panel.querySelector(
      ".tab-preview-thumbnail-container"
    );
    if (thumbnailContainer.firstChild != this._thumbnailElement) {
      thumbnailContainer.replaceChildren();
      if (this._thumbnailElement) {
        thumbnailContainer.appendChild(this._thumbnailElement);
      }
      this._panel.dispatchEvent(
        new CustomEvent("previewThumbnailUpdated", {
          detail: {
            thumbnail: this._thumbnailElement,
          },
        })
      );
    }
    if (this._tab && this._panel.state == "open") {
      this._panel.moveToAnchor(
        this._tab,
        POPUP_OPTIONS.position,
        POPUP_OPTIONS.x,
        POPUP_OPTIONS.y
      );
    }
  }

  get _displayTitle() {
    if (!this._tab) {
      return "";
    }
    return this._tab.textLabel.textContent;
  }

  get _displayURI() {
    if (!this._tab) {
      return "";
    }
    return this.getPrettyURI(this._tab.linkedBrowser.currentURI.spec);
  }

  get _displayPids() {
    const pids = this._win.gBrowser.getTabPids(this._tab);
    if (!pids.length) {
      return "";
    }

    let pidLabel = pids.length > 1 ? "pids" : "pid";
    return `${pidLabel}: ${pids.join(", ")}`;
  }

  get _displayActiveness() {
    return this._tab.linkedBrowser.docShellIsActive ? "[A]" : "";
  }
}
