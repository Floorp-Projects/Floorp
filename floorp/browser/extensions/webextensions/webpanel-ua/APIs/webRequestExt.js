/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, ExtensionCommon, Services, XPCOMUtils */

const { WebRequest } = ChromeUtils.import(
  "resource://gre/modules/WebRequest.jsm"
);

const { Services } = ChromeUtils.import(
  "resource://gre/modules/Services.jsm"
);

this.webRequestExt = class extends ExtensionAPI {
  getAPI(context) {
    const EventManager = ExtensionCommon.EventManager;

    return {
      webRequestExt: {
        onBeforeRequest_webpanel_requestId: new EventManager({
          context,
          register: (fire) => {
            function listener(e) {
              if (typeof e.browserElement !== "undefined" &&
                  e.browserElement.id.startsWith("webpanel")) {
                if (e.browserElement.getAttribute("changeuseragent") == "true") {
                  return fire.async(e.requestId);
                }
              }
            }
            WebRequest.onBeforeRequest.addListener(
              listener,
              null,
              ["blocking"]
            );
            return () => {
              WebRequest.onBeforeRequest.removeListener(
                listener,
                null,
                ["blocking"]
              );
            };
          },
        }).api(),
      },
    };
  }
};
