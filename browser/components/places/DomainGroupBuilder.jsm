/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["DomainGroupBuilder"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  CommonNames: "resource:///modules/CommonNames.jsm",
  SnapshotGroups: "resource:///modules/SnapshotGroups.jsm",
});

/**
 * A builder for snapshot groups based on domains.
 */
const DomainGroupBuilder = new (class DomainGroupBuilder {
  /**
   * @type {Map<string, object>}
   * A map of domains to snapshot group data for groups that are currently in
   * the database.
   */
  #currentGroups = null;

  /**
   * Rebuilds the domain groups from the complete list of snapshots.
   *
   * @param {Snapshot[]} snapshots
   *   The current array of snapshots in the database.
   */
  async rebuild(snapshots) {
    // Clear the current groups and rebuild.
    this.#currentGroups = null;
    await this.#maybeLoadGroups();

    let untouchedDomains = new Set(this.#currentGroups.keys());
    let changedDomains = new Set();

    for (let snapshot of snapshots) {
      let domain = this.#getDomain(snapshot.url);
      if (!domain) {
        continue;
      }

      untouchedDomains.delete(domain);

      let group = this.#currentGroups.get(domain);
      if (!group) {
        this.#currentGroups.set(
          domain,
          this.#generateDomainData(domain, snapshot.url)
        );
        changedDomains.add(domain);
      } else if (!group.urls.has(snapshot.url)) {
        group.urls.add(snapshot.url);
        changedDomains.add(domain);
      }
    }

    for (let domain of [
      ...changedDomains.values(),
      // Untouched domains are ones which have no-urls, and therefore should
      // be deleted.
      ...untouchedDomains.values(),
    ]) {
      await this.#checkDomain(domain);
    }
  }

  /**
   * Updates the domain snapshot groups based on the added and removed urls.
   *
   * @param {object} options
   * @param {Set<string>} options.addedUrls
   *   The added urls to handle in this update.
   * @param {Set<string>} options.removedUrls
   *   The removed urls to handle in this update.
   */
  async update({ addedUrls, removedUrls }) {
    await this.#maybeLoadGroups();
    // Update the groups with the changes, and keep a note of the changed
    // domains.
    let changedDomains = new Set();
    for (let url of addedUrls.values()) {
      let domain = this.#getDomain(url);
      if (!domain) {
        continue;
      }

      let group = this.#currentGroups.get(domain);
      if (!group) {
        this.#currentGroups.set(domain, this.#generateDomainData(domain, url));
        changedDomains.add(domain);
      } else if (!group.urls.has(url)) {
        group.urls.add(url);
        changedDomains.add(domain);
      }
    }

    for (let url of removedUrls.values()) {
      let domain = this.#getDomain(url);
      if (!domain) {
        continue;
      }

      let group = this.#currentGroups.get(domain);
      if (group?.urls.has(url)) {
        group.urls.delete(url);
        changedDomains.add(domain);
      }
    }

    // Check the saved groups are up to date.
    for (let domain of changedDomains.values()) {
      await this.#checkDomain(domain);
    }
  }

  /**
   * Handles checking a domain to see if it now should be added, updated
   * or deleted in the snapshot groups in the database.
   *
   * @param {string} domain
   *   The domain that is being checked.
   * @param {Set<string>} urls
   *   The updated set of urls for the domain.
   */
  async #checkDomain(domain, urls) {
    let group = this.#currentGroups.get(domain);
    if (group.urls.size > 0) {
      // If the group has an id, then it is already in the database.
      if (group.id) {
        await SnapshotGroups.updateUrls(group.id, [...group.urls.values()]);
      } else {
        // This is a new group.
        let id = await SnapshotGroups.add(group, [...group.urls.values()]);
        group.id = id;
      }
    } else {
      // This group no longer has entries, and needs to be removed.
      await SnapshotGroups.delete(group.id);
      this.#currentGroups.delete(domain);
    }
  }

  /**
   * Loads the current domain snapshot groups and all the snapshot urls from
   * the database, and inserts them into #currentGroups.
   */
  async #maybeLoadGroups() {
    if (this.#currentGroups) {
      return;
    }
    this.#currentGroups = new Map();

    let groups = await SnapshotGroups.query({
      builder: "domain",
      limit: -1,
      skipMinimum: true,
    });

    for (let group of groups) {
      this.#currentGroups.set(group.builderMetadata.domain, group);
      group.urls = new Set(await SnapshotGroups.getUrls(group.id));
    }
  }

  /**
   * Generates the group information for a domain.
   *
   * @param {string} domain
   *   The domain of the group.
   * @param {string} url
   *   The initial url being added to the group.
   * @returns {SnapshotGroup}
   */
  #generateDomainData(domain, url) {
    return {
      title: CommonNames.getURLName(new URL(url)),
      builder: "domain",
      builderMetadata: {
        domain,
      },
      urls: new Set([url]),
    };
  }

  /**
   * Gets the domain of a URL.
   *
   * @param {string} url
   * @returns {string}
   */
  #getDomain(url) {
    try {
      return new URL(url).hostname;
    } catch (ex) {
      return null;
    }
  }
})();
