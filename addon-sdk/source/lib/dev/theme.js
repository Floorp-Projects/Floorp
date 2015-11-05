/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Class } = require("../sdk/core/heritage");
const { EventTarget } = require("../sdk/event/target");
const { Disposable, setup, dispose } = require("../sdk/core/disposable");
const { contract, validate } = require("../sdk/util/contract");
const { id: addonID } = require("../sdk/self");
const { onEnable, onDisable } = require("dev/theme/hooks");
const { isString, instanceOf, isFunction } = require("sdk/lang/type");
const { add } = require("sdk/util/array");
const { data } = require("../sdk/self");
const { isLocalURL } = require("../sdk/url");

const makeID = name =>
  ("dev-theme-" + addonID + (name ? "-" + name : "")).
  split(/[ . /]/).join("-").
  replace(/[^A-Za-z0-9_\-]/g, "");

const Theme = Class({
  extends: Disposable,
  implements: [EventTarget],

  initialize: function(options) {
    this.name = options.name;
    this.label = options.label;
    this.styles = options.styles;

    // Event handlers
    this.onEnable = options.onEnable;
    this.onDisable = options.onDisable;
  },
  get id() {
    return makeID(this.name || this.label);
  },
  setup: function() {
    // Any initialization steps done at the registration time.
  },
  getStyles: function() {
    if (!this.styles) {
      return [];
    }

    if (isString(this.styles)) {
      if (isLocalURL(this.styles)) {
        return [data.url(this.styles)];
      }
    }

    let result = [];
    for (let style of this.styles) {
      if (isString(style)) {
        if (isLocalURL(style)) {
          style = data.url(style);
        }
        add(result, style);
      } else if (instanceOf(style, Theme)) {
        result = result.concat(style.getStyles());
      }
    }
    return result;
  },
  getClassList: function() {
    let result = [];
    for (let style of this.styles) {
      if (instanceOf(style, Theme)) {
        result = result.concat(style.getClassList());
      }
    }

    if (this.name) {
      add(result, this.name);
    }

    return result;
  }
});

exports.Theme = Theme;

// Initialization & dispose

setup.define(Theme, (theme) => {
  theme.classList = [];
  theme.setup();
});

dispose.define(Theme, function(theme) {
  theme.dispose();
});

// Validation

validate.define(Theme, contract({
  label: {
    is: ["string"],
    msg: "The `option.label` must be a provided"
  },
}));

// Support theme events: apply and unapply the theme.

onEnable.define(Theme, (theme, {window, oldTheme}) => {
  if (isFunction(theme.onEnable)) {
    theme.onEnable(window, oldTheme);
  }
});

onDisable.define(Theme, (theme, {window, newTheme}) => {
  if (isFunction(theme.onDisable)) {
    theme.onDisable(window, newTheme);
  }
});

// Support for built-in themes

const LightTheme = Theme({
  name: "theme-light",
  styles: "chrome://devtools/skin/light-theme.css",
});

const DarkTheme = Theme({
  name: "theme-dark",
  styles: "chrome://devtools/skin/dark-theme.css",
});

exports.LightTheme = LightTheme;
exports.DarkTheme = DarkTheme;
