"use strict";

module.exports = {  // eslint-disable-line no-undef
  "extends": "../../../toolkit/components/extensions/.eslintrc.js",

  "globals": {
    "EventEmitter": true,
    "IconDetails": true,
    "Tab": true,
    "TabContext": true,
    "Window": true,
    "WindowEventManager": true,
    "browserActionFor": true,
    "getCookieStoreIdForTab": true,
    "getDevToolsTargetForContext": true,
    "getTargetTabIdForToolbox": true,
    "makeWidgetId": true,
    "pageActionFor": true,
    "sidebarActionFor": true,
    "tabTracker": true,
    "windowTracker": true,
  },
};
