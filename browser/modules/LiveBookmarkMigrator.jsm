/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
});

XPCOMUtils.defineLazyGlobalGetters(this, ["URL", "XMLSerializer"]);

XPCOMUtils.defineLazyGetter(this, "gBrowserBundle", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

const kMigrationPref = "browser.livebookmarks.migrationAttemptsLeft";

function migrationSucceeded() {
  Services.prefs.clearUserPref(kMigrationPref);
}

function migrationError() {
  // Decrement the number of remaining attempts.
  let remainingAttempts = Math.max(
    0,
    Services.prefs.getIntPref(kMigrationPref, 1) - 1
  );
  Services.prefs.setIntPref(kMigrationPref, remainingAttempts);
}

var LiveBookmarkMigrator = {
  _isOldDefaultBookmark(liveBookmark) {
    if (!liveBookmark.feedURI || !liveBookmark.feedURI.host) {
      return false;
    }
    let { host } = liveBookmark.feedURI;
    return (
      host.endsWith("fxfeeds.mozilla.com") ||
      host.endsWith("fxfeeds.mozilla.org")
    );
  },

  async _fetch() {
    function getAnnoSQLFragment(aAnnoParam) {
      return `SELECT a.content
              FROM moz_items_annos a
              JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id
              WHERE a.item_id = b.id
                AND n.name = ${aAnnoParam}`;
    }

    // Copied and modified from nsLivemarkService.js, which we'll want to remove even when
    // we keep this migration for a while.
    const LB_SQL = `SELECT b.title, b.guid, b.dateAdded, b.position as 'index', p.guid AS parentGuid,
              ( ${getAnnoSQLFragment(":feedURI_anno")} ) AS feedURI,
              ( ${getAnnoSQLFragment(":siteURI_anno")} ) AS siteURI
            FROM moz_bookmarks b
            JOIN moz_bookmarks p ON b.parent = p.id
            JOIN moz_items_annos a ON a.item_id = b.id
            JOIN moz_anno_attributes n ON a.anno_attribute_id = n.id
            WHERE b.type = :folder_type
              AND n.name = :feedURI_anno
      ORDER BY b.position DESC`;
    // We sort by position so we go over items last-to-first. This way, we can insert a
    // duplicate "normal" bookmark for each livemark, without causing future insertions
    // to be off-by-N in their positioning because of the insertions.

    let conn = await PlacesUtils.promiseDBConnection();
    let rows = await conn.execute(LB_SQL, {
      folder_type: Ci.nsINavBookmarksService.TYPE_FOLDER,
      feedURI_anno: PlacesUtils.LMANNO_FEEDURI,
      siteURI_anno: PlacesUtils.LMANNO_SITEURI,
    });
    // Create a JS object out of the sqlite result:
    let liveBookmarks = [];
    for (let row of rows) {
      let siteURI = row.getResultByName("siteURI");
      let feedURI = row.getResultByName("feedURI");
      try {
        feedURI = new URL(feedURI);
        siteURI = siteURI ? new URL(siteURI) : null;
      } catch (ex) {
        // Skip items with broken URLs:
        Cu.reportError(ex);
        continue;
      }
      liveBookmarks.push({
        guid: row.getResultByName("guid"),
        index: row.getResultByName("index"),
        dateAdded: PlacesUtils.toDate(row.getResultByName("dateAdded")),
        parentGuid: row.getResultByName("parentGuid"),
        title: row.getResultByName("title"),
        siteURI,
        feedURI,
      });
    }
    return liveBookmarks;
  },

  async _writeOPML(liveBookmarks) {
    const appName = Services.appinfo.name;
    let hiddenBrowser = Services.appShell.createWindowlessBrowser();
    let opmlString = "";
    try {
      let hiddenDOMDoc = hiddenBrowser.document;
      // Create head:
      let doc = hiddenDOMDoc.implementation.createDocument("", "opml", null);
      let root = doc.documentElement;
      root.setAttribute("version", "1.0");
      let head = doc.createElement("head");
      root.appendChild(head);
      let title = doc.createElement("title");
      title.textContent = gBrowserBundle.formatStringFromName(
        "livebookmarkMigration.title",
        [appName]
      );
      head.appendChild(title);

      let body = doc.createElement("body");
      root.appendChild(body);
      // Make things vaguely readable:
      body.textContent = "\n";

      for (let lb of liveBookmarks) {
        if (this._isOldDefaultBookmark(lb)) {
          // Ignore the old default bookmarks and don't back them up.
          continue;
        }
        let outline = doc.createElement("outline");
        outline.setAttribute("type", "rss");
        outline.setAttribute("title", lb.title);
        outline.setAttribute("text", lb.title);
        outline.setAttribute("xmlUrl", lb.feedURI.href);
        if (lb.siteURI) {
          outline.setAttribute("htmlUrl", lb.siteURI.href);
        }
        body.appendChild(outline);
        body.appendChild(doc.createTextNode("\n"));
      }

      let serializer = new XMLSerializer();
      // The serializer doesn't add an XML declaration (bug 318086), so we add it manually.
      opmlString =
        '<?xml version="1.0"?>\n' + serializer.serializeToString(doc);
    } finally {
      hiddenBrowser.close();
    }

    let { path: basePath } = Services.dirsvc.get("Desk", Ci.nsIFile);
    let feedFileName = appName + " feeds backup.opml";
    basePath = OS.Path.join(basePath, feedFileName);
    let { file, path } = await OS.File.openUnique(basePath, {
      humanReadable: true,
    });
    await file.close();
    return OS.File.writeAtomic(path, opmlString, { encoding: "utf-8" });
  },

  async _transformBookmarks(liveBookmarks) {
    let itemsToReplace = liveBookmarks.filter(
      lb => !this._isOldDefaultBookmark(lb)
    );
    let itemsToInsert = itemsToReplace.map(item => ({
      url: item.siteURI || item.feedURI,
      parentGuid: item.parentGuid,
      index: item.index,
      title: item.title,
      dateAdded: item.dateAdded,
    }));
    // Insert new bookmarks at the same index. The list is sorted by position
    // in reverse order, so we'll insert later items before the earlier ones.
    // This avoids the indices getting outdated for later insertions.
    for (let item of itemsToInsert) {
      await PlacesUtils.bookmarks.insert(item).catch(Cu.reportError);
    }
    // Now remove all of the actual live bookmarks. Avoid mismatches due to the
    // bookmarks having been moved or altered in the meantime, just remove
    // anything with a matching guid:
    let itemsToRemove = liveBookmarks.map(lb => ({ guid: lb.guid }));
    await PlacesUtils.bookmarks.remove(itemsToRemove).catch(Cu.reportError);
  },

  _openSUMOPage() {
    let sumoURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "live-bookmarks-migration";
    let topWin = BrowserWindowTracker.getTopWindow({ private: false });
    if (!topWin) {
      let args = PlacesUtils.toISupportsString(sumoURL);
      Services.ww.openWindow(
        null,
        AppConstants.BROWSER_CHROME_URL,
        "_blank",
        "chrome,dialog=no,all",
        args
      );
    } else {
      topWin.openTrustedLinkIn(sumoURL, "tab");
    }
  },

  async migrate() {
    try {
      // First fetch all live bookmark folders:
      let liveBookmarks = await this._fetch();
      if (!liveBookmarks || !liveBookmarks.length) {
        migrationSucceeded();
        return;
      }

      let haveNonDefaultBookmarks = liveBookmarks.some(
        lb => !this._isOldDefaultBookmark(lb)
      );
      // Then generate OPML file content, write to disk, if we've got anything to back up:
      if (haveNonDefaultBookmarks) {
        await this._writeOPML(liveBookmarks);
      }
      // Replace all live bookmarks with normal bookmarks.
      await this._transformBookmarks(liveBookmarks).catch(ex => {
        // Don't stop migrating at this point, because we've written the exported OPML file.
        // We shouldn't ever hit this - transformLiveBookmarks is supposed to take
        // care of its own failures.
        Cu.reportError(ex);
      });

      try {
        if (haveNonDefaultBookmarks) {
          this._openSUMOPage();
        }
      } catch (ex) {
        // Note that if we get here, we've removed any extant livemarks, so there's no point
        // re-running the migration - there won't be any livemarks left and we won't re-show the SUMO
        // page. So just report the error, but mark migration as successful.
        Cu.reportError(
          new Error(
            "Live bookmarks migration didn't manage to show the support page: " +
              ex
          )
        );
      }
    } catch (ex) {
      migrationError();
      throw ex;
    }

    migrationSucceeded();
  },
};

var EXPORTED_SYMBOLS = ["LiveBookmarkMigrator"];
