/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * A Bookmark object received through the policy engine will be an
 * object with the following properties:
 *
 * - URL (URL)
 *   (required) The URL for this bookmark
 *
 * - Title (string)
 *   (required) The title for this bookmark
 *
 * - Placement (string)
 *   (optional) Either "toolbar" or "menu". If missing or invalid,
 *   "toolbar" will be used
 *
 * - Folder (string)
 *   (optional) The name of the folder to put this bookmark into.
 *   If present, a folder with this name will be created in the
 *   chosen placement above, and the bookmark will be created there.
 *   If missing, the bookmark will be created directly into the
 *   chosen placement.
 *
 * - Favicon (URL)
 *   (optional) An http:, https: or data: URL with the favicon.
 *   If possible, we recommend against using this property, in order
 *   to keep the json file small.
 *   If a favicon is not provided through the policy, it will be loaded
 *   naturally after the user first visits the bookmark.
 *
 *
 * Note: The Policy Engine automatically converts the strings given to
 * the URL and favicon properties into a URL object.
 *
 * The schema for this object is defined in policies-schema.json.
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

const PREF_LOGLEVEL = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "BookmarksPolicies.jsm",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

this.EXPORTED_SYMBOLS = ["BookmarksPolicies"];

this.BookmarksPolicies = {
  // These prefixes must only contain characters
  // allowed by PlacesUtils.isValidGuid
  BOOKMARK_GUID_PREFIX: "PolB-",
  FOLDER_GUID_PREFIX: "PolF-",

  /*
   * Process the bookmarks specified by the policy engine.
   *
   * @param param
   *        This will be an array of bookmarks objects, as
   *        described on the top of this file.
   */
  processBookmarks(param) {
    calculateLists(param).then(async function addRemoveBookmarks(results) {
      for (let bookmark of results.add.values()) {
        await insertBookmark(bookmark).catch(log.error);
      }
      for (let bookmark of results.remove.values()) {
        await PlacesUtils.bookmarks.remove(bookmark).catch(log.error);
      }
      for (let bookmark of results.emptyFolders.values()) {
        await PlacesUtils.bookmarks.remove(bookmark).catch(log.error);
      }

      gFoldersMapPromise.then(map => map.clear());
    });
  },
};

/*
 * This function calculates the differences between the existing bookmarks
 * that are managed by the policy engine (which are known through a guid
 * prefix) and the specified bookmarks in the policy file.
 * They can differ if the policy file has changed.
 *
 * @param specifiedBookmarks
 *        This will be an array of bookmarks objects, as
 *        described on the top of this file.
 */
async function calculateLists(specifiedBookmarks) {
  // --------- STEP 1 ---------
  // Build two Maps (one with the existing bookmarks, another with
  // the specified bookmarks), to make iteration quicker.

  // LIST A
  // MAP of url (string) -> bookmarks objects from the Policy Engine
  let specifiedBookmarksMap = new Map();
  for (let bookmark of specifiedBookmarks) {
    specifiedBookmarksMap.set(bookmark.URL.href, bookmark);
  }

  // LIST B
  // MAP of url (string) -> bookmarks objects from Places
  let existingBookmarksMap = new Map();
  await PlacesUtils.bookmarks.fetch(
    { guidPrefix: BookmarksPolicies.BOOKMARK_GUID_PREFIX },
    bookmark => existingBookmarksMap.set(bookmark.url.href, bookmark)
  );

  // --------- STEP 2 ---------
  //
  //     /=====/====\=====\
  //    /     /      \     \
  //    |     |      |     |
  //    |  A  |  {}  |  B  |
  //    |     |      |     |
  //    \     \      /     /
  //     \=====\====/=====/
  //
  // Find the intersection of the two lists. Items in the intersection
  // are removed from the original lists.
  //
  // The items remaining in list A are new bookmarks to be added.
  // The items remaining in list B are old bookmarks to be removed.
  //
  // There's nothing to do with items in the intersection, so there's no
  // need to keep track of them.
  //
  // BONUS: It's necessary to keep track of the folder names that were
  // seen, to make sure we remove the ones that were left empty.

  let foldersSeen = new Set();

  for (let [url, item] of specifiedBookmarksMap) {
    foldersSeen.add(item.Folder);

    if (existingBookmarksMap.has(url)) {
      log.debug(`Bookmark intersection: ${url}`);
      // If this specified bookmark exists in the existing bookmarks list,
      // we can remove it from both lists as it's in the intersection.
      specifiedBookmarksMap.delete(url);
      existingBookmarksMap.delete(url);
    }
  }

  for (let url of specifiedBookmarksMap.keys()) {
    log.debug(`Bookmark to add: ${url}`);
  }

  for (let url of existingBookmarksMap.keys()) {
    log.debug(`Bookmark to remove: ${url}`);
  }

  // SET of folders to be deleted (bookmarks object from Places)
  let foldersToRemove = new Set();

  // If no bookmarks will be deleted, then no folder will
  // need to be deleted either, so this next section can be skipped.
  if (existingBookmarksMap.size > 0) {
    await PlacesUtils.bookmarks.fetch(
      { guidPrefix: BookmarksPolicies.FOLDER_GUID_PREFIX },
      folder => {
        if (!foldersSeen.has(folder.title)) {
          log.debug(`Folder to remove: ${folder.title}`);
          foldersToRemove.add(folder);
        }
      }
    );
  }

  return {
    add: specifiedBookmarksMap,
    remove: existingBookmarksMap,
    emptyFolders: foldersToRemove,
  };
}

