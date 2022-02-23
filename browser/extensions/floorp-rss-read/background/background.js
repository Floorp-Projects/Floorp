"use strict";

/* import-globals-from ../shared/livemark-store.js */
/* import-globals-from ../shared/settings.js */
/* import-globals-from ../shared/feed-parser.js */

async function updateExtensionIcon(tabId) {
  const icon = await Settings.getExtensionIcon();
  const path = icon == "dark" ? "feed.png" : `feed.png`;
  await browser.pageAction.setIcon({ path: { 16: "icons/" + path }, tabId });
}

function hashString(str) {
  let hash = 0;
  for (let i = 0; i < str.length; i++) {
    hash  = ((hash << 5) - hash) + str.charCodeAt(i);
    hash |= 0; // Convert to 32bit integer
  }
  return hash;
}

const LivemarkUpdater = {
  async init() {
    await LivemarkStore.init();
    this.updateAllLivemarks = this.updateAllLivemarks.bind(this);
    this.handleSettingsChange = this.handleSettingsChange.bind(this);
    this.historyOnVisited = this.historyOnVisited.bind(this);

    const interval = await Settings.getPollInterval();
    this.intervalId = setInterval(this.updateAllLivemarks, 60 * 1000 * interval);

    LivemarkStore.addChangeListener(this.updateAllLivemarks);
    Settings.addChangeListener(this.handleSettingsChange);

    // We use this Map to mark feed items as visited.
    // Map [int32 -> Set[String]]
    this.itemURLHashToFeeds = new Map();
    browser.history.onVisited.addListener(this.historyOnVisited);

    // Force every feed to be refreshed on startup.
    const feedIds = (await LivemarkStore.getAll()).map(feed => feed.id);
    await this.updateAllLivemarks({changedKeys: feedIds});
  },
  async handleSettingsChange(changes) {
    if (changes["settings.pollInterval"]) {
      clearInterval(this.intervalId);
      const interval = await Settings.getPollInterval();
      this.intervalId = setInterval(this.updateAllLivemarks, 60 * 1000 * interval);
    }
  },
  async historyOnVisited(item) {
    const hash = hashString(item.url);
    const entry = this.itemURLHashToFeeds.get(hash);

    if (entry === undefined) {
      return;
    }

    for (const bookmarkId of entry) {
      if (await LivemarkStore.isLivemarkFolder(bookmarkId)) {
        const feed = await LivemarkStore.getDetails(bookmarkId);
        await this.updateLivemark(feed, {forceUpdate: true});
      }
      entry.delete(bookmarkId);
      if (entry.size === 0) {
        this.itemURLHashToFeeds.delete(hash);
      }
    }
  },
  async updateAllLivemarks({changedKeys = []} = {}) {
    const livemarks = await LivemarkStore.getAll();

    const next = () => {
      const feed = livemarks.pop();
      if (feed) {
        this.updateLivemark(feed, {
          forceUpdate: changedKeys.includes(feed.id),
        }).finally(next);
      }
    };

    // Only update at most 5 livemarks concurrently.
    livemarks.splice(0, 5).forEach(feed => {
      this.updateLivemark(feed, {
        forceUpdate: changedKeys.includes(feed.id),
      }).finally(next);
    });
  },
  // adds the site url bookmark if it doesn't
  // exist already. Returns whether or not it modified the child list.
  async addFeedSiteUrlBookmark(folder, feed, children) {
    const siteUrlTitle = browser.i18n.getMessage("openSiteUrl", folder.title);
    let didChange = false;
    if (children.length === 0) {
      await browser.bookmarks.create({
        title: siteUrlTitle,
        url: feed.siteUrl,
        parentId: folder.id,
      });
      didChange = true;
    } else if (children[0].title !== siteUrlTitle ||
               children[0].url !== feed.siteUrl) {
      await browser.bookmarks.update(children[0].id, {
        title: siteUrlTitle,
        url: feed.siteUrl,
      });
      didChange = true;
    }

    if (children.length <= 1 || children[1].type !== "separator") {
      // We don't have a separator or it's in the wrong place, just make a new
      // one. We delete separators found later in the list anyway.
      await browser.bookmarks.create({
        parentId: folder.id,
        title: "",
        type: "separator",
        index: 1,
      });
      didChange = true;
    }
    return didChange;
  },
  async updateLivemark(feed, options) {
    const {
      forceUpdate = false,
    } = options || {};

    const feedData = await FeedParser.getFeed(feed.feedUrl);
    if (feedData.updated && feedData.updated == feed.updated && !forceUpdate) {
      return;
    }
    const [folder] = await browser.bookmarks.get(feed.id);
    const readPrefix = await Settings.getReadPrefix();
    const unreadPrefix = await Settings.getUnreadPrefix();

    // Note: We make an effort to avoid unnecessary churn on bookmark
    // creation/deletion, as it give firefox sync a hard time:
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1499881

    let children = await browser.bookmarks.getChildren(folder.id);
    let startIndex = 0;

    if (feed.siteUrl) {
      const didChange = await this.addFeedSiteUrlBookmark(folder, feed, children);
      // Changes should be rare, so just refetch the child list if it changed
      // rather than attempting to keep it up to date.
      if (didChange) {
        children = await browser.bookmarks.getChildren(folder.id);
      }
      startIndex = 2;
    }

    // List of children we can overwrite with update.
    const usableChildren = [];

    for (let i = startIndex; i < children.length; ++i) {
      // Remove unexpected separators/folders, since update can't change type.
      const bookmark = children[i];
      if (bookmark.type !== "bookmark") {
        await browser.bookmarks.remove(bookmark.id);
      } else {
        usableChildren.push(bookmark);
      }
    }

    // Remove the old hashes from the map.
    for (const bookmark of usableChildren) {
      const hash = hashString(bookmark.url);
      const entry = this.itemURLHashToFeeds.get(hash);
      if (entry) {
        entry.delete(folder.id);
        if (entry.size === 0) {
          this.itemURLHashToFeeds.delete(hash);
        }
      }
    }

    // Filter items with no title and URL.
    const items = feedData.items.filter(item => item.title || item.url);
    const max = Math.min(feed.maxItems, items.length);
    let i = 0;
    let itemsReadCount = 0;
    for (; i < max; i++) {
      const item = items[i];
      const url = item.url || feed.feedUrl;
      let visits = 0;
      try {
        visits = (await browser.history.getVisits({url})).length;
      } catch (e) {}

      if (visits > 0) {
        itemsReadCount += (visits > 0);
      }

      // Only unvisited URLs need to be updated later
      if (visits === 0 && (readPrefix || unreadPrefix)) {
        const hash = hashString(url);
        const entry = this.itemURLHashToFeeds.get(hash);
        if (entry === undefined) {
          this.itemURLHashToFeeds.set(hash, new Set([folder.id]));
        } else if (!entry.has(folder.id)) {
          // Handle collusion.
          entry.add(folder.id);
        }
      }

      const itemTitle = item.title ? item.title : url;
      const title = PrefixUtils.addPrefix(
        (visits > 0) ? readPrefix : unreadPrefix,
        itemTitle,
      );
      if (i < usableChildren.length) {
        // There's a child in the right place, see if it has the right data,
        // and update it if not.
        const child = usableChildren[i];
        if (child.url !== url || child.title !== title) {
          await browser.bookmarks.update(child.id, {
            title,
            url
          });
        }
      } else {
        // Out of children, make a new one.
        await browser.bookmarks.create({
          parentId: folder.id,
          title,
          url
        });
      }
    }
    // Delete any children we didn't end up needing.
    for (; i < usableChildren.length; ++i) {
      await browser.bookmarks.remove(usableChildren[i].id);
    }
    await LivemarkStore.edit(folder.id, {updated: feedData.updated});

    // Update the feed folder title prefix if all items have been read
    if (await Settings.getPrefixFeedFolderEnabled()) {
      await this.setPrefix(folder, itemsReadCount == max);
    }

    // Update the parent folders' title prefix title if all RSS feeds have been read
    if (await Settings.getPrefixParentFoldersEnabled()) {
      this.setParentFoldersPrefix(folder);
    }
  },
  async setParentFoldersPrefix(livemark) {
    const [folder] = await browser.bookmarks.get(livemark.parentId);

    // Don't change the folder name if it is any of the builtin folders
    const builtinIds = ["toolbar_____", "menu________", "unfiled_____", "mobile______"];
    if (builtinIds.includes(folder.id)) {
      return;
    }

    const children = await browser.bookmarks.getChildren(folder.id);
    if (!children || children.length == 0) {
      return;
    }

    const readPrefix = await Settings.getReadPrefix();

    const allRead = children.every(
      child => PrefixUtils.hasPrefix(readPrefix, child.title)
    );

    await this.setPrefix(folder, allRead);
    this.setParentFoldersPrefix(folder);
  },
  async setPrefix(item, isRead) {
    const readPrefix = await Settings.getReadPrefix();
    const unreadPrefix = await Settings.getUnreadPrefix();

    const oldTitle = item.title;

    let title = PrefixUtils.removePrefix(readPrefix, oldTitle);
    title = PrefixUtils.removePrefix(unreadPrefix, title);

    // Update the title with the new prefix
    title = PrefixUtils.addPrefix(isRead ? readPrefix : unreadPrefix, title);

    if (title != oldTitle) {
      await browser.bookmarks.update(item.id, {title});
    }
  }
};

