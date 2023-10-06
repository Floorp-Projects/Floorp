/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html, ifDefined } from "chrome://global/content/vendor/lit.all.mjs";

export const stylesTemplate = () =>
  html`
    <link rel="stylesheet" href="chrome://global/skin/in-content/common.css" />
    <link
      rel="stylesheet"
      href="chrome://browser/content/aboutlogins/components/input-field/input-field.css"
    />
  `;

export const editableFieldTemplate = ({
  type,
  value,
  inputId,
  disabled,
  onFocus,
  onBlur,
}) =>
  html`<input
    class="input-field"
    data-l10n-id=${ifDefined(inputId)}
    type=${type}
    value=${value}
    ?disabled=${disabled}
    @focus=${onFocus}
    @blur=${onBlur}
  />`;