async function insertBookmark(bookmark) {
  let parentGuid = await getParentGuid(bookmark.Placement, bookmark.Folder);

  await PlacesUtils.bookmarks.insert({
    url: Services.io.newURI(bookmark.URL.href),
    title: bookmark.Title,
    guid: PlacesUtils.generateGuidWithPrefix(
      BookmarksPolicies.BOOKMARK_GUID_PREFIX
    ),
    parentGuid,
  });

  if (bookmark.Favicon) {
    setFaviconForBookmark(bookmark);
  }
}

function setFaviconForBookmark(bookmark) {
  let faviconURI;
  let nullPrincipal = Services.scriptSecurityManager.createNullPrincipal({});

  switch (bookmark.Favicon.protocol) {
    case "data:":
      // data urls must first call replaceFaviconDataFromDataURL, using a
      // fake URL. Later, it's needed to call setAndFetchFaviconForPage
      // with the same URL.
      faviconURI = Services.io.newURI("fake-favicon-uri:" + bookmark.URL.href);

      PlacesUtils.favicons.replaceFaviconDataFromDataURL(
        faviconURI,
        bookmark.Favicon.href,
        0 /* max expiration length */,
        nullPrincipal
      );
      break;

    case "http:":
    case "https:":
      faviconURI = Services.io.newURI(bookmark.Favicon.href);
      break;

    default:
      log.error(`Bad URL given for favicon on bookmark "${bookmark.Title}"`);
      return;
  }

  PlacesUtils.favicons.setAndFetchFaviconForPage(
    Services.io.newURI(bookmark.URL.href),
    faviconURI,
    false /* forceReload */,
    PlacesUtils.favicons.FAVICON_LOAD_NON_PRIVATE,
    null,
    nullPrincipal
  );
}

// Cache of folder names to guids to be used by the getParentGuid
// function. The name consists in the parentGuid (which should always
// be the menuGuid or the toolbarGuid) + the folder title. This is to
// support having the same folder name in both the toolbar and menu.
XPCOMUtils.defineLazyGetter(this, "gFoldersMapPromise", () => {
  return new Promise(resolve => {
    let foldersMap = new Map();
    return PlacesUtils.bookmarks
      .fetch(
        {
          guidPrefix: BookmarksPolicies.FOLDER_GUID_PREFIX,
        },
        result => {
          foldersMap.set(`${result.parentGuid}|${result.title}`, result.guid);
        }
      )
      .then(() => resolve(foldersMap));
  });
});

async function getParentGuid(placement, folderTitle) {
  // Defaults to toolbar if no placement was given.
  let parentGuid =
    placement == "menu"
      ? PlacesUtils.bookmarks.menuGuid
      : PlacesUtils.bookmarks.toolbarGuid;

  if (!folderTitle) {
    // If no folderTitle is given, this bookmark is to be placed directly
    // into the toolbar or menu.
    return parentGuid;
  }

  let foldersMap = await gFoldersMapPromise;
  let folderName = `${parentGuid}|${folderTitle}`;

  if (foldersMap.has(folderName)) {
    return foldersMap.get(folderName);
  }

  let guid = PlacesUtils.generateGuidWithPrefix(
    BookmarksPolicies.FOLDER_GUID_PREFIX
  );
  await PlacesUtils.bookmarks.insert({
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: folderTitle,
    guid,
    parentGuid,
  });

  foldersMap.set(folderName, guid);
  return guid;
}
