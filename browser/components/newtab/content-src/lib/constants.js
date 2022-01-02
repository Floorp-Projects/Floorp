/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const IS_NEWTAB =
  global.document && global.document.documentURI === "about:newtab";
export const NEWTAB_DARK_THEME = {
  ntp_background: {
    r: 42,
    g: 42,
    b: 46,
    a: 1,
  },
  ntp_card_background: {
    r: 66,
    g: 65,
    b: 77,
    a: 1,
  },
  ntp_text: {
    r: 249,
    g: 249,
    b: 250,
    a: 1,
  },
  sidebar: {
    r: 56,
    g: 56,
    b: 61,
    a: 1,
  },
  sidebar_text: {
    r: 249,
    g: 249,
    b: 250,
    a: 1,
  },
};
