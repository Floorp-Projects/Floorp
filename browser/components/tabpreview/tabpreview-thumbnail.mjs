/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * TabPreviewThumbnail communicates with the tabPreviews service
 * to retrieve and display the thumbnail corresponding to the tab
 * specified by the `tab` property.
 *
 * @property {MozTabbrowserTab} tab the tab to display a thumbnail for
 * @property {Function} onThumbnailUpdated this function will be called after
 *   a thumbnail element has been retrieved from the service and rendered in the document.
 */
export class TabPreviewThumbnail extends MozLitElement {
  static properties = {
    tab: {
      type: Object,
      hasChanged: () => {
        return true;
      },
    },
    _previewImg: { type: Object, state: true },
    onThumbnailUpdated: { type: Function },
  };

  get thumbnailCanShow() {
    return this._previewImg;
  }

  willUpdate(changedProperties) {
    if (changedProperties.has("tab")) {
      this._previewImg = null;
      if (!this.tab) {
        return;
      }
      let { tab } = this;
      window.tabPreviews.get(this.tab).then(el => {
        if (this.tab == tab) {
          this._previewImg = el;
        }
      });
    }
  }

  updated(changedProperties) {
    if (changedProperties.has("_previewImg") && this.onThumbnailUpdated) {
      this.updateComplete.then(() => this.onThumbnailUpdated(this._previewImg));
    }
  }

  render() {
    return html`<style>
        img,
        canvas {
          display: block;
          width: 100%;
        }</style
      >${this._previewImg ?? ""}`;
  }
}
