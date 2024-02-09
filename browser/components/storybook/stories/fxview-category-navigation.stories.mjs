/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/firefoxview/fxview-category-navigation.mjs";

export default {
  title: "Domain-specific UI Widgets/Firefox View/Category Navigation",
  component: "fxview-category-navigation",
  parameters: {
    status: "in-development",
    actions: {
      handles: ["change-category"],
    },
    fluent: `
fxview-category-button-one = Category 1
  .title = Category 1
fxview-category-button-two = Category 2
  .title = Category 2
fxview-category-button-three = Category 3
  .title = Category 3
fxview-category-footer-button = Settings
  .title = Settings
     `,
  },
};

const Template = () => html`
  <style>
    #page {
      height: 100%;
      display: grid;
      grid-template-columns: var(--in-content-sidebar-width) 1fr;
    }
    fxview-category-navigation {
      margin-inline-start: 10px;
    }
    fxview-category-button[name="category-one"]::part(icon) {
      background-image: url("chrome://browser/skin/preferences/category-general.svg");
    }
    fxview-category-button[name="category-two"]::part(icon) {
      background-image: url("chrome://browser/skin/preferences/category-general.svg");
    }
    fxview-category-button[name="category-three"]::part(icon) {
      background-image: url("chrome://browser/skin/preferences/category-general.svg");
    }
    .footer-button {
      display: flex;
      gap: 12px;
      font-weight: normal;
      min-width: unset;
      padding: 8px;
      margin: 0;
    }
    .footer-button .cat-icon {
      background-image: url("chrome://browser/skin/preferences/category-general.svg");
      background-color: initial;
      background-size: 20px;
      background-repeat: no-repeat;
      background-position: center;
      height: 20px;
      width: 20px;
      display: inline-block;
      -moz-context-properties: fill;
      fill: currentColor;
    }
    @media (max-width: 52rem) {
      #page {
        grid-template-columns: 82px 1fr;
      }
      .cat-name {
        display: none;
      }
    }
  </style>
  <div id="page">
    <fxview-category-navigation>
      <h2 slot="category-nav-header">Header</h2>
      <fxview-category-button
        slot="category-button"
        name="category-one"
        data-l10n-id="fxview-category-button-one"
      >
      </fxview-category-button>
      <fxview-category-button
        slot="category-button"
        name="category-two"
        data-l10n-id="fxview-category-button-two"
      >
      </fxview-category-button>
      <fxview-category-button
        slot="category-button"
        name="category-three"
        data-l10n-id="fxview-category-button-three"
      >
      </fxview-category-button>
      <div slot="category-nav-footer" class="category-nav-footer">
        <button class="footer-button ghost-button">
          <span class="cat-icon"></span>
          <span
            class="cat-name"
            data-l10n-id="fxview-category-footer-button"
          ></span>
        </button>
      </div>
    </fxview-category-navigation>
  </div>
`;

export const Default = Template.bind({});
Default.args = {};
