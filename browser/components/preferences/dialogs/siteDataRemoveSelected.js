/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { SiteDataManager } = ChromeUtils.import(
  "resource:///modules/SiteDataManager.jsm"
);

/**
 * This dialog will ask the user to confirm that they really want to delete
 * all site data for a number of hosts. Displaying the hosts can be done in
 * two different ways by passing options in the arguments object.
 *
 * - Passing a "baseDomain" will cause the dialog to search for all applicable
 *   host names with that baseDomain using the SiteDataManager and populate the list
 *   asynchronously. As a side effect, the SiteDataManager will update. Thus it is
 *   safe to call SiteDataManager methods that require a manual updateSites call
 *   after spawning this dialog. However, you need to ensure that the SiteDataManager
 *   has finished updating.
 *
 * - Passing a "hosts" array allows you to manually specify the hosts to be displayed.
 *   The SiteDataManager will not be updated by spawning this dialog in that case.
 *
 *   The two options are mutually exclusive. You must specify one.
 **/
let gSiteDataRemoveSelected = {
  init() {
    document.addEventListener("dialogaccept", function() {
      window.arguments[0].allowed = true;
    });
    document.addEventListener("dialogcancel", function() {
      window.arguments[0].allowed = false;
    });

    let list = document.getElementById("removalList");

    let baseDomain = window.arguments[0].baseDomain;
    if (baseDomain) {
      let hosts = new Set();
      SiteDataManager.updateSites((host, site) => {
        // Disregard local files.
        if (!host) {
          return;
        }

        if (site.baseDomain != baseDomain) {
          return;
        }

        // Avoid listing duplicate hosts.
        if (hosts.has(host)) {
          return;
        }
        hosts.add(host);

        let listItem = document.createXULElement("richlistitem");
        let label = document.createXULElement("label");
        label.setAttribute("value", host);
        listItem.appendChild(label);
        list.appendChild(listItem);
      });
      return;
    }

    let hosts = window.arguments[0].hosts;
    if (hosts) {
      hosts.sort();
      let fragment = document.createDocumentFragment();
      for (let host of hosts) {
        let listItem = document.createXULElement("richlistitem");
        let label = document.createXULElement("label");
        if (host) {
          label.setAttribute("value", host);
        } else {
          document.l10n.setAttributes(label, "site-data-local-file-host");
        }
        listItem.appendChild(label);
        fragment.appendChild(listItem);
      }
      list.appendChild(fragment);
      return;
    }

    throw new Error(
      "Must specify either a hosts or baseDomain option in arguments."
    );
  },
};
