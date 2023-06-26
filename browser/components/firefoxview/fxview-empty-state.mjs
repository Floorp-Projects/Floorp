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
 * @property {boolean} isSelectedTab - (Optional) True if the component is the selected navigation tab - defaults to false
 * @property {string} descriptionLabels - (Required) An array of l10n id for the secondary description text for the empty/error state
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
    isSelectedTab: { type: Boolean },
    descriptionLabel: { type: String },
    mainImageUrl: { type: String },
  };

  static queries = {
    headerEl: ".header",
    descriptionEl: ".description",
  };

  render() {
    return html`
       <link
         rel="stylesheet"
         href="chrome://browser/content/firefoxview/fxview-empty-state.css"
       />
       <card-container hideHeader="true" exportparts="image">
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
                 data-l10n-id=${descLabel}
               ></p>`
             )}
             <slot name="primary-action"></slot>
           </div>
         </div>
       </card-container>
     `;
  }
}
customElements.define("fxview-empty-state", FxviewEmptyState);
