/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

var { ExtensionError } = ExtensionUtils;

const dispositionMap = {
  CURRENT_TAB: "current",
  NEW_TAB: "tab",
  NEW_WINDOW: "window",
};

this.search = class extends ExtensionAPI {
  getAPI(context) {
    function getTarget({ tabId, disposition, defaultDisposition }) {
      let tab, where;
      if (disposition) {
        if (tabId) {
          throw new ExtensionError(`Cannot set both 'disposition' and 'tabId'`);
        }
        where = dispositionMap[disposition];
      } else if (tabId) {
        tab = tabTracker.getTab(tabId);
      } else {
        where = dispositionMap[defaultDisposition];
      }
      return { tab, where };
    }

    return {
      search: {
        async get() {
          await Services.search.promiseInitialized;
          let visibleEngines = await Services.search.getVisibleEngines();
          let defaultEngine = await Services.search.getDefault();
          return Promise.all(
            visibleEngines.map(async engine => {
              let favIconUrl = engine.getIconURL();
              // Convert moz-extension:-URLs to data:-URLs to make sure that
              // extensions can see icons from other extensions, even if they
              // are not web-accessible.
              // Also prevents leakage of extension UUIDs to other extensions..
              if (
                favIconUrl &&
                favIconUrl.startsWith("moz-extension:") &&
                !favIconUrl.startsWith(context.extension.baseURL)
              ) {
                favIconUrl = await ExtensionUtils.makeDataURI(favIconUrl);
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
          await Services.search.promiseInitialized;
          let engine;

          if (searchProperties.engine) {
            engine = Services.search.getEngineByName(searchProperties.engine);
            if (!engine) {
              throw new ExtensionError(
                `${searchProperties.engine} was not found`
              );
            }
          }

          let { tab, where } = getTarget({
            tabId: searchProperties.tabId,
            disposition: searchProperties.disposition,
            defaultDisposition: "NEW_TAB",
          });

          await windowTracker.topWindow.BrowserSearch.loadSearchFromExtension({
            query: searchProperties.query,
            where,
            engine,
            tab,
            triggeringPrincipal: context.principal,
          });
        },

        async query(queryProperties) {
          await Services.search.promiseInitialized;

          let { tab, where } = getTarget({
            tabId: queryProperties.tabId,
            disposition: queryProperties.disposition,
            defaultDisposition: "CURRENT_TAB",
          });

          await windowTracker.topWindow.BrowserSearch.loadSearchFromExtension({
            query: queryProperties.text,
            where,
            tab,
            triggeringPrincipal: context.principal,
          });
        },
      },
    };
  }
};
