/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { MigrationUtils } from "resource:///modules/MigrationUtils.sys.mjs";
import { MigratorBase } from "resource:///modules/MigratorBase.sys.mjs";
import { MSMigrationUtils } from "resource:///modules/MSMigrationUtils.sys.mjs";

import { PlacesUtils } from "resource://gre/modules/PlacesUtils.sys.mjs";

// Resources

function History() {}

History.prototype = {
  type: MigrationUtils.resourceTypes.HISTORY,

  get exists() {
    return true;
  },

  migrate: function H_migrate(aCallback) {
    let pageInfos = [];
    let typedURLs = MSMigrationUtils.getTypedURLs(
      "Software\\Microsoft\\Internet Explorer"
    );
    let now = new Date();
    let maxDate = new Date(
      Date.now() - MigrationUtils.HISTORY_MAX_AGE_IN_MILLISECONDS
    );

    for (let entry of Cc[
      "@mozilla.org/profile/migrator/iehistoryenumerator;1"
    ].createInstance(Ci.nsISimpleEnumerator)) {
      let url = entry.get("uri").QueryInterface(Ci.nsIURI);
      // MSIE stores some types of URLs in its history that we don't handle,
      // like HTMLHelp and others.  Since we don't properly map handling for
      // all of them we just avoid importing them.
      if (!["http", "https", "ftp", "file"].includes(url.scheme)) {
        continue;
      }

      let title = entry.get("title");
      // Embed visits have no title and don't need to be imported.
      if (!title.length) {
        continue;
      }

      // The typed urls are already fixed-up, so we can use them for comparison.
      let transition = typedURLs.has(url.spec)
        ? PlacesUtils.history.TRANSITIONS.LINK
        : PlacesUtils.history.TRANSITIONS.TYPED;

      let time = entry.get("time");

      let visitDate = time ? PlacesUtils.toDate(time) : null;
      if (visitDate && visitDate < maxDate) {
        continue;
      }

      pageInfos.push({
        url,
        title,
        visits: [
          {
            transition,
            // use the current date if we have no visits for this entry.
            date: visitDate ?? now,
          },
        ],
      });
    }

    // Check whether there is any history to import.
    if (!pageInfos.length) {
      aCallback(true);
      return;
    }

    MigrationUtils.insertVisitsWrapper(pageInfos).then(
      () => aCallback(true),
      () => aCallback(false)
    );
  },
};

/**
 * Internet Explorer profile migrator
 */
export class IEProfileMigrator extends MigratorBase {
  static get key() {
    return "ie";
  }

  static get displayNameL10nID() {
    return "migration-wizard-migrator-display-name-ie";
  }

  static get brandImage() {
    return "chrome://browser/content/migration/brands/ie.png";
  }

  getResources() {
    let resources = [MSMigrationUtils.getBookmarksMigrator(), new History()];
    let windowsVaultFormPasswordsMigrator =
      MSMigrationUtils.getWindowsVaultFormPasswordsMigrator();
    windowsVaultFormPasswordsMigrator.name = "IEVaultFormPasswords";
    resources.push(windowsVaultFormPasswordsMigrator);
    return resources.filter(r => r.exists);
  }

  async getLastUsedDate() {
    const datePromises = ["Favs", "CookD"].map(dirId => {
      const { path } = Services.dirsvc.get(dirId, Ci.nsIFile);
      return IOUtils.stat(path)
        .then(info => info.lastModified)
        .catch(() => 0);
    });

    const dates = await Promise.all(datePromises);

    try {
      const typedURLs = MSMigrationUtils.getTypedURLs(
        "Software\\Microsoft\\Internet Explorer"
      );
      // typedURLs.values() returns an array of PRTimes, which are in
      // microseconds - convert to milliseconds
      dates.push(Math.max(0, ...typedURLs.values()) / 1000);
    } catch (ex) {}

    return new Date(Math.max(...dates));
  }
}
