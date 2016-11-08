"use strict";

module.exports = {  // eslint-disable-line no-undef
  "extends": "../../../../../testing/mochitest/browser.eslintrc.js",

  "env": {
    "webextensions": true,
  },

  "globals": {
    "NetUtil": true,
    "XPCOMUtils": true,
    "Task": true,

    // Browser window globals.
    "PanelUI": false,

    // Test harness globals
    "ExtensionTestUtils": false,
    "TestUtils": false,

    "clickBrowserAction": true,
    "clickPageAction": true,
    "closeContextMenu": true,
    "closeExtensionContextMenu": true,
    "focusWindow": true,
    "makeWidgetId": true,
    "openContextMenu": true,
    "openExtensionContextMenu": true,
    "CustomizableUI": true,
  },

  "rules": {
    "no-shadow": 0,
  },
};
