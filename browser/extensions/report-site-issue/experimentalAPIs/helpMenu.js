/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services */

const TOPIC = "report-site-issue";

this.helpMenu = class extends ExtensionAPI {
  getAPI(context) {
    const { tabManager } = context.extension;
    let EventManager = ExtensionCommon.EventManager;

    return {
      helpMenu: {
        onHelpMenuCommand: new EventManager({
          context,
          name: "helpMenu",
          register: fire => {
            let observer = subject => {
              let nativeTab = subject.wrappedJSObject;
              let tab = tabManager.convert(nativeTab);
              fire.async(tab);
            };

            Services.obs.addObserver(observer, TOPIC);

            return () => {
              Services.obs.removeObserver(observer, TOPIC);
            };
          },
        }).api(),
      },
    };
  }
};
