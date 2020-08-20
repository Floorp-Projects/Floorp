/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserUsageTelemetry: "resource:///modules/BrowserUsageTelemetry.jsm",
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "searchLoadInBackground",
  "browser.search.context.loadInBackground"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "btoa"]);

var { ExtensionError } = ExtensionUtils;

async function getDataURI(localIconUrl) {
  let response;
  try {
    response = await fetch(localIconUrl);
  } catch (e) {
    // Failed to fetch, ignore engine's favicon.
    Cu.reportError(e);
    return;
  }
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
                  favIconUrl = await getDataURI(engine.iconURI.spec);
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
          } else {
            engine = await Services.search.getDefault();
          }
          let submission = engine.getSubmission(
            searchProperties.query,
            null,
            "webextension"
          );
          let options = {
            postData: submission.postData,
            triggeringPrincipal: context.principal,
          };
          let tabbrowser;
          if (searchProperties.tabId === null) {
            let { gBrowser } = windowTracker.topWindow;
            let nativeTab = gBrowser.addTab(submission.uri.spec, options);
            if (!searchLoadInBackground) {
              gBrowser.selectedTab = nativeTab;
            }
            tabbrowser = gBrowser;
          } else {
            let tab = tabTracker.getTab(searchProperties.tabId);
            tab.linkedBrowser.loadURI(submission.uri.spec, options);
            tabbrowser = tab.linkedBrowser.getTabBrowser();
          }
          BrowserUsageTelemetry.recordSearch(
            tabbrowser,
            engine,
            "webextension"
          );
        },
      },
    };
  }
};
