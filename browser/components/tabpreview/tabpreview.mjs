/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const TAB_PREVIEW_USE_THUMBNAILS_PREF =
  "browser.tabs.cardPreview.showThumbnails";
const TAB_PREVIEW_DELAY_PREF = "browser.tabs.cardPreview.delayMs";

/**
 * Detailed preview card that displays when hovering a tab
 *
 * @property {MozTabbrowserTab} tab - the tab to preview
 * @fires TabPreview#previewhidden
 * @fires TabPreview#previewshown
 * @fires TabPreview#previewThumbnailUpdated
 */
export default class TabPreview extends MozLitElement {
  static properties = {
    tab: { type: Object },

    _previewIsActive: { type: Boolean, state: true },
    _previewDelayTimeout: { type: Number, state: true },
    _displayTitle: { type: String, state: true },
    _displayURI: { type: String, state: true },
    _displayImg: { type: Object, state: true },
  };

  constructor() {
    super();
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefPreviewDelay",
      TAB_PREVIEW_DELAY_PREF,
      1000
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_prefDisplayThumbnail",
      TAB_PREVIEW_USE_THUMBNAILS_PREF,
      false
    );
  }

  // render this inside a <panel>
  createRenderRoot() {
    if (!document.createXULElement) {
      console.error(
        "Unable to create panel: document.createXULElement is not available"
      );
      return super.createRenderRoot();
    }
    this.attachShadow({ mode: "open" });
    this.panel = document.createXULElement("panel");
    this.panel.setAttribute("id", "tabPreviewPanel");
    this.panel.setAttribute("noautofocus", true);
    this.panel.setAttribute("norolluponanchor", true);
    this.panel.setAttribute("consumeoutsideclicks", "never");
    this.panel.setAttribute("level", "parent");
    this.shadowRoot.append(this.panel);
    return this.panel;
  }

  get previewCanShow() {
    return this._previewIsActive && this.tab;
  }

  get thumbnailCanShow() {
    return (
      this.previewCanShow &&
      this._prefDisplayThumbnail &&
      !this.tab.selected &&
      this._displayImg
    );
  }

  getPrettyURI(uri) {
    try {
      const url = new URL(uri);
      return `${url.hostname}${url.pathname}`.replace(/\/+$/, "");
    } catch {
      return this.pageURI;
    }
  }

  handleEvent(e) {
    if (e.type == "TabSelect") {
      this.requestUpdate();
    }
  }

  connectedCallback() {
    super.connectedCallback();
    window.addEventListener("TabSelect", this);
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener("TabSelect", this);
  }

  // compute values derived from tab element
  willUpdate(changedProperties) {
    if (!changedProperties.has("tab")) {
      return;
    }
    if (!this.tab) {
      this._displayTitle = "";
      this._displayURI = "";
      this._displayImg = null;
      return;
    }
    this._displayTitle = this.tab.textLabel.textContent;
    this._displayURI = this.getPrettyURI(
      this.tab.linkedBrowser.currentURI.spec
    );
    this._displayImg = null;
    let { tab } = this;
    window.tabPreviews.get(this.tab).then(el => {
      if (this.tab == tab) {
        this._displayImg = el;
      }
    });
  }

  updated(changedProperties) {
    if (changedProperties.has("tab")) {
      // handle preview delay
      if (!this.tab) {
        clearTimeout(this._previewDelayTimeout);
        this._previewIsActive = false;
      } else {
        let lastTabVal = changedProperties.get("tab");
        if (!lastTabVal) {
          // tab was set from an empty state,
          // so wait for the delay duration before showing
          this._previewDelayTimeout = setTimeout(() => {
            this._previewIsActive = true;
          }, this._prefPreviewDelay);
        }
      }
    }
    if (changedProperties.has("_previewIsActive")) {
      if (!this._previewIsActive) {
        this.panel.hidePopup();
        this.updateComplete.then(() => {
          /**
           * @event TabPreview#previewhidden
           * @type {CustomEvent}
           */
          this.dispatchEvent(new CustomEvent("previewhidden"));
        });
      }
    }
    if (
      (changedProperties.has("tab") ||
        changedProperties.has("_previewIsActive")) &&
      this.previewCanShow
    ) {
      this.updateComplete.then(() => {
        if (this.panel.state == "open" || this.panel.state == "showing") {
          this.panel.moveToAnchor(this.tab, "bottomleft topleft", 0, -2);
        } else {
          this.panel.openPopup(this.tab, {
            position: "bottomleft topleft",
            y: -2,
            isContextMenu: false,
          });
        }

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
      });
    }
    if (changedProperties.has("_displayImg")) {
      this.updateComplete.then(() => {
        /**
         * fires when the thumbnail for a preview is loaded
         * and added to the document.
         *
         * @event TabPreview#previewThumbnailUpdated
         * @type {CustomEvent}
         */
        this.dispatchEvent(new CustomEvent("previewThumbnailUpdated"));
      });
    }
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
        ${this.thumbnailCanShow
          ? html`
              <div class="tab-preview-thumbnail-container">
                ${this._displayImg}
              </div>
            `
          : ""}
      </div>
    `;
  }
}
customElements.define("tab-preview", TabPreview);
