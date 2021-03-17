/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

var { ExtensionError } = ExtensionUtils;

this.search = class extends ExtensionAPI {
  getAPI(context) {
    return {
      search: {
        async get() {
          await searchInitialized;
          let visibleEngines = await Services.search.getVisibleEngines();
          let defaultEngine = await Services.search.getDefault();
          return Promise.all(
            visibleEngines.map(async engine => {
              let favIconUrl;
              if (engine.iconURI) {
                // Convert moz-extension:-URLs to data:-URLs to make sure that
                // extensions can see icons from other extensions, even if they
                // are not web-accessible.
                // Also prevents leakage of extension UUIDs to other extensions..
                if (
                  engine.iconURI.schemeIs("moz-extension") &&
                  engine.iconURI.host !== context.extension.uuid
                ) {
                  favIconUrl = await ExtensionUtils.makeDataURI(
                    engine.iconURI.spec
                  );
                } else {
                  favIconUrl = engine.iconURI.spec;
                }
              }

              return {
                name: engine.name,
                isDefault: engine.name === defaultEngine.name,
                alias: engine.alias || undefined,
                favIconUrl,
              };
            })
          );
        },

        async search(searchProperties) {
          await searchInitialized;
          let engine;
          if (searchProperties.engine) {
            engine = Services.search.getEngineByName(searchProperties.engine);
            if (!engine) {
              throw new ExtensionError(
                `${searchProperties.engine} was not found`
              );
            }
          }

          const tab = searchProperties.tabId
            ? tabTracker.getTab(searchProperties.tabId)
            : null;

          await windowTracker.topWindow.BrowserSearch.loadSearchFromExtension(
            searchProperties.query,
            engine,
            tab,
            context.principal
          );
        },
      },
    };
  }
};