/* Context menu */

const ContextMenu = {
  async init() {
    const getUnreadChildren = async (bookmarkId) => {
      const feed = await LivemarkStore.getDetails(bookmarkId);
      let children = await browser.bookmarks.getChildren(bookmarkId);
      // Skip "Open <Site URL>" bookmark and separator.
      children = children.slice(feed.siteUrl ? 2 : 0);

      const getVisits = ({url}) => {
        return browser.history.getVisits({url});
      };

      const visits = await Promise.all(children.map(getVisits));
      return children.filter((_, i) => visits[i] < 1);
    };

    const createItems = async () => {
      this.reloadItemId = await browser.menus.create({
        contexts: ["bookmark"],
        title: browser.i18n.getMessage("reloadLiveBookmark"),
        async onclick({bookmarkId}) {
          const feed = await LivemarkStore.getDetails(bookmarkId);
          await LivemarkUpdater.updateLivemark(feed, {forceUpdate: true});
        }
      });

      this.openUnreadItemId = await browser.menus.create({
        contexts: ["bookmark"],
        title: browser.i18n.getMessage("openAllUnreadPages"),
        async onclick({bookmarkId}) {
          const unreadBookmarks = await getUnreadChildren(bookmarkId);
          for (let bookmark of unreadBookmarks) {
            browser.tabs.create({url: bookmark.url});
          }
        }
      });
    };

    browser.menus.onShown.addListener(async ({bookmarkId}) => {
      const isLivemarkFolder = await LivemarkStore.isLivemarkFolder(bookmarkId);
      if (!isLivemarkFolder) {
        await browser.menus.removeAll();
        this.reloadItemId = null;
      } else {
        if (!this.reloadItemId) {
          await createItems();
        }

        // Disable "Open all unread pages in tabs" if all feeds are already
        // read/visited.
        browser.menus.update(this.openUnreadItemId, {
          enabled: (await getUnreadChildren(bookmarkId)).length >= 1
        });
      }
      await browser.menus.refresh();
    });

    createItems();
  }
};

