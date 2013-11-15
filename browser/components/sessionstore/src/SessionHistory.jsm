/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["SessionHistory"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivacyLevel",
  "resource:///modules/sessionstore/PrivacyLevel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Utils",
  "resource:///modules/sessionstore/Utils.jsm");

function debug(msg) {
  Services.console.logStringMessage("SessionHistory: " + msg);
}

// The preference value that determines how much post data to save.
XPCOMUtils.defineLazyGetter(this, "gPostData", function () {
  const PREF = "browser.sessionstore.postdata";

  // Observer that updates the cached value when the preference changes.
  Services.prefs.addObserver(PREF, () => {
    this.gPostData = Services.prefs.getIntPref(PREF);
  }, false);

  return Services.prefs.getIntPref(PREF);
});

/**
 * The external API exported by this module.
 */
this.SessionHistory = Object.freeze({
  collect: function (docShell, includePrivateData) {
    return SessionHistoryInternal.collect(docShell, includePrivateData);
  },

  restore: function (docShell, tabData) {
    SessionHistoryInternal.restore(docShell, tabData);
  }
});

/**
 * The internal API for the SessionHistory module.
 */
let SessionHistoryInternal = {
  /**
   * Collects session history data for a given docShell.
   *
   * @param docShell
   *        The docShell that owns the session history.
   * @param includePrivateData (optional)
   *        True to always include private data and skip any privacy checks.
   */
  collect: function (docShell, includePrivateData = false) {
    let data = {entries: []};
    let isPinned = docShell.isAppTab;
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory;

    if (history && history.count > 0) {
      try {
        for (let i = 0; i < history.count; i++) {
          let shEntry = history.getEntryAtIndex(i, false);
          let entry = this.serializeEntry(shEntry, includePrivateData, isPinned);
          data.entries.push(entry);
        }
      } catch (ex) {
        // In some cases, getEntryAtIndex will throw. This seems to be due to
        // history.count being higher than it should be. By doing this in a
        // try-catch, we'll update history to where it breaks, print an error
        // message, and still save sessionstore.js.
        debug("SessionStore failed gathering complete history " +
              "for the focused window/tab. See bug 669196.");
      }

      // Ensure the index isn't out of bounds if an exception was thrown above.
      data.index = Math.min(history.index + 1, data.entries.length);
    }

    // If either the session history isn't available yet or doesn't have any
    // valid entries, make sure we at least include the current page.
    if (data.entries.length == 0) {
      let uri = webNavigation.currentURI.spec;
      // We landed here because the history is inaccessible or there are no
      // history entries. In that case we should at least record the docShell's
      // current URL as a single history entry. If the URL is not about:blank
      // or it's a blank tab that was modified (like a custom newtab page),
      // record it. For about:blank we explicitly want an empty array without
      // an 'index' property to denote that there are no history entries.
      if (uri != "about:blank" || webNavigation.document.body.hasChildNodes()) {
        data.entries.push({ url: uri });
        data.index = 1;
      }
    }

    return data;
  },

  /**
   * Get an object that is a serialized representation of a History entry.
   *
   * @param shEntry
   *        nsISHEntry instance
   * @param includePrivateData
   *        Always return privacy sensitive data (use with care).
   * @param isPinned
   *        The tab is pinned and should be treated differently for privacy.
   * @return object
   */
  serializeEntry: function (shEntry, includePrivateData, isPinned) {
    let entry = { url: shEntry.URI.spec };

    // Save some bytes and don't include the title property
    // if that's identical to the current entry's URL.
    if (shEntry.title && shEntry.title != entry.url) {
      entry.title = shEntry.title;
    }
    if (shEntry.isSubFrame) {
      entry.subframe = true;
    }

    let cacheKey = shEntry.cacheKey;
    if (cacheKey && cacheKey instanceof Ci.nsISupportsPRUint32 &&
        cacheKey.data != 0) {
      // XXXbz would be better to have cache keys implement
      // nsISerializable or something.
      entry.cacheKey = cacheKey.data;
    }
    entry.ID = shEntry.ID;
    entry.docshellID = shEntry.docshellID;

    // We will include the property only if it's truthy to save a couple of
    // bytes when the resulting object is stringified and saved to disk.
    if (shEntry.referrerURI)
      entry.referrer = shEntry.referrerURI.spec;

    if (shEntry.srcdocData)
      entry.srcdocData = shEntry.srcdocData;

    if (shEntry.isSrcdocEntry)
      entry.isSrcdocEntry = shEntry.isSrcdocEntry;

    if (shEntry.contentType)
      entry.contentType = shEntry.contentType;

    let x = {}, y = {};
    shEntry.getScrollPosition(x, y);
    if (x.value != 0 || y.value != 0)
      entry.scroll = x.value + "," + y.value;

    // Collect post data for the current history entry.
    try {
      let postdata = this.serializePostData(shEntry, isPinned);
      if (postdata) {
        entry.postdata_b64 = postdata;
      }
    } catch (ex) {
      // POSTDATA is tricky - especially since some extensions don't get it right
      debug("Failed serializing post data: " + ex);
    }

    // Collect owner data for the current history entry.
    try {
      let owner = this.serializeOwner(shEntry);
      if (owner) {
        entry.owner_b64 = owner;
      }
    } catch (ex) {
      // Not catching anything specific here, just possible errors
      // from writeCompoundObject() and the like.
      debug("Failed serializing owner data: " + ex);
    }

    entry.docIdentifier = shEntry.BFCacheEntry.ID;

    if (shEntry.stateData != null) {
      entry.structuredCloneState = shEntry.stateData.getDataAsBase64();
      entry.structuredCloneVersion = shEntry.stateData.formatVersion;
    }

    if (!(shEntry instanceof Ci.nsISHContainer)) {
      return entry;
    }

    if (shEntry.childCount > 0) {
      let children = [];
      for (let i = 0; i < shEntry.childCount; i++) {
        let child = shEntry.GetChildAt(i);

        if (child) {
          // Don't try to restore framesets containing wyciwyg URLs.
          // (cf. bug 424689 and bug 450595)
          if (child.URI.schemeIs("wyciwyg")) {
            children.length = 0;
            break;
          }

          children.push(this.serializeEntry(child, includePrivateData, isPinned));
        }
      }

      if (children.length) {
        entry.children = children;
      }
    }

    return entry;
  },

  /**
   * Serialize post data contained in the given session history entry.
   *
   * @param shEntry
   *        The session history entry.
   * @param isPinned
   *        Whether the docShell is owned by a pinned tab.
   * @return The base64 encoded post data.
   */
  serializePostData: function (shEntry, isPinned) {
    let isHttps = shEntry.URI.schemeIs("https");
    if (!shEntry.postData || !gPostData ||
        !PrivacyLevel.canSave({isHttps: isHttps, isPinned: isPinned})) {
      return null;
    }

    shEntry.postData.QueryInterface(Ci.nsISeekableStream)
                    .seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
    let stream = Cc["@mozilla.org/binaryinputstream;1"]
                   .createInstance(Ci.nsIBinaryInputStream);
    stream.setInputStream(shEntry.postData);
    let postBytes = stream.readByteArray(stream.available());
    let postdata = String.fromCharCode.apply(null, postBytes);
    if (gPostData != -1 &&
        postdata.replace(/^(Content-.*\r\n)+(\r\n)*/, "").length > gPostData) {
      return null;
    }

    // We can stop doing base64 encoding once our serialization into JSON
    // is guaranteed to handle all chars in strings, including embedded
    // nulls.
    return btoa(postdata);
  },

  /**
   * Serialize owner data contained in the given session history entry.
   *
   * @param shEntry
   *        The session history entry.
   * @return The base64 encoded owner data.
   */
  serializeOwner: function (shEntry) {
    if (!shEntry.owner) {
      return null;
    }

    let binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].
                       createInstance(Ci.nsIObjectOutputStream);
    let pipe = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);
    pipe.init(false, false, 0, 0xffffffff, null);
    binaryStream.setOutputStream(pipe.outputStream);
    binaryStream.writeCompoundObject(shEntry.owner, Ci.nsISupports, true);
    binaryStream.close();

    // Now we want to read the data from the pipe's input end and encode it.
    let scriptableStream = Cc["@mozilla.org/binaryinputstream;1"].
                           createInstance(Ci.nsIBinaryInputStream);
    scriptableStream.setInputStream(pipe.inputStream);
    let ownerBytes =
      scriptableStream.readByteArray(scriptableStream.available());

    // We can stop doing base64 encoding once our serialization into JSON
    // is guaranteed to handle all chars in strings, including embedded
    // nulls.
    return btoa(String.fromCharCode.apply(null, ownerBytes));
  },

  /**
   * Restores session history data for a given docShell.
   *
   * @param docShell
   *        The docShell that owns the session history.
   * @param tabData
   *        The tabdata including all history entries.
   */
  restore: function (docShell, tabData) {
    let webNavigation = docShell.QueryInterface(Ci.nsIWebNavigation);
    let history = webNavigation.sessionHistory;

    if (history.count > 0) {
      history.PurgeHistory(history.count);
    }
    history.QueryInterface(Ci.nsISHistoryInternal);

    let idMap = { used: {} };
    let docIdentMap = {};
    for (let i = 0; i < tabData.entries.length; i++) {
      //XXXzpao Wallpaper patch for bug 514751
      if (!tabData.entries[i].url)
        continue;
      history.addEntry(this.deserializeEntry(tabData.entries[i],
                                             idMap, docIdentMap), true);
    }
  },

  /**
   * Expands serialized history data into a session-history-entry instance.
   *
   * @param entry
   *        Object containing serialized history data for a URL
   * @param idMap
   *        Hash for ensuring unique frame IDs
   * @param docIdentMap
   *        Hash to ensure reuse of BFCache entries
   * @returns nsISHEntry
   */
  deserializeEntry: function (entry, idMap, docIdentMap) {

    var shEntry = Cc["@mozilla.org/browser/session-history-entry;1"].
                  createInstance(Ci.nsISHEntry);

    shEntry.setURI(Utils.makeURI(entry.url));
    shEntry.setTitle(entry.title || entry.url);
    if (entry.subframe)
      shEntry.setIsSubFrame(entry.subframe || false);
    shEntry.loadType = Ci.nsIDocShellLoadInfo.loadHistory;
    if (entry.contentType)
      shEntry.contentType = entry.contentType;
    if (entry.referrer)
      shEntry.referrerURI = Utils.makeURI(entry.referrer);
    if (entry.isSrcdocEntry)
      shEntry.srcdocData = entry.srcdocData;

    if (entry.cacheKey) {
      var cacheKey = Cc["@mozilla.org/supports-PRUint32;1"].
                     createInstance(Ci.nsISupportsPRUint32);
      cacheKey.data = entry.cacheKey;
      shEntry.cacheKey = cacheKey;
    }

    if (entry.ID) {
      // get a new unique ID for this frame (since the one from the last
      // start might already be in use)
      var id = idMap[entry.ID] || 0;
      if (!id) {
        for (id = Date.now(); id in idMap.used; id++);
        idMap[entry.ID] = id;
        idMap.used[id] = true;
      }
      shEntry.ID = id;
    }

    if (entry.docshellID)
      shEntry.docshellID = entry.docshellID;

    if (entry.structuredCloneState && entry.structuredCloneVersion) {
      shEntry.stateData =
        Cc["@mozilla.org/docshell/structured-clone-container;1"].
        createInstance(Ci.nsIStructuredCloneContainer);

      shEntry.stateData.initFromBase64(entry.structuredCloneState,
                                       entry.structuredCloneVersion);
    }

    if (entry.scroll) {
      var scrollPos = (entry.scroll || "0,0").split(",");
      scrollPos = [parseInt(scrollPos[0]) || 0, parseInt(scrollPos[1]) || 0];
      shEntry.setScrollPosition(scrollPos[0], scrollPos[1]);
    }

    if (entry.postdata_b64) {
      var postdata = atob(entry.postdata_b64);
      var stream = Cc["@mozilla.org/io/string-input-stream;1"].
                   createInstance(Ci.nsIStringInputStream);
      stream.setData(postdata, postdata.length);
      shEntry.postData = stream;
    }

    let childDocIdents = {};
    if (entry.docIdentifier) {
      // If we have a serialized document identifier, try to find an SHEntry
      // which matches that doc identifier and adopt that SHEntry's
      // BFCacheEntry.  If we don't find a match, insert shEntry as the match
      // for the document identifier.
      let matchingEntry = docIdentMap[entry.docIdentifier];
      if (!matchingEntry) {
        matchingEntry = {shEntry: shEntry, childDocIdents: childDocIdents};
        docIdentMap[entry.docIdentifier] = matchingEntry;
      }
      else {
        shEntry.adoptBFCacheEntry(matchingEntry.shEntry);
        childDocIdents = matchingEntry.childDocIdents;
      }
    }

    if (entry.owner_b64) {
      var ownerInput = Cc["@mozilla.org/io/string-input-stream;1"].
                       createInstance(Ci.nsIStringInputStream);
      var binaryData = atob(entry.owner_b64);
      ownerInput.setData(binaryData, binaryData.length);
      var binaryStream = Cc["@mozilla.org/binaryinputstream;1"].
                         createInstance(Ci.nsIObjectInputStream);
      binaryStream.setInputStream(ownerInput);
      try { // Catch possible deserialization exceptions
        shEntry.owner = binaryStream.readObject(true);
      } catch (ex) { debug(ex); }
    }

    if (entry.children && shEntry instanceof Ci.nsISHContainer) {
      for (var i = 0; i < entry.children.length; i++) {
        //XXXzpao Wallpaper patch for bug 514751
        if (!entry.children[i].url)
          continue;

        // We're getting sessionrestore.js files with a cycle in the
        // doc-identifier graph, likely due to bug 698656.  (That is, we have
        // an entry where doc identifier A is an ancestor of doc identifier B,
        // and another entry where doc identifier B is an ancestor of A.)
        //
        // If we were to respect these doc identifiers, we'd create a cycle in
        // the SHEntries themselves, which causes the docshell to loop forever
        // when it looks for the root SHEntry.
        //
        // So as a hack to fix this, we restrict the scope of a doc identifier
        // to be a node's siblings and cousins, and pass childDocIdents, not
        // aDocIdents, to _deserializeHistoryEntry.  That is, we say that two
        // SHEntries with the same doc identifier have the same document iff
        // they have the same parent or their parents have the same document.

        shEntry.AddChild(this.deserializeEntry(entry.children[i], idMap,
                                               childDocIdents), i);
      }
    }

    return shEntry;
  },

};
