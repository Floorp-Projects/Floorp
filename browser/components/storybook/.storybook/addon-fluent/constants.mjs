/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ADDON_ID = "addon-fluent";
export const PANEL_ID = `${ADDON_ID}/fluentPanel`;
export const TOOL_ID = `${ADDON_ID}/toolbarButton`;

export const STRATEGY_DEFAULT = "default";
export const STRATEGY_ACCENTED = "accented";
export const STRATEGY_BIDI = "bidi";

export const PSEUDO_STRATEGIES = [
  STRATEGY_DEFAULT,
  STRATEGY_ACCENTED,
  STRATEGY_BIDI,
];

export const DIRECTIONS = {
  ltr: "ltr",
  rtl: "rtl",
};

export const DIRECTION_BY_STRATEGY = {
  [STRATEGY_DEFAULT]: DIRECTIONS.ltr,
  [STRATEGY_ACCENTED]: DIRECTIONS.ltr,
  [STRATEGY_BIDI]: DIRECTIONS.rtl,
};

export const UPDATE_STRATEGY_EVENT = "update-strategy";
export const FLUENT_SET_STRINGS = "fluent-set-strings";
export const FLUENT_CHANGED = "fluent-changed";