function getSubscribeURL(feedUrl) {
  const url = chrome.extension.getURL("pages/subscribe/subscribe.html");
  return url + "?" + encodeURIComponent(feedUrl);
}

const FeedPreview = {
  async init() {
    // This value needs to be synchronously accessible in the
    // chrome.webRequest.onHeadersReceived listener below.
    this.enabled = await Settings.getFeedPreviewEnabled();

    Settings.addChangeListener(async (changes) => {
      if (changes["settings.feedPreviewEnabled"]) {
        this.enabled = await Settings.getFeedPreviewEnabled();
      }
    });
  },

  async show(tabId, url) {
    try {
      // Verify that this is actually a feed by parsing it.
      const feed = await FeedParser.getFeed(url);
      if (feed.items.length > 0) {
        chrome.tabs.update(tabId, {url: getSubscribeURL(url), loadReplace: true});
      }
    } catch (e) {}
  }
};

(async function() {
  await LivemarkUpdater.init();
  await ContextMenu.init();
  await FeedPreview.init();
})();

// A dictionary keyed off of tabId that keeps track of data per tab (for
// example what feedUrl was detected in the tab).
const tabIdToFeeds = {};

browser.runtime.onMessage.addListener(async (request, sender) => {
  if (request.msg == "feeds") {
    // We have received a list of feed urls found on the page.
    // Enable the page action icon.
    tabIdToFeeds[sender.tab.id] = request.feeds;
    chrome.pageAction.show(sender.tab.id);
    updateExtensionIcon(sender.tab.id);
  }

  if (request.msg == "get-tab-feeds") {
    return tabIdToFeeds[request.tabId];
  }

  if (request.msg == "load-preview") {
    if (FeedPreview.enabled) {
      FeedPreview.show(sender.tab.id, request.url);
    }
  }

  if (request.msg == "get-feed") {
    return FeedParser.getFeed(request.feedUrl);
  }

  if (request.msg == "subscribe") {
    const {title, feedUrl, siteUrl} = request;
    const folderId = await Settings.getDefaultFolder();

    await LivemarkStore.add({
      title,
      feedUrl,
      siteUrl,
      parentId: folderId,
      maxItems: 25,
    });

    const [folderProps] = await browser.bookmarks.get(folderId);
    let formattedFolderTitle = folderProps.title;
    const readPrefix = await Settings.getReadPrefix();
    formattedFolderTitle = PrefixUtils.removePrefix(readPrefix, formattedFolderTitle);
    const unreadPrefix = await Settings.getUnreadPrefix();
    formattedFolderTitle = PrefixUtils.removePrefix(unreadPrefix, formattedFolderTitle);

    return formattedFolderTitle;
  }
});

