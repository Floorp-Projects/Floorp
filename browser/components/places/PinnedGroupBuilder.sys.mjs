/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Snapshots: "resource:///modules/Snapshots.sys.mjs",
  SnapshotGroups: "resource:///modules/SnapshotGroups.sys.mjs",
});

/**
 * A builder for snapshot groups based on pinned snapshots.
 */
export const PinnedGroupBuilder = new (class PinnedGroupBuilder {
  /**
   * @type {Map<string, object>}
   * A map of domains to snapshot group data for groups that are currently in
   * the database.
   */
  #group = null;

  /**
   * @type {string}
   * The name of the builder recorded in the groups.
   */
  name = "pinned";

  /**
   * @type {boolean}
   * This will cause the minimum snapshot size check to be skipped.
   */
  skipMinimumSize = true;

  /**
   * Rebuilds the group from the complete list of snapshots.
   *
   * @param {Snapshot[]} snapshots
   *   The current array of snapshots in the database.
   */
  async rebuild(snapshots) {
    await this.#maybeLoadGroup();

    let urls = snapshots
      .filter(s => s.userPersisted == lazy.Snapshots.USER_PERSISTED.PINNED)
      .map(u => u.url);
    if (
      urls.length == this.#group.urls.size &&
      urls.every(u => this.#group.urls.has(u))
    ) {
      return;
    }

    this.#group.urls = new Set(urls);
    await this.#updateGroup(this.#group.urls);
  }

  /**
   * Updates the group based on the added and removed urls.
   *
   * @param {object} options
   * @param {Set<object>} options.addedItems
   *   The added items to handle in this update. A set of objects with
   *   properties of url and userPersisted.
   * @param {Set<string>} options.removedUrls
   *   The removed urls to handle in this update.
   */
  async update({ addedItems, removedUrls }) {
    await this.#maybeLoadGroup();

    let changed = false;
    for (let { url, userPersisted } of addedItems.values()) {
      if (userPersisted != lazy.Snapshots.USER_PERSISTED.PINNED) {
        continue;
      }

      if (!this.#group.urls.has(url)) {
        changed = true;
        this.#group.urls.add(url);
      }
    }

    for (let url of removedUrls.values()) {
      if (this.#group.urls.has(url)) {
        changed = true;
        this.#group.urls.delete(url);
      }
    }

    if (!changed) {
      return;
    }

    await this.#updateGroup(this.#group.urls);
  }

  /**
   * Updates the group in the database. This should only be called when there
   * have been changes made to the list of URLs.
   */
  async #updateGroup() {
    // Don't create the group if it hasn't got any items.
    if (this.#group.id == undefined && !this.#group.urls.size) {
      return;
    }

    if (this.#group.id == undefined) {
      let id = await lazy.SnapshotGroups.add(this.#group, this.#group.urls);
      this.#group.id = id;
    } else {
      // For this group, we allow it to continue to exist if the pins have
      // reduced to zero, SnapshotGroups will filter it out.
      await lazy.SnapshotGroups.updateUrls(this.#group.id, this.#group.urls);
    }
  }

  /**
   * Loads the current domain snapshot groups and all the snapshot urls from
   * the database, and inserts them into #groups.
   */
  async #maybeLoadGroup() {
    if (this.#group) {
      return;
    }

    let groups = await lazy.SnapshotGroups.query({
      builder: this.name,
      limit: -1,
      skipMinimum: true,
    });

    if (!groups.length) {
      this.#group = this.#generateEmptyGroup();
      return;
    }

    if (groups.length > 1) {
      console.error("Should only be one pinned group in the database");
    }

    this.#group = groups[0];
    this.#group.urls = new Set(
      await lazy.SnapshotGroups.getUrls({ id: this.#group.id, hidden: true })
    );
  }

  /**
   * Generates an empty pinned group.
   *
   * @returns {SnapshotGroup}
   */
  #generateEmptyGroup() {
    return {
      builder: "pinned",
      builderMetadata: {
        fluentTitle: { id: "snapshot-group-pinned-header" },
      },
      urls: new Set(),
    };
  }

  /**
   * Used for tests to delete the group and reset this object.
   */
  async reset() {
    await lazy.SnapshotGroups.delete(this.#group.id);
    this.#group = null;
  }
})();
