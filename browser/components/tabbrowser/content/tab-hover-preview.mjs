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
export default class TabHoverPreviewPanel {
  constructor(panel) {
    this._panel = panel;
    this._win = panel.ownerGlobal;
    this._tab = null;
    this._thumbnailElement = null;

    this._setExternalPopupListeners();

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
      "browser.tabs.hoverPreview.showThumbnails",
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
      if (url.protocol === "about:") {
        return url.href;
      }
      return `${url.hostname}`.replace(/^w{3}\./, "");
    } catch {
      return uri;
    }
  }

  _hasValidThumbnailState(tab) {
    return (
      tab && tab.linkedBrowser && !tab.getAttribute("pending") && !tab.selected
    );
  }

  _maybeRequestThumbnail() {
    let tab = this._tab;

    if (!this._prefDisplayThumbnail || !this._hasValidThumbnailState(tab)) {
      return;
    }
    let thumbnailCanvas = this._win.document.createElement("canvas");

    this._win.PageThumbs.captureToCanvas(tab.linkedBrowser, thumbnailCanvas, {
      fullViewport: true,
      targetWidth: 280,
      preserveAspectRatio: true,
    })
      .then(() => {
        // in case we've changed tabs after capture started, ensure we still want to show the thumbnail
        if (this._tab == tab && this._hasValidThumbnailState(tab)) {
          this._thumbnailElement = thumbnailCanvas;
          this._updatePreview();
        }
      })
      .catch(e => {
        // Most likely the window was killed before capture completed, so just log the error
        console.error(e);
      });
  }

  activate(tab) {
    if (this._isDisabled()) {
      return;
    }

    this._tab = tab;

    // Calling `moveToAnchor` in advance of the call to `openPopup` ensures
    // that race conditions can be avoided in cases where the user hovers
    // over a different tab while the preview panel is still opening.
    // This will ensure the move operation is carried out even if the popup is
    // in an intermediary state (opening but not fully open).
    //
    // If the popup is closed this call will be ignored.
    this._movePanel();

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
        if (
          this._thumbnailElement &&
          !this._hasValidThumbnailState(this._tab)
        ) {
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
    this._movePanel();
  }

  _movePanel() {
    if (this._tab) {
      this._panel.moveToAnchor(
        this._tab,
        POPUP_OPTIONS.position,
        POPUP_OPTIONS.x,
        POPUP_OPTIONS.y
      );
    }
  }

  /**
   * Listen for any panels or menupopups that open or close anywhere else in the DOM tree
   * and maintain a list of the ones that are currently open.
   * This is used to disable tab previews until such time as the other panels are closed.
   */
  _setExternalPopupListeners() {
    // Since the tab preview panel is lazy loaded, there is a possibility that panels could
    // already be open on init. Therefore we need to initialize _openPopups with existing panels
    // the first time.
    const initialPopups = this._win.document.querySelectorAll(
      "panel[panelopen=true]:not(#tab-preview-panel), menupopup[open=true]"
    );
    this._openPopups = new Set(initialPopups);

    const handleExternalPopupEvent = (eventName, setMethod) => {
      this._win.addEventListener(eventName, ev => {
        if (
          ev.target !== this._panel &&
          (ev.target.nodeName == "panel" || ev.target.nodeName == "menupopup")
        ) {
          this._openPopups[setMethod](ev.target);
        }
      });
    };
    handleExternalPopupEvent("popupshowing", "add");
    handleExternalPopupEvent("popuphidden", "delete");
  }

  _isDisabled() {
    return Boolean(this._openPopups.size);
  }

  get _displayTitle() {
    if (!this._tab) {
      return "";
    }
    return this._tab.textLabel.textContent;
  }

  get _displayURI() {
    if (!this._tab || !this._tab.linkedBrowser) {
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
    return this._tab?.linkedBrowser?.docShellIsActive ? "[A]" : "";
  }
}
