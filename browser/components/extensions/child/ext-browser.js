/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

extensions.registerModules({
  devtools: {
    url: "chrome://browser/content/child/ext-devtools.js",
    scopes: ["devtools_child"],
    paths: [["devtools"]],
  },
  devtools_inspectedWindow: {
    url: "chrome://browser/content/child/ext-devtools-inspectedWindow.js",
    scopes: ["devtools_child"],
    paths: [["devtools", "inspectedWindow"]],
  },
  devtools_panels: {
    url: "chrome://browser/content/child/ext-devtools-panels.js",
    scopes: ["devtools_child"],
    paths: [["devtools", "panels"]],
  },
  devtools_network: {
    url: "chrome://browser/content/child/ext-devtools-network.js",
    scopes: ["devtools_child"],
    paths: [["devtools", "network"]],
  },
  // Because of permissions, the module name must differ from both namespaces.
  menusInternal: {
    url: "chrome://browser/content/child/ext-menus.js",
    scopes: ["addon_child"],
    paths: [["contextMenus"], ["menus"]],
  },
  menusChild: {
    url: "chrome://browser/content/child/ext-menus-child.js",
    scopes: ["addon_child", "devtools_child"],
    paths: [["menus"]],
  },
  omnibox: {
    url: "chrome://browser/content/child/ext-omnibox.js",
    scopes: ["addon_child"],
    paths: [["omnibox"]],
  },
  tabs: {
    url: "chrome://browser/content/child/ext-tabs.js",
    scopes: ["addon_child"],
    paths: [["tabs"]],
  },
});
