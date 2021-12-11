"use strict";

/* import-globals-from prefix-utils.js */
/* import-globals-from settings.js */

const PREFIX = "livemarks.";

function toInternalId(id) {
  return PREFIX + id;
}

function fromInternalId(id) {
  return id.slice(PREFIX.length);
}

/* exported LivemarkStore */
const LivemarkStore = {
  async isLivemarkFolder(id) {
    const livemark = await this.get(id);
    return livemark !== undefined;
  },

  async get(id) {
    const result = await browser.storage.sync.get(toInternalId(id));
    return result[toInternalId(id)];
  },

  async getAll(broken = []) {
    const livemarks = await browser.storage.sync.get();

    // Pass in prefixes so they can be removed from the title. We don't get them in
    // _makeDetails to avoid unnecessarily fetching into storage
    const readPrefix = await Settings.getReadPrefix();
    const unreadPrefix = await Settings.getUnreadPrefix();

    const all = [];
    for (const key in livemarks) {
      if (!key.startsWith(PREFIX)) {
        continue;
      }

      const id = fromInternalId(key);
      try {
        const details = await this._makeDetails(id, livemarks[key], {
          readPrefix,
          unreadPrefix,
        });
        all.push(details);
      } catch (e) {
        broken.push({id, ...livemarks[key]});
        console.error("Found broken bookmark", id, e);
      }
    }

    return all;
  },

  async add(feed) {
    const { title, parentId } = feed;
    const bookmark = await browser.bookmarks.create({
      title,
      type: "folder",
      parentId,
    });

    this.addWithBookmark(bookmark.id, feed);
  },

  async addWithBookmark(id, feed) {
    const feedDetails = {
      feedUrl: feed.feedUrl,
      maxItems: feed.maxItems,
    };
    if (feed.siteUrl) {
      feedDetails.siteUrl = new URL(feed.siteUrl, feed.feedUrl).href;
    } else {
      feedDetails.siteUrl = "";
    }

    await browser.storage.sync.set({
      [toInternalId(id)]: feedDetails
    });
  },

  async remove(bookmarkId) {
    try {
      await browser.bookmarks.removeTree(bookmarkId);
    } catch (e) {
      // Bookmark already deleted
    }

    await browser.storage.sync.remove(toInternalId(bookmarkId));
  },

  async edit(id, feed) {
    const oldFeed = await this.get(id);
    const [oldBookmark] = await browser.bookmarks.get(id);
    if (!oldBookmark || !oldFeed) {
      return;
    }
    // Handle renames
    if (feed.title && feed.title !== oldBookmark.title) {
      await browser.bookmarks.update(id, {
        "title": feed.title,
      });
    }

    // Folder change
    if (feed.parentId && feed.parentId !== oldBookmark.parentId) {
      await browser.bookmarks.move(id, {
        "parentId": feed.parentId,
      });
    }

    if (feed.siteUrl && feed.siteUrl !== oldFeed.siteUrl) {
      oldFeed.siteUrl = feed.siteUrl;
    } else if (feed.siteUrl === "" && oldFeed.siteUrl) {
      // We have to check against "" since we only want to cover the case where
      // the user explicitly set the value to be empty.
      oldFeed.siteUrl = null;
    }

    if (feed.feedUrl && feed.feedUrl !== oldFeed.feedUrl) {
      oldFeed.feedUrl = feed.feedUrl;
    }

    if (feed.maxItems && feed.maxItems !== oldFeed.maxItems) {
      oldFeed.maxItems = feed.maxItems;
    }

    if (feed.updated && feed.updated !== oldFeed.updated) {
      oldFeed.updated = feed.updated;
    }

    await browser.storage.sync.set({ [toInternalId(id)]: oldFeed });
  },

  async _makeDetails(id, {feedUrl, siteUrl, maxItems, updated}, {readPrefix, unreadPrefix}) {
    let [{title, parentId}] = await browser.bookmarks.get(id);

    title = PrefixUtils.removePrefix(readPrefix, title);
    title = PrefixUtils.removePrefix(unreadPrefix, title);

    return {
      title,
      feedUrl,
      siteUrl,
      maxItems,
      parentId,
      updated,
      id,
    };
  },

  async getDetails(id) {
    const feed = await this.get(id);
    const readPrefix = await Settings.getReadPrefix();
    const unreadPrefix = await Settings.getUnreadPrefix();
    return this._makeDetails(id, feed, {readPrefix, unreadPrefix});
  },

  addChangeListener(listener) {
    this.listeners.push(listener);
  },

  async init() {
    this.listeners = [];

    try {
      const { livemarks } = await browser.storage.local.get("livemarks");
      if (livemarks) {
        for (const [id, feed] of livemarks) {
          const [bookmark] = await browser.bookmarks.get(id).catch(() => {
            return [];
          });

          if (bookmark === undefined) {
            continue;
          }

          await browser.storage.sync.set({ [toInternalId(bookmark.id)]: feed });
        }

        await browser.storage.local.remove("livemarks");
      }
    } catch (e) {
      console.error("Storage migration failed", e);
    }

    browser.bookmarks.onRemoved.addListener(async id => {
      const isLivemarkFolder = await this.isLivemarkFolder(id);
      if (isLivemarkFolder) {
        await this.remove(id);
      }
    });

    browser.storage.onChanged.addListener((changes, areaName) => {
      if (areaName !== "sync") {
        return;
      }

      const changedKeys = [];
      for (const key in changes) {
        if (!key.startsWith(PREFIX)) {
          continue;
        }

        // Creation
        if (!changes[key].oldValue) {
          changedKeys.push(fromInternalId(key));
          continue;
        }

        // Deletion
        if (!changes[key].newValue) {
          changedKeys.push(fromInternalId(key));
          continue;
        }

        // Important: the `updated` field must not be considered!
        const {feedUrl, maxItems, siteUrl} = changes[key].newValue;
        const old = changes[key].oldValue;
        if (old.feedUrl !== feedUrl || old.maxItems !== maxItems ||
            old.siteUrl !== siteUrl) {
          changedKeys.push(fromInternalId(key));
        }
      }

      if (changedKeys.length > 0) {
        this.listeners.forEach(listener => listener({ changedKeys }));
      }
    });
  }
};
