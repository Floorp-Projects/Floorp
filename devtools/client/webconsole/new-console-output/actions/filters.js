/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  FILTER_TEXT_SET,
  FILTER_TOGGLE,
  FILTERS_CLEAR
} = require("../constants");

function filterTextSet(text) {
  return {
    type: FILTER_TEXT_SET,
    text
  };
}

function filterToggle(filter) {
  return {
    type: FILTER_TOGGLE,
    filter,
  };
}

function filtersClear() {
  return {
    type: FILTERS_CLEAR
  };
}

module.exports = {
  filterTextSet,
  filterToggle,
  filtersClear
};
