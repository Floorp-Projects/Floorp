/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(this, "Services",
                               "resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyPreferenceGetter(this, "searchLoadInBackground",
                                      "browser.search.context.loadInBackground");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "btoa"]);

var {
  ExtensionError,
} = ExtensionUtils;

async function getDataURI(resourceURI) {
  let response = await fetch(resourceURI);
  let buffer = await response.arrayBuffer();
  let contentType = response.headers.get("content-type");
  let bytes = new Uint8Array(buffer);
  let str = String.fromCharCode.apply(null, bytes);
  return `data:${contentType};base64,${btoa(str)}`;
}

this.search = class extends ExtensionAPI {
  getAPI(context) {
    return {
      search: {
        async get() {
          await searchInitialized;
          let engines = Services.search.getEngines();
          let visibleEngines = engines.filter(engine => !engine.hidden);
          return Promise.all(visibleEngines.map(async engine => {
            let favicon_url = null;
            if (engine.iconURI) {
              if (engine.iconURI.schemeIs("resource") ||
                  engine.iconURI.schemeIs("chrome")) {
                // Convert internal URLs to data URLs
                favicon_url = await getDataURI(engine.iconURI.spec);
              } else {
                favicon_url = engine.iconURI.spec;
              }
            }

            return {
              name: engine.name,
              is_default: engine === Services.search.currentEngine,
              alias: engine.alias,
              favicon_url,
            };
          }));
        },

        async search(name, searchTerms, tabId) {
          await searchInitialized;
          let engine = Services.search.getEngineByName(name);
          if (!engine) {
            throw new ExtensionError(`${name} was not found`);
          }
          let submission = engine.getSubmission(searchTerms, null, "webextension");
          let options = {
            postData: submission.postData,
            triggeringPrincipal: context.principal,
          };
          if (tabId === null) {
            let browser = context.pendingEventBrowser || context.xulBrowser;
            let {gBrowser} = browser.ownerGlobal;
            if (!gBrowser || !gBrowser.addTab) {
              // In some cases (about:addons, sidebar, maybe others), we need
              // to go up one more level.
              browser = browser.ownerDocument.docShell.chromeEventHandler;

              ({gBrowser} = browser.ownerGlobal);
            }
            if (!gBrowser || !gBrowser.addTab) {
              throw new ExtensionError("Unable to locate a browser.");
            }
            let nativeTab = gBrowser.addTab(submission.uri.spec, options);
            if (!searchLoadInBackground) {
              gBrowser.selectedTab = nativeTab;
            }
          } else {
            let tab = tabTracker.getTab(tabId);
            tab.linkedBrowser.loadURI(submission.uri.spec, options);
          }
        },
      },
    };
  }
};
