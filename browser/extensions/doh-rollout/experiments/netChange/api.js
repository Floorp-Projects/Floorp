/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* exported netChange */
/* global Cc, Ci, ExtensionAPI, ExtensionCommon */
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var { EventManager } = ExtensionCommon;
let gNetworkLinkService = Cc[
  "@mozilla.org/network/network-link-service;1"
].getService(Ci.nsINetworkLinkService);

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

let netChangeWaiting = false;

var netChange = class netChange extends ExtensionAPI {
  getAPI(context) {
    return {
      experiments: {
        netChange: {
          onConnectionChanged: new EventManager({
            context,
            name: "netChange.onConnectionChanged",
            register: fire => {
              let observer = async (subject, topic, data) => {
                if (netChangeWaiting) {
                  return;
                }
                if (data === "changed" || data === "up") {
                  // Trigger the netChangeWaiting switch, initiating 5sec timeout
                  netChangeWaiting = true;
                  await sleep(60000);
                  if (
                    gNetworkLinkService.linkStatusKnown &&
                    gNetworkLinkService.isLinkUp
                  ) {
                    fire.async(data);
                  }
                  // Reset the netChangeWaiting switch
                  netChangeWaiting = false;
                }
              };

              Services.obs.addObserver(observer, "network:link-status-changed");
              return () => {
                Services.obs.removeObserver(
                  observer,
                  "network:link-status-changed"
                );
              };
            },
          }).api(),
        },
      },
    };
  }
};
