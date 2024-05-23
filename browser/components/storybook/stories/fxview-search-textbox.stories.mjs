/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { action } from "@storybook/addon-actions";

import { html, ifDefined } from "lit.all.mjs";
import "browser/components/firefoxview/fxview-search-textbox.mjs";

export default {
  title: "Domain-specific UI Widgets/Firefox View/Search Textbox",
  component: "fxview-search-textbox",
  argTypes: {
    size: {
      control: { type: "number", min: 1, step: 1 },
    },
  },
};

const Template = ({ placeholder, size }) => html`
  <style>
    fxview-search-textbox {
      --fxview-border: var(--in-content-border-color);
      --fxview-text-primary-color: var(--in-content-page-color);
      --fxview-element-background-hover: var(
        --in-content-button-background-hover
      );
      --fxview-text-color-hover: var(--in-content-button-text-color-hover);
    }
  </style>
  <fxview-search-textbox
    placeholder=${ifDefined(placeholder)}
    size=${ifDefined(size)}
    @fxview-search-textbox-query=${action("fxview-search-textbox-query")}
  ></fxview-search-textbox>
`;

export const Default = Template.bind({});
Default.args = {
  placeholder: "",
};

export const SearchBoxWithPlaceholder = Template.bind({});
SearchBoxWithPlaceholder.args = {
  ...Default.args,
  placeholder: "Search",
};

export const SearchBoxWithCustomSize = Template.bind({});
SearchBoxWithCustomSize.args = {
  ...Default.args,
  size: 32,
};
