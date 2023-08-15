/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  html,
  ifDefined,
  map,
} from "chrome://global/content/vendor/lit.all.mjs";
import { MozLitElement } from "chrome://global/content/lit-utils.mjs";

/**
 * An empty state card to be used throughout Firefox View
 *
 * @property {string} headerIconUrl - (Optional) The chrome:// url for an icon to be displayed within the header
 * @property {string} headerLabel - (Optional) The l10n id for the header text for the empty/error state
 * @property {string} isInnerCard - (Optional) True if the card is displayed within another card and needs a border instead of box shadow
 * @property {boolean} isSelectedTab - (Optional) True if the component is the selected navigation tab - defaults to false
 * @property {Array} descriptionLabels - (Required) An array of l10n ids for the secondary description text for the empty/error state
 * @property {object} descriptionLink - (Optional) An object describing the l10n name and url needed within a description label
 * @property {string} mainImageUrl - (Optional) The chrome:// url for the main image of the empty/error state
 */
class FxviewEmptyState extends MozLitElement {
  constructor() {
    super();
    this.isSelectedTab = false;
  }

  static properties = {
    headerLabel: { type: String },
    headerIconUrl: { type: String },
    isInnerCard: { type: Boolean },
    isSelectedTab: { type: Boolean },
    descriptionLabels: { type: Array },
    desciptionLink: { type: Object },
    mainImageUrl: { type: String },
  };

  static queries = {
    headerEl: ".header",
    descriptionEls: { all: ".description" },
  };

  render() {
    return html`
       <link
         rel="stylesheet"
         href="chrome://browser/content/firefoxview/fxview-empty-state.css"
       />
       <card-container hideHeader="true" exportparts="image" ?isInnerCard="${
         this.isInnerCard
       }">
         <div slot="main" class=${this.isSelectedTab ? "selectedTab" : null}>
           <img class="image" role="presentation" alt="" ?hidden=${!this
             .mainImageUrl} src=${this.mainImageUrl}/>
           <div class="main">
             <h2
               id="header"
               class="header"
               ?hidden=${!this.headerLabel}
             >
                 <img class="icon info" ?hidden=${!this
                   .headerIconUrl} src=${ifDefined(this.headerIconUrl)}></img>
                 <span data-l10n-id="${this.headerLabel}"></span>
             </h2>
             ${map(
               this.descriptionLabels,
               (descLabel, index) => html`<p
                 class="description ${index !== 0 ? "secondary" : null}"
                 data-l10n-id="${descLabel}"
               >
                 <a
                   ?hidden=${!this.descriptionLink}
                   data-l10n-name=${ifDefined(this.descriptionLink?.name)}
                   href=${ifDefined(this.descriptionLink?.url)}
                   target=${this.descriptionLink?.sameTarget
                     ? "_self"
                     : "_blank"}
                 />
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
