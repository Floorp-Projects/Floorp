/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozPanelWrappedElement } from "./panel-wrapped-element.mjs";
import { TabPreviewThumbnail } from "./tabpreview-thumbnail.mjs";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const TAB_PREVIEW_USE_THUMBNAILS_PREF =
  "browser.tabs.cardPreview.showThumbnails";

/**
 * Detailed preview card that displays when hovering a tab
 *
 * @property {MozTabbrowserTab} tab - the tab to preview
 * @fires TabPreview#previewhidden
 * @fires TabPreview#previewshown
 * @fires TabPreview#previewThumbnailUpdated
 */
export default class TabPreview extends MozPanelWrappedElement {
  static properties = {
    tab: { type: Object },

    _previewsActive: { type: Boolean, state: true },
    _previewDelayTimeout: { type: Number, state: true },
    _previewDelayTimeoutActive: { type: Boolean, state: true },
    _displayImg: { type: Object, state: true },
  };

  constructor() {
    super();
    this.panelID = "tabpreviewpanel";
    this.noautofocus = true;
    this.norolluponanchor = true;
    this.rolluponmousewheel = true;
    this.consumeoutsideclicks = false;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefPreviewDelay",
      "ui.tooltip.delay_ms"
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefDisplayThumbnail",
      TAB_PREVIEW_USE_THUMBNAILS_PREF,
      false
    );
    this._previewDelayTimeoutActive = true;
  }

  getPrettyURI(uri) {
    try {
      const url = new URL(uri);
      return `${url.hostname}`.replace(/^w{3}\./, "");
    } catch {
      return uri;
    }
  }

  handleEvent(e) {
    if (e.type === "TabSelect") {
      this.requestUpdate();
    }
  }

  activate() {
    this._previewsActive = true;
  }

  deactivate() {
    this._previewsActive = false;
    this._previewDelayTimeoutActive = true;
  }

  on_popuphidden() {
    window.removeEventListener("TabSelect", this);

    /**
     * @event TabPreview#previewhidden
     * @type {CustomEvent}
     */
    this.dispatchEvent(new CustomEvent("previewhidden"));
  }

  on_popupshown() {
    window.addEventListener("TabSelect", this);
    this.dispatchEvent(
      /**
       * @event TabPreview#previewshown
       * @type {CustomEvent}
       * @property {object} detail
       * @property {MozTabbrowserTab} detail.tab - the tab being previewed
       */
      new CustomEvent("previewshown", {
        detail: { tab: this.tab },
      })
    );
  }

  get _displayTitle() {
    if (!this.tab) {
      return "";
    }
    return this.tab.textLabel.textContent;
  }

  get _displayURI() {
    if (!this.tab) {
      return "";
    }
    return this.getPrettyURI(this.tab.linkedBrowser.currentURI.spec);
  }

  willUpdate() {
    if (this.tab && this._previewsActive) {
      clearTimeout(this._previewDelayTimeout);

      this._previewDelayTimeout = setTimeout(
        () => {
          this.panelAnchor = this.tab;
          this.panelState = "open";
          this._previewDelayTimeoutActive = false;
        },
        this._previewDelayTimeoutActive ? this._prefPreviewDelay : 0
      );
    } else {
      clearTimeout(this._previewDelayTimeout);
      this.panelState = "closed";
    }
  }

  get thumbnailContainer() {
    return this.renderRoot.querySelector("tabpreview-thumbnail");
  }

  render() {
    return html`
      <link
        rel="stylesheet"
        type="text/css"
        href="chrome://browser/content/tabpreview/tabpreview.css"
      />
      <div class="tab-preview-container">
        <div class="tab-preview-text-container">
          <div class="tab-preview-title">${this._displayTitle}</div>
          <div class="tab-preview-uri">${this._displayURI}</div>
        </div>
        ${this._prefDisplayThumbnail && this.tab && !this.tab.selected
          ? html`<div class="tab-preview-thumbnail-container">
              <tabpreview-thumbnail
                .tab=${this.tab}
                .onThumbnailUpdated=${thumbnail => {
                  /**
                   * fires when the thumbnail for a preview is loaded
                   * and added to the document.
                   *
                   * @event TabPreview#previewThumbnailUpdated
                   * @type {CustomEvent}
                   */
                  this.dispatchEvent(
                    new CustomEvent("previewThumbnailUpdated", {
                      detail: {
                        thumbnail,
                      },
                    })
                  );
                }}
              ></tabpreview-thumbnail>
            </div>`
          : ""}
      </div>
    `;
  }
}

customElements.define("tabpreview-thumbnail", TabPreviewThumbnail);
customElements.define("tab-preview", TabPreview);
