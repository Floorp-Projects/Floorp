/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "StyleEditorDefinition" ];

const Cu = Components.utils;
const STRINGS_URI = "chrome://browser/locale/devtools/styleeditor.properties";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/devtools/StyleEditorChrome.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "_strings",
  function() Services.strings.createBundle(STRINGS_URI));

XPCOMUtils.defineLazyModuleGetter(this, "StyleEditorPanel",
  "resource:///modules/devtools/StyleEditorPanel.jsm");

/**
 * The external API allowing us to be registered with DevTools.jsm
 */
this.StyleEditorDefinition = {
  id: "styleeditor",
  key: l10n("open.commandkey"),
  ordinal: 3,
  accesskey: l10n("open.accesskey"),
  modifiers: "shift",
  label: l10n("ToolboxStyleEditor.label"),
  url: "chrome://browser/content/styleeditor.xul",

  isTargetSupported: function(target) {
    return !target.isRemote && !target.isChrome;
  },

  build: function(iframeWindow, toolbox) {
    return new StyleEditorPanel(iframeWindow, toolbox);
  }
};

/**
 * Lookup l10n string from a string bundle.
 * @param {string} aName The key to lookup.
 * @returns A localized version of the given key.
 */
function l10n(aName)
{
  try {
    return _strings.GetStringFromName(aName);
  } catch (ex) {
    Services.console.logStringMessage("Error reading '" + aName + "'");
    throw new Error("l10n error with " + aName);
  }
}
