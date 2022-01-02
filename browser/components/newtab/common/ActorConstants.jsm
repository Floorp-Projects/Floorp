/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const MESSAGE_TYPE_LIST = [
  "BLOCK_MESSAGE_BY_ID",
  "USER_ACTION",
  "IMPRESSION",
  "TRIGGER",
  "NEWTAB_MESSAGE_REQUEST",
  "DOORHANGER_TELEMETRY",
  "TOOLBAR_BADGE_TELEMETRY",
  "TOOLBAR_PANEL_TELEMETRY",
  "MOMENTS_PAGE_TELEMETRY",
  "INFOBAR_TELEMETRY",
  "SPOTLIGHT_TELEMETRY",
  "AS_ROUTER_TELEMETRY_USER_EVENT",

  // Admin types
  "ADMIN_CONNECT_STATE",
  "UNBLOCK_MESSAGE_BY_ID",
  "UNBLOCK_ALL",
  "BLOCK_BUNDLE",
  "UNBLOCK_BUNDLE",
  "DISABLE_PROVIDER",
  "ENABLE_PROVIDER",
  "EVALUATE_JEXL_EXPRESSION",
  "EXPIRE_QUERY_CACHE",
  "FORCE_ATTRIBUTION",
  "FORCE_WHATSNEW_PANEL",
  "CLOSE_WHATSNEW_PANEL",
  "OVERRIDE_MESSAGE",
  "MODIFY_MESSAGE_JSON",
  "RESET_PROVIDER_PREF",
  "SET_PROVIDER_USER_PREF",
  "RESET_GROUPS_STATE",
];

const MESSAGE_TYPE_HASH = MESSAGE_TYPE_LIST.reduce((hash, value) => {
  hash[value] = value;
  return hash;
}, {});

const EXPORTED_SYMBOLS = ["MESSAGE_TYPE_LIST", "MESSAGE_TYPE_HASH"];
