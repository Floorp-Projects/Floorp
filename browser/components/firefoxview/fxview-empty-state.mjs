/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  classMap,
  repeat,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * An empty state card to be used throughout Firefox View
 *
 * @property {string} headerIconUrl - (Optional) The chrome:// url for an icon to be displayed within the header
 * @property {string} headerLabel - (Optional) The l10n id for the header text for the empty/error state
 * @property {object} headerArgs - (Optional) The l10n args for the header text for the empty/error state
 * @property {string} isInnerCard - (Optional) True if the card is displayed within another card and needs a border instead of box shadow
 * @property {boolean} isSelectedTab - (Optional) True if the component is the selected navigation tab - defaults to false
 * @property {Array} descriptionLabels - (Optional) An array of l10n ids for the secondary description text for the empty/error state
 * @property {object} descriptionLink - (Optional) An object describing the l10n name and url needed within a description label
 * @property {string} mainImageUrl - (Optional) The chrome:// url for the main image of the empty/error state
 * @property {string} errorGrayscale - (Optional) The image should be shown in gray scale
 */
class FxviewEmptyState extends MozLitElement {
  constructor() {
    super();
    this.isSelectedTab = false;
    this.descriptionLabels = [];
    this.headerArgs = {};
  }

  static properties = {
    headerLabel: { type: String },
    headerArgs: { type: Object },
    headerIconUrl: { type: String },
    isInnerCard: { type: Boolean },
    isSelectedTab: { type: Boolean },
    descriptionLabels: { type: Array },
    desciptionLink: { type: Object },
    mainImageUrl: { type: String },
    errorGrayscale: { type: Boolean },
  };

  static queries = {
    headerEl: ".header",
    descriptionEls: { all: ".description" },
  };

  linkTemplate(descriptionLink) {
    if (!descriptionLink) {
      return html``;
    }
    return html` <a
      aria-details="card-container"
      data-l10n-name=${descriptionLink.name}
      href=${descriptionLink.url}
      target=${descriptionLink?.sameTarget ? "_self" : "_blank"}
    />`;
  }

  render() {
    return html`
       <link
         rel="stylesheet"
         href="chrome://browser/content/firefoxview/fxview-empty-state.css"
       />
       <card-container hideHeader="true" exportparts="image" ?isInnerCard="${
         this.isInnerCard
       }" id="card-container" isEmptyState="true">
         <div slot="main" class=${classMap({
           selectedTab: this.isSelectedTab,
           imageHidden: !this.mainImageUrl,
         })}>
           <div class="image-container">
             <img class=${classMap({
               image: true,
               greyscale: this.errorGrayscale,
             })}
              role="presentation"
              alt=""
              ?hidden=${!this.mainImageUrl}
                       src=${this.mainImageUrl}/>
           </div>
           <div class="main">
             <h2
               id="header"
               class="header"
               ?hidden=${!this.headerLabel}
             >
                 <img class="icon info"
                   data-l10n-id="firefoxview-empty-state-icon"
                   ?hidden=${!this.headerIconUrl}
                   src=${ifDefined(this.headerIconUrl)}></img>
                 <span
                   data-l10n-id="${this.headerLabel}"
                   data-l10n-args="${JSON.stringify(this.headerArgs)}">
                 </span>
             </h2>
             ${repeat(
               this.descriptionLabels,
               descLabel => descLabel,
               (descLabel, index) => html`<p
                 class=${classMap({
                   description: true,
                   secondary: index !== 0,
                 })}
                 data-l10n-id="${descLabel}"
               >
                 ${this.linkTemplate(this.descriptionLink)}
               </p>`
             )}
             <slot name="primary-action"></slot>
           </div>
         </div>
       </card-container>
     `;
  }
}
customElements.define("fxview-empty-state", FxviewEmptyState);
