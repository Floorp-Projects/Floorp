/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * The default localization just returns the last part of the key
 * (all after the last dot).
 */
const DefaultL10N = {
  getStr: function (key) {
    let index = key.lastIndexOf(".");
    return key.substr(index + 1);
  }
};

/**
 * The 'l10n' object is set by main.js in case the DOM panel content
 * runs within a scope with chrome privileges.
 *
 * Note that DOM panel content can also run within a scope with no chrome
 * privileges, e.g. in an iframe with type 'content' or in a browser tab,
 * which allows using our own tools for development.
 */
exports.l10n = window.l10n || DefaultL10N;