chrome.tabs.onRemoved.addListener(function(tabId) {
  delete tabIdToFeeds[tabId];
});

chrome.webRequest.onHeadersReceived.addListener(details => {
  if (!FeedPreview.enabled) {
    // Feed preview disabled, nothing to do here.
    return;
  }

  const header = details.responseHeaders.find(h => {
    return h.name.toLowerCase() == "content-type";
  });
  if (header === undefined) {
    return;
  }

  // Atom or RSS MIME type, redirect to preview page
  const type = header.value.toLowerCase().replace(/^\s+|\s*(?:;.*)?$/g, "");
  if (type == "application/rss+xml" || type == "application/atom+xml") {
    return {
      redirectUrl: getSubscribeURL(details.url),
    };
  }

  // XML MIME type, try sniffing for feed content
  if (type == "application/xml" || type == "text/xml") {
    const decoder = new TextDecoder("utf-8");
    let str = "";

    const filter = browser.webRequest.filterResponseData(details.requestId);
    filter.ondata = async (event) => {
      filter.write(event.data);

      str += decoder.decode(event.data, {stream: true});
      if (str.includes("<channel") || str.includes("<feed")) {
        filter.disconnect();

        FeedPreview.show(details.tabId, details.url);
        return;
      }

      // Stop sniffing after some time.
      if (str.length > 1024) {
        filter.disconnect();
      }
    };
    filter.onstop = (event) => {
      filter.disconnect();
    };
  }
}, {urls: ["<all_urls>"], types: ["main_frame"]}, ["blocking", "responseHeaders"]);
