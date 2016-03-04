/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");

const { defaultTools, defaultThemes } = require("devtools/client/definitions");

defaultTools.forEach(definition => gDevTools.registerTool(definition));
defaultThemes.forEach(definition => gDevTools.registerTheme(definition));


Object.defineProperty(exports, "Toolbox", {
  get: () => require("devtools/client/framework/toolbox").Toolbox
});
Object.defineProperty(exports, "TargetFactory", {
  get: () => require("devtools/client/framework/target").TargetFactory
});

const unloadObserver = {
  observe: function(subject) {
    if (subject.wrappedJSObject === require("@loader/unload")) {
      Services.obs.removeObserver(unloadObserver, "sdk:loader:destroy");
      for (let definition of gDevTools.getToolDefinitionArray()) {
        gDevTools.unregisterTool(definition.id);
      }
      for (let definition of gDevTools.getThemeDefinitionArray()) {
        gDevTools.unregisterTheme(definition.id);
      }
    }
  }
};
Services.obs.addObserver(unloadObserver, "sdk:loader:destroy", false);

const events = require("sdk/system/events");
events.emit("devtools-loaded", {});
