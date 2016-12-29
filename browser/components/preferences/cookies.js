/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsICookie = Components.interfaces.nsICookie;

Components.utils.import("resource://gre/modules/AppConstants.jsm");
Components.utils.import("resource://gre/modules/PluralForm.jsm");
Components.utils.import("resource://gre/modules/Services.jsm")
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ContextualIdentityService",
                                  "resource://gre/modules/ContextualIdentityService.jsm");

var gCookiesWindow = {
  _cm               : Components.classes["@mozilla.org/cookiemanager;1"]
                                .getService(Components.interfaces.nsICookieManager),
  _hosts            : {},
  _hostOrder        : [],
  _tree             : null,
  _bundle           : null,

  init: function() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.addObserver(this, "cookie-changed", false);
    os.addObserver(this, "perm-changed", false);

    this._bundle = document.getElementById("bundlePreferences");
    this._tree = document.getElementById("cookiesList");

    this._populateList(true);

    document.getElementById("filter").focus();

    if (!Services.prefs.getBoolPref("privacy.userContext.enabled")) {
      document.getElementById("userContextRow").hidden = true;
    }
  },

  uninit: function() {
    var os = Components.classes["@mozilla.org/observer-service;1"]
                       .getService(Components.interfaces.nsIObserverService);
    os.removeObserver(this, "cookie-changed");
    os.removeObserver(this, "perm-changed");
  },

  _populateList: function(aInitialLoad) {
    this._loadCookies();
    this._tree.view = this._view;
    if (aInitialLoad)
      this.sort("rawHost");
    if (this._view.rowCount > 0)
      this._tree.view.selection.select(0);

    if (aInitialLoad) {
      if ("arguments" in window &&
          window.arguments[0] &&
          window.arguments[0].filterString)
        this.setFilter(window.arguments[0].filterString);
    }
    else if (document.getElementById("filter").value != "") {
      this.filter();
    }

    this._updateRemoveAllButton();

    this._saveState();
  },

  _cookieEquals: function(aCookieA, aCookieB, aStrippedHost) {
    return aCookieA.rawHost == aStrippedHost &&
           aCookieA.name == aCookieB.name &&
           aCookieA.path == aCookieB.path &&
           ChromeUtils.isOriginAttributesEqual(aCookieA.originAttributes,
                                               aCookieB.originAttributes);
  },

  _isPrivateCookie: function(aCookie) {
      let { userContextId } = aCookie.originAttributes;
      if (!userContextId) {
        // Default identity is public.
        return false;
      }
      return !ContextualIdentityService.getIdentityFromId(userContextId).public;
  },

  observe: function(aCookie, aTopic, aData) {
    if (aTopic != "cookie-changed")
      return;

    if (aCookie instanceof Components.interfaces.nsICookie) {
      if (this._isPrivateCookie(aCookie)) {
        return;
      }

      var strippedHost = this._makeStrippedHost(aCookie.host);
      if (aData == "changed")
        this._handleCookieChanged(aCookie, strippedHost);
      else if (aData == "added")
        this._handleCookieAdded(aCookie, strippedHost);
    }
    else if (aData == "cleared") {
      this._hosts = {};
      this._hostOrder = [];

      var oldRowCount = this._view._rowCount;
      this._view._rowCount = 0;
      this._tree.treeBoxObject.rowCountChanged(0, -oldRowCount);
      this._view.selection.clearSelection();
      this._updateRemoveAllButton();
    }
    else if (aData == "reload") {
      // first, clear any existing entries
      this.observe(aCookie, aTopic, "cleared");

      // then, reload the list
      this._populateList(false);
    }

    // We don't yet handle aData == "deleted" - it's a less common case
    // and is rather complicated as selection tracking is difficult
  },

  _handleCookieChanged: function(changedCookie, strippedHost) {
    var rowIndex = 0;
    var cookieItem = null;
    if (!this._view._filtered) {
      for (let host of this._hostOrder) {
        ++rowIndex;
        var hostItem = this._hosts[host];
        if (host == strippedHost) {
          // Host matches, look for the cookie within this Host collection
          // and update its data
          for (let currCookie of hostItem.cookies) {
            ++rowIndex;
            if (this._cookieEquals(currCookie, changedCookie, strippedHost)) {
              currCookie.value    = changedCookie.value;
              currCookie.isSecure = changedCookie.isSecure;
              currCookie.isDomain = changedCookie.isDomain;
              currCookie.expires  = changedCookie.expires;
              cookieItem = currCookie;
              break;
            }
          }
        }
        else if (hostItem.open)
          rowIndex += hostItem.cookies.length;
      }
    }
    else {
      // Just walk the filter list to find the item. It doesn't matter that
      // we don't update the main Host collection when we do this, because
      // when the filter is reset the Host collection is rebuilt anyway.
      for (let currCookie of this._view._filterSet) {
        if (this._cookieEquals(currCookie, changedCookie, strippedHost)) {
          currCookie.value    = changedCookie.value;
          currCookie.isSecure = changedCookie.isSecure;
          currCookie.isDomain = changedCookie.isDomain;
          currCookie.expires  = changedCookie.expires;
          cookieItem = currCookie;
          break;
        }
      }
    }

    // Make sure the tree display is up to date...
    this._tree.treeBoxObject.invalidateRow(rowIndex);
    // ... and if the cookie is selected, update the displayed metadata too
    if (cookieItem != null && this._view.selection.currentIndex == rowIndex)
      this._updateCookieData(cookieItem);
  },

  _handleCookieAdded: function(changedCookie, strippedHost) {
    var rowCountImpact = 0;
    var addedHost = { value: 0 };
    this._addCookie(strippedHost, changedCookie, addedHost);
    if (!this._view._filtered) {
      // The Host collection for this cookie already exists, and it's not open,
      // so don't increment the rowCountImpact becaues the user is not going to
      // see the additional rows as they're hidden.
      if (addedHost.value || this._hosts[strippedHost].open)
        ++rowCountImpact;
    }
    else {
      // We're in search mode, and the cookie being added matches
      // the search condition, so add it to the list.
      var c = this._makeCookieObject(strippedHost, changedCookie);
      if (this._cookieMatchesFilter(c)) {
        this._view._filterSet.push(this._makeCookieObject(strippedHost, changedCookie));
        ++rowCountImpact;
      }
    }
    // Now update the tree display at the end (we could/should re run the sort
    // if any to get the position correct.)
    var oldRowCount = this._rowCount;
    this._view._rowCount += rowCountImpact;
    this._tree.treeBoxObject.rowCountChanged(oldRowCount - 1, rowCountImpact);

    this._updateRemoveAllButton();
  },

  _view: {
    _filtered   : false,
    _filterSet  : [],
    _filterValue: "",
    _rowCount   : 0,
    _cacheValid : 0,
    _cacheItems : [],
    get rowCount() {
      return this._rowCount;
    },

    _getItemAtIndex: function(aIndex) {
      if (this._filtered)
        return this._filterSet[aIndex];

      var start = 0;
      var count = 0, hostIndex = 0;

      var cacheIndex = Math.min(this._cacheValid, aIndex);
      if (cacheIndex > 0) {
        var cacheItem = this._cacheItems[cacheIndex];
        start = cacheItem['start'];
        count = hostIndex = cacheItem['count'];
      }

      for (let i = start; i < gCookiesWindow._hostOrder.length; ++i) { // var host in gCookiesWindow._hosts) {
        let currHost = gCookiesWindow._hosts[gCookiesWindow._hostOrder[i]];// gCookiesWindow._hosts[host];
        if (!currHost) continue;
        if (count == aIndex)
          return currHost;
        hostIndex = count;

        var cacheEntry = { 'start' : i, 'count' : count };
        var cacheStart = count;

        if (currHost.open) {
          if (count < aIndex && aIndex <= (count + currHost.cookies.length)) {
            // We are looking for an entry within this host's children,
            // enumerate them looking for the index.
            ++count;
            for (let cookie of currHost.cookies) {
              if (count == aIndex) {
                cookie.parentIndex = hostIndex;
                return cookie;
              }
              ++count;
            }
          }
          else {
            // A host entry was open, but we weren't looking for an index
            // within that host entry's children, so skip forward over the
            // entry's children. We need to add one to increment for the
            // host value too.
            count += currHost.cookies.length + 1;
          }
        }
        else
          ++count;

        for (let k = cacheStart; k < count; k++)
          this._cacheItems[k] = cacheEntry;
        this._cacheValid = count - 1;
      }
      return null;
    },

    _removeItemAtIndex: function(aIndex, aCount) {
      let removeCount = aCount === undefined ? 1 : aCount;
      if (this._filtered) {
        // remove the cookies from the unfiltered set so that they
        // don't reappear when the filter is changed. See bug 410863.
        for (let i = aIndex; i < aIndex + removeCount; ++i) {
          let item = this._filterSet[i];
          let parent = gCookiesWindow._hosts[item.rawHost];
          for (let j = 0; j < parent.cookies.length; ++j) {
            if (item == parent.cookies[j]) {
              parent.cookies.splice(j, 1);
              break;
            }
          }
        }
        this._filterSet.splice(aIndex, removeCount);
        return;
      }

      let item = this._getItemAtIndex(aIndex);
      if (!item) return;
      this._invalidateCache(aIndex - 1);
      if (item.container) {
        gCookiesWindow._hosts[item.rawHost] = null;
      } else {
        let parent = this._getItemAtIndex(item.parentIndex);
        for (let i = 0; i < parent.cookies.length; ++i) {
          let cookie = parent.cookies[i];
          if (item.rawHost == cookie.rawHost &&
              item.name == cookie.name &&
              item.path == cookie.path &&
              ChromeUtils.isOriginAttributesEqual(item.originAttributes,
                                                  cookie.originAttributes)) {
            parent.cookies.splice(i, removeCount);
          }
        }
      }
    },

    _invalidateCache: function(aIndex) {
      this._cacheValid = Math.min(this._cacheValid, aIndex);
    },

    getCellText: function(aIndex, aColumn) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item)
          return "";
        if (aColumn.id == "domainCol")
          return item.rawHost;
        else if (aColumn.id == "nameCol")
          return item.name;
      }
      else if (aColumn.id == "domainCol") {
        return this._filterSet[aIndex].rawHost;
      } else if (aColumn.id == "nameCol") {
        return this._filterSet[aIndex].name;
      }
      return "";
    },

    _selection: null,
    get selection() { return this._selection; },
    set selection(val) { this._selection = val; return val; },
    getRowProperties: function(aIndex) { return ""; },
    getCellProperties: function(aIndex, aColumn) { return ""; },
    getColumnProperties: function(aColumn) { return ""; },
    isContainer: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item) return false;
        return item.container;
      }
      return false;
    },
    isContainerOpen: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item) return false;
        return item.open;
      }
      return false;
    },
    isContainerEmpty: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item) return false;
        return item.cookies.length == 0;
      }
      return false;
    },
    isSeparator: function(aIndex) { return false; },
    isSorted: function(aIndex) { return false; },
    canDrop: function(aIndex, aOrientation) { return false; },
    drop: function(aIndex, aOrientation) {},
    getParentIndex: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        // If an item has no parent index (i.e. it is at the top level) this
        // function MUST return -1 otherwise we will go into an infinite loop.
        // Containers are always top level items in the cookies tree, so make
        // sure to return the appropriate value here.
        if (!item || item.container) return -1;
        return item.parentIndex;
      }
      return -1;
    },
    hasNextSibling: function(aParentIndex, aIndex) {
      if (!this._filtered) {
        // |aParentIndex| appears to be bogus, but we can get the real
        // parent index by getting the entry for |aIndex| and reading the
        // parentIndex field.
        // The index of the last item in this host collection is the
        // index of the parent + the size of the host collection, and
        // aIndex has a next sibling if it is less than this value.
        var item = this._getItemAtIndex(aIndex);
        if (item) {
          if (item.container) {
            for (let i = aIndex + 1; i < this.rowCount; ++i) {
              var subsequent = this._getItemAtIndex(i);
              if (subsequent.container)
                return true;
            }
            return false;
          }
          var parent = this._getItemAtIndex(item.parentIndex);
          if (parent && parent.container)
            return aIndex < item.parentIndex + parent.cookies.length;
        }
      }
      return aIndex < this.rowCount - 1;
    },
    hasPreviousSibling: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item) return false;
        var parent = this._getItemAtIndex(item.parentIndex);
        if (parent && parent.container)
          return aIndex > item.parentIndex + 1;
      }
      return aIndex > 0;
    },
    getLevel: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item) return 0;
        return item.level;
      }
      return 0;
    },
    getImageSrc: function(aIndex, aColumn) {},
    getProgressMode: function(aIndex, aColumn) {},
    getCellValue: function(aIndex, aColumn) {},
    setTree: function(aTree) {},
    toggleOpenState: function(aIndex) {
      if (!this._filtered) {
        var item = this._getItemAtIndex(aIndex);
        if (!item) return;
        this._invalidateCache(aIndex);
        var multiplier = item.open ? -1 : 1;
        var delta = multiplier * item.cookies.length;
        this._rowCount += delta;
        item.open = !item.open;
        gCookiesWindow._tree.treeBoxObject.rowCountChanged(aIndex + 1, delta);
        gCookiesWindow._tree.treeBoxObject.invalidateRow(aIndex);
      }
    },
    cycleHeader: function(aColumn) {},
    selectionChanged: function() {},
    cycleCell: function(aIndex, aColumn) {},
    isEditable: function(aIndex, aColumn) {
      return false;
    },
    isSelectable: function(aIndex, aColumn) {
      return false;
    },
    setCellValue: function(aIndex, aColumn, aValue) {},
    setCellText: function(aIndex, aColumn, aValue) {},
    performAction: function(aAction) {},
    performActionOnRow: function(aAction, aIndex) {},
    performActionOnCell: function(aAction, aindex, aColumn) {}
  },

  _makeStrippedHost: function(aHost) {
    var formattedHost = aHost.charAt(0) == "." ? aHost.substring(1, aHost.length) : aHost;
    return formattedHost.substring(0, 4) == "www." ? formattedHost.substring(4, formattedHost.length) : formattedHost;
  },

  _addCookie: function(aStrippedHost, aCookie, aHostCount) {
    if (!(aStrippedHost in this._hosts) || !this._hosts[aStrippedHost]) {
      this._hosts[aStrippedHost] = { cookies   : [],
                                     rawHost   : aStrippedHost,
                                     level     : 0,
                                     open      : false,
                                     container : true };
      this._hostOrder.push(aStrippedHost);
      ++aHostCount.value;
    }

    var c = this._makeCookieObject(aStrippedHost, aCookie);
    this._hosts[aStrippedHost].cookies.push(c);
  },

  _makeCookieObject: function(aStrippedHost, aCookie) {
    var c = { name            : aCookie.name,
              value           : aCookie.value,
              isDomain        : aCookie.isDomain,
              host            : aCookie.host,
              rawHost         : aStrippedHost,
              path            : aCookie.path,
              isSecure        : aCookie.isSecure,
              expires         : aCookie.expires,
              level           : 1,
              container       : false,
              originAttributes: aCookie.originAttributes };
    return c;
  },

  _loadCookies: function() {
    var e = this._cm.enumerator;
    var hostCount = { value: 0 };
    this._hosts = {};
    this._hostOrder = [];
    while (e.hasMoreElements()) {
      var cookie = e.getNext();
      if (cookie && cookie instanceof Components.interfaces.nsICookie) {
        if (this._isPrivateCookie(cookie)) {
          continue;
        }

        var strippedHost = this._makeStrippedHost(cookie.host);
        this._addCookie(strippedHost, cookie, hostCount);
      }
      else
        break;
    }
    this._view._rowCount = hostCount.value;
  },

  formatExpiresString: function(aExpires) {
    if (aExpires) {
      var date = new Date(1000 * aExpires);
      const locale = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
                     .getService(Components.interfaces.nsIXULChromeRegistry)
                     .getSelectedLocale("global", true);
      const dtOptions = { year: 'numeric', month: 'long', day: 'numeric',
                          hour: 'numeric', minute: 'numeric', second: 'numeric' };
      return date.toLocaleString(locale, dtOptions);
    }
    return this._bundle.getString("expireAtEndOfSession");
  },

  _getUserContextString: function(aUserContextId) {
    if (parseInt(aUserContextId) == 0) {
      return this._bundle.getString("defaultUserContextLabel");
    }

    return ContextualIdentityService.getUserContextLabel(aUserContextId);
  },

  _updateCookieData: function(aItem) {
    var seln = this._view.selection;
    var ids = ["name", "value", "host", "path", "isSecure", "expires", "userContext"];
    var properties;

    if (aItem && !aItem.container && seln.count > 0) {
      properties = { name: aItem.name, value: aItem.value, host: aItem.host,
                     path: aItem.path, expires: this.formatExpiresString(aItem.expires),
                     isDomain: aItem.isDomain ? this._bundle.getString("domainColon")
                                              : this._bundle.getString("hostColon"),
                     isSecure: aItem.isSecure ? this._bundle.getString("forSecureOnly")
                                              : this._bundle.getString("forAnyConnection"),
                     userContext: this._getUserContextString(aItem.originAttributes.userContextId) };
      for (let id of ids) {
        document.getElementById(id).disabled = false;
      }
    }
    else {
      var noneSelected = this._bundle.getString("noCookieSelected");
      properties = { name: noneSelected, value: noneSelected, host: noneSelected,
                     path: noneSelected, expires: noneSelected,
                     isSecure: noneSelected, userContext: noneSelected };
      for (let id of ids) {
        document.getElementById(id).disabled = true;
      }
    }
    for (let property in properties)
      document.getElementById(property).value = properties[property];
  },

  onCookieSelected: function() {
    var item;
    var seln = this._tree.view.selection;
    if (!this._view._filtered)
      item = this._view._getItemAtIndex(seln.currentIndex);
    else
      item = this._view._filterSet[seln.currentIndex];

    this._updateCookieData(item);

    var rangeCount = seln.getRangeCount();
    var selectedCookieCount = 0;
    for (let i = 0; i < rangeCount; ++i) {
      var min = {}; var max = {};
      seln.getRangeAt(i, min, max);
      for (let j = min.value; j <= max.value; ++j) {
        item = this._view._getItemAtIndex(j);
        if (!item) continue;
        if (item.container)
          selectedCookieCount += item.cookies.length;
        else if (!item.container)
          ++selectedCookieCount;
      }
    }

    let buttonLabel = this._bundle.getString("removeSelectedCookies");
    let removeSelectedCookies = document.getElementById("removeSelectedCookies");
    removeSelectedCookies.label = PluralForm.get(selectedCookieCount, buttonLabel)
                                            .replace("#1", selectedCookieCount);

    removeSelectedCookies.disabled = !(seln.count > 0);
  },

  performDeletion: function gCookiesWindow_performDeletion(deleteItems) {
    var psvc = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    var blockFutureCookies = false;
    if (psvc.prefHasUserValue("network.cookie.blockFutureCookies"))
      blockFutureCookies = psvc.getBoolPref("network.cookie.blockFutureCookies");
    for (let item of deleteItems) {
      this._cm.remove(item.host, item.name, item.path,
                      blockFutureCookies, item.originAttributes);
    }
  },

  deleteCookie: function() {
    // Selection Notes
    // - Selection always moves to *NEXT* adjacent item unless item
    //   is last child at a given level in which case it moves to *PREVIOUS*
    //   item
    //
    // Selection Cases (Somewhat Complicated)
    //
    // 1) Single cookie selected, host has single child
    //    v cnn.com
    //    //// cnn.com ///////////// goksdjf@ ////
    //    > atwola.com
    //
    //    Before SelectedIndex: 1   Before RowCount: 3
    //    After  SelectedIndex: 0   After  RowCount: 1
    //
    // 2) Host selected, host open
    //    v goats.com ////////////////////////////
    //         goats.com             sldkkfjl
    //         goat.scom             flksj133
    //    > atwola.com
    //
    //    Before SelectedIndex: 0   Before RowCount: 4
    //    After  SelectedIndex: 0   After  RowCount: 1
    //
    // 3) Host selected, host closed
    //    > goats.com ////////////////////////////
    //    > atwola.com
    //
    //    Before SelectedIndex: 0   Before RowCount: 2
    //    After  SelectedIndex: 0   After  RowCount: 1
    //
    // 4) Single cookie selected, host has many children
    //    v goats.com
    //         goats.com             sldkkfjl
    //    //// goats.com /////////// flksjl33 ////
    //    > atwola.com
    //
    //    Before SelectedIndex: 2   Before RowCount: 4
    //    After  SelectedIndex: 1   After  RowCount: 3
    //
    // 5) Single cookie selected, host has many children
    //    v goats.com
    //    //// goats.com /////////// flksjl33 ////
    //         goats.com             sldkkfjl
    //    > atwola.com
    //
    //    Before SelectedIndex: 1   Before RowCount: 4
    //    After  SelectedIndex: 1   After  RowCount: 3
    var seln = this._view.selection;
    var tbo = this._tree.treeBoxObject;

    if (seln.count < 1) return;

    var nextSelected = 0;
    var rowCountImpact = 0;
    var deleteItems = [];
    if (!this._view._filtered) {
      var ci = seln.currentIndex;
      nextSelected = ci;
      var invalidateRow = -1;
      var item = this._view._getItemAtIndex(ci);
      if (item.container) {
        rowCountImpact -= (item.open ? item.cookies.length : 0) + 1;
        deleteItems = deleteItems.concat(item.cookies);
        if (!this._view.hasNextSibling(-1, ci))
          --nextSelected;
        this._view._removeItemAtIndex(ci);
      }
      else {
        var parent = this._view._getItemAtIndex(item.parentIndex);
        --rowCountImpact;
        if (parent.cookies.length == 1) {
          --rowCountImpact;
          deleteItems.push(item);
          if (!this._view.hasNextSibling(-1, ci))
            --nextSelected;
          if (!this._view.hasNextSibling(-1, item.parentIndex))
            --nextSelected;
          this._view._removeItemAtIndex(item.parentIndex);
          invalidateRow = item.parentIndex;
        }
        else {
          deleteItems.push(item);
          if (!this._view.hasNextSibling(-1, ci))
            --nextSelected;
          this._view._removeItemAtIndex(ci);
        }
      }
      this._view._rowCount += rowCountImpact;
      tbo.rowCountChanged(ci, rowCountImpact);
      if (invalidateRow != -1)
        tbo.invalidateRow(invalidateRow);
    }
    else {
      var rangeCount = seln.getRangeCount();
      // Traverse backwards through selections to avoid messing
      // up the indices when they are deleted.
      // See bug 388079.
      for (let i = rangeCount - 1; i >= 0; --i) {
        var min = {}; var max = {};
        seln.getRangeAt(i, min, max);
        nextSelected = min.value;
        for (let j = min.value; j <= max.value; ++j) {
          deleteItems.push(this._view._getItemAtIndex(j));
          if (!this._view.hasNextSibling(-1, max.value))
            --nextSelected;
        }
        var delta = max.value - min.value + 1;
        this._view._removeItemAtIndex(min.value, delta);
        rowCountImpact = -1 * delta;
        this._view._rowCount += rowCountImpact;
        tbo.rowCountChanged(min.value, rowCountImpact);
      }
    }

    this.performDeletion(deleteItems);

    if (nextSelected < 0)
      seln.clearSelection();
    else {
      seln.select(nextSelected);
      this._tree.focus();
    }
  },

  deleteAllCookies: function() {
    if (this._view._filtered) {
      var rowCount = this._view.rowCount;
      var deleteItems = [];
      for (let index = 0; index < rowCount; index++) {
        deleteItems.push(this._view._getItemAtIndex(index));
      }
      this._view._removeItemAtIndex(0, rowCount);
      this._view._rowCount = 0;
      this._tree.treeBoxObject.rowCountChanged(0, -rowCount);
      this.performDeletion(deleteItems);
    }
    else {
      this._cm.removeAll();
    }
    this._updateRemoveAllButton();
    this.focusFilterBox();
  },

  onCookieKeyPress: function(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_DELETE) {
      this.deleteCookie();
    } else if (AppConstants.platform == "macosx" &&
               aEvent.keyCode == KeyEvent.DOM_VK_BACK_SPACE) {
      this.deleteCookie();
    }
  },

  _lastSortProperty : "",
  _lastSortAscending: false,
  sort: function(aProperty) {
    var ascending = (aProperty == this._lastSortProperty) ? !this._lastSortAscending : true;
    // Sort the Non-Filtered Host Collections
    if (aProperty == "rawHost") {
      function sortByHost(a, b) {
        return a.toLowerCase().localeCompare(b.toLowerCase());
      }
      this._hostOrder.sort(sortByHost);
      if (!ascending)
        this._hostOrder.reverse();
    }

    function sortByProperty(a, b) {
      return a[aProperty].toLowerCase().localeCompare(b[aProperty].toLowerCase());
    }
    for (let host in this._hosts) {
      var cookies = this._hosts[host].cookies;
      cookies.sort(sortByProperty);
      if (!ascending)
        cookies.reverse();
    }
    // Sort the Filtered List, if in Filtered mode
    if (this._view._filtered) {
      this._view._filterSet.sort(sortByProperty);
      if (!ascending)
        this._view._filterSet.reverse();
    }

    // Adjust the Sort Indicator
    var domainCol = document.getElementById("domainCol");
    var nameCol = document.getElementById("nameCol");
    var sortOrderString = ascending ? "ascending" : "descending";
    if (aProperty == "rawHost") {
      domainCol.setAttribute("sortDirection", sortOrderString);
      nameCol.removeAttribute("sortDirection");
    }
    else {
      nameCol.setAttribute("sortDirection", sortOrderString);
      domainCol.removeAttribute("sortDirection");
    }

    this._view._invalidateCache(0);
    this._view.selection.clearSelection();
    this._view.selection.select(0);
    this._tree.treeBoxObject.invalidate();
    this._tree.treeBoxObject.ensureRowIsVisible(0);

    this._lastSortAscending = ascending;
    this._lastSortProperty = aProperty;
  },

  clearFilter: function() {
    // Revert to single-select in the tree
    this._tree.setAttribute("seltype", "single");

    // Clear the Tree Display
    this._view._filtered = false;
    this._view._rowCount = 0;
    this._tree.treeBoxObject.rowCountChanged(0, -this._view._filterSet.length);
    this._view._filterSet = [];

    // Just reload the list to make sure deletions are respected
    this._loadCookies();
    this._tree.view = this._view;

    // Restore sort order
    var sortby = this._lastSortProperty;
    if (sortby == "") {
      this._lastSortAscending = false;
      this.sort("rawHost");
    }
    else {
      this._lastSortAscending = !this._lastSortAscending;
      this.sort(sortby);
    }

    // Restore open state
    for (let openIndex of this._openIndices) {
      this._view.toggleOpenState(openIndex);
    }
    this._openIndices = [];

    // Restore selection
    this._view.selection.clearSelection();
    for (let range of this._lastSelectedRanges) {
      this._view.selection.rangedSelect(range.min, range.max, true);
    }
    this._lastSelectedRanges = [];

    document.getElementById("cookiesIntro").value = this._bundle.getString("cookiesAll");
    this._updateRemoveAllButton();
  },

  _cookieMatchesFilter: function(aCookie) {
    return aCookie.rawHost.indexOf(this._view._filterValue) != -1 ||
           aCookie.name.indexOf(this._view._filterValue) != -1 ||
           aCookie.value.indexOf(this._view._filterValue) != -1;
  },

  _filterCookies: function(aFilterValue) {
    this._view._filterValue = aFilterValue;
    var cookies = [];
    for (let i = 0; i < gCookiesWindow._hostOrder.length; ++i) { // var host in gCookiesWindow._hosts) {
      let currHost = gCookiesWindow._hosts[gCookiesWindow._hostOrder[i]]; // gCookiesWindow._hosts[host];
      if (!currHost) continue;
      for (let cookie of currHost.cookies) {
        if (this._cookieMatchesFilter(cookie))
          cookies.push(cookie);
      }
    }
    return cookies;
  },

  _lastSelectedRanges: [],
  _openIndices: [],
  _saveState: function() {
    // Save selection
    var seln = this._view.selection;
    this._lastSelectedRanges = [];
    var rangeCount = seln.getRangeCount();
    for (let i = 0; i < rangeCount; ++i) {
      var min = {}; var max = {};
      seln.getRangeAt(i, min, max);
      this._lastSelectedRanges.push({ min: min.value, max: max.value });
    }

    // Save open states
    this._openIndices = [];
    for (let i = 0; i < this._view.rowCount; ++i) {
      var item = this._view._getItemAtIndex(i);
      if (item && item.container && item.open)
        this._openIndices.push(i);
    }
  },

  _updateRemoveAllButton: function gCookiesWindow__updateRemoveAllButton() {
    document.getElementById("removeAllCookies").disabled = this._view._rowCount == 0;
  },

  filter: function() {
    var filter = document.getElementById("filter").value;
    if (filter == "") {
      gCookiesWindow.clearFilter();
      return;
    }
    var view = gCookiesWindow._view;
    view._filterSet = gCookiesWindow._filterCookies(filter);
    if (!view._filtered) {
      // Save Display Info for the Non-Filtered mode when we first
      // enter Filtered mode.
      gCookiesWindow._saveState();
      view._filtered = true;
    }
    // Move to multi-select in the tree
    gCookiesWindow._tree.setAttribute("seltype", "multiple");

    // Clear the display
    var oldCount = view._rowCount;
    view._rowCount = 0;
    gCookiesWindow._tree.treeBoxObject.rowCountChanged(0, -oldCount);
    // Set up the filtered display
    view._rowCount = view._filterSet.length;
    gCookiesWindow._tree.treeBoxObject.rowCountChanged(0, view.rowCount);

    // if the view is not empty then select the first item
    if (view.rowCount > 0)
      view.selection.select(0);

    document.getElementById("cookiesIntro").value = gCookiesWindow._bundle.getString("cookiesFiltered");
    this._updateRemoveAllButton();
  },

  setFilter: function(aFilterString) {
    document.getElementById("filter").value = aFilterString;
    this.filter();
  },

  focusFilterBox: function() {
    var filter = document.getElementById("filter");
    filter.focus();
    filter.select();
  },

  onWindowKeyPress: function(aEvent) {
    if (aEvent.keyCode == KeyEvent.DOM_VK_ESCAPE)
      window.close();
  }
};
