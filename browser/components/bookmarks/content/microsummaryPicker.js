//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Microsummarizer.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// XXX Synchronize these values with the VARIANT constants in Places code?
const ADD_BOOKMARK_MODE = 0;
const EDIT_BOOKMARK_MODE = 1;

const Cc = Components.classes;
const Ci = Components.interfaces;

var MicrosummaryPicker = {

  // The microsummary service.
  __mss: null,
  get _mss() {
    if (!this.__mss)
      this.__mss = Cc["@mozilla.org/microsummary/service;1"].
                   getService(Ci.nsIMicrosummaryService);
    return this.__mss;
  },

  // The IO service.
  __ios: null,
  get _ios() {
    if (!this.__ios)
      this.__ios = Cc["@mozilla.org/network/io-service;1"].
                   getService(Ci.nsIIOService);
    return this.__ios;
  },

  // The URI of the page being bookmarked.
  get _pageURI() {
    if (this._mode == ADD_BOOKMARK_MODE)
      return this._ios.newURI(gArg.url, null, null);
    else if (this._mode == EDIT_BOOKMARK_MODE) {
      var uriResource = BMDS.GetTarget(gResource, gProperties[1], true);
      if (!uriResource)
        return null;

      // The URI spec will be the empty string if the user opened the bookmark
      // properties dialog via the "New Bookmark" command.
      var uriSpec = uriResource.QueryInterface(Ci.nsIRDFLiteral).Value;
      if (!uriSpec)
        return null;

      // The IO Service will throw an exception if it can't convert the spec
      // into an nsIURI object.
      var uri;
      try {
        uri = this._ios.newURI(uriSpec, null, null);
      }
      catch(e) {
        return null;
      }

      return uri;
    }

    return null;
  },

  // The unique identifier for the bookmark.  If we're adding a bookmark,
  // this may not exist yet.
  get _bookmarkID() {
    if (this._mode == ADD_BOOKMARK_MODE && typeof gResource == "undefined")
        return null;

    return gResource;
  },

  _microsummaries: null,

  get _mode() {
    if ("gArg" in window)
      return ADD_BOOKMARK_MODE;
    else if ("gProperties" in window)
      return EDIT_BOOKMARK_MODE;

    return null;
  },

  /*
   * Whether or not the microsummary picker is enabled for this bookmark.
   * The picker is enabled for regular pages.  It's disabled for livemarks,
   * separators, folders, or when bookmarking multiple tabs.
   *
   * @returns boolean true if the picker is enabled; false otherwise
   *
   */
  get enabled() {
    if (this._mode == ADD_BOOKMARK_MODE) {
      // If we're adding a bookmark, we only have to worry about livemarks
      // and bookmarking multiple tabs.
      if ("feedURL" in gArg || gArg.bBookmarkAllTabs)
        return false;
    }
    else if (this._mode == EDIT_BOOKMARK_MODE) {
      // If we're modifying a bookmark, it could be a livemark, separator,
      // folder, or regular page.  The picker is only enabled for regular pages.
      var isLivemark = BookmarksUtils.resolveType(gResource) == "Livemark";
      var isSeparator = BookmarksUtils.resolveType(gResource) == "BookmarkSeparator";
      var isContainer = RDFCU.IsContainer(BMDS, gResource);
      if (isLivemark || isSeparator || isContainer)
        return false;
    }
    else {
      // We should never get to this point, since we're only being used
      // in the Add Bookmark and Bookmark Properties dialogs, but if we're here
      // for some reason, be conservative and assume the picker is disabled.
      return false;
    }
  
    // We haven't found a reason to disable the picker, so say it's enabled.
    return true;
  },

  init: function MSP_init() {
    // Set the label of the menu item representing the user-entered name.
    this._updateUserEnteredNameItem();

    if (this._pageURI) {
      this._microsummaries = this._mss.getMicrosummaries(this._pageURI, this._bookmarkID);
      this._microsummaries.addObserver(this._observer);
      this.rebuild();
    }
  },

  destroy: function MSP_destroy() {
    if (this._pageURI && this._microsummaries)
      this._microsummaries.removeObserver(this._observer);
  },

  rebuild: function MSP_rebuild() {
    var microsummaryMenuList = document.getElementById("name");
    var microsummaryMenuPopup = document.getElementById("microsummaryMenuPopup");
  
    // Remove old items from the menu, except for the first item, which holds
    // the user-entered name, and the second item, which separates and labels
    // the microsummaries below it.
    while (microsummaryMenuPopup.childNodes.length > 2)
      microsummaryMenuPopup.removeChild(microsummaryMenuPopup.lastChild);
  
    var enumerator = this._microsummaries.Enumerate();

    // Show the drop marker if there are microsummaries; otherwise hide it.
    // We do this via a microsummary picker-specific "droppable" attribute
    // that, when set to "false", activates a "display: none" CSS rule
    // on the drop marker.
    if (enumerator.hasMoreElements())
      microsummaryMenuList.setAttribute("droppable", "true");
    else
      microsummaryMenuList.setAttribute("droppable", "false");

    while (enumerator.hasMoreElements()) {
      var microsummary = enumerator.getNext().QueryInterface(Ci.nsIMicrosummary);
  
      var menuItem = document.createElement("menuitem");
  
      // Store a reference to the microsummary in the menu item, so we know
      // which microsummary this menu item represents when it's time to save
      // changes to the datastore.
      menuItem.microsummary = microsummary;
  
      // Content may have to be generated asynchronously; we don't necessarily
      // have it now.  If we do, great; otherwise, fall back to the generator
      // name, then the URI, and we trigger a microsummary content update.
      // Once the update completes, the microsummary will notify our observer
      // to rebuild the menu.
      // XXX Instead of just showing the generator name or (heaven forbid)
      // its URI when we don't have content, we should tell the user that we're
      // loading the microsummary, perhaps with some throbbing to let her know
      // it's in progress.
      if (microsummary.content != null)
        menuItem.setAttribute("label", microsummary.content);
      else {
        menuItem.setAttribute("label", microsummary.generator ? microsummary.generator.name
                                                               : microsummary.generatorURI.spec);
        microsummary.update();
      }

      microsummaryMenuPopup.appendChild(menuItem);
  
      // Select the item if this is the current microsummary for the bookmark.
      if (this._bookmarkID && this._mss.isMicrosummary(this._bookmarkID, microsummary))
          microsummaryMenuList.selectedItem = menuItem;
    }
  },

  _observer: {
    interfaces: [Ci.nsIMicrosummaryObserver, Ci.nsISupports],
  
    QueryInterface: function (iid) {
      //if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      if (!iid.equals(Ci.nsIMicrosummaryObserver) &&
          !iid.equals(Ci.nsISupports))
        throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
    },
  
    onContentLoaded: function(microsummary) {
      MicrosummaryPicker.rebuild();
    },
  
    onElementAppended: function(microsummary) {
      MicrosummaryPicker.rebuild();
    }
  },

  /**
   * Called when the user types in the microsummary picker's text box.
   * 
   * @param   event
   *          the event object representing the input event
   *
   */
  onInput: function MSP_onInput(event) {
    this._updateUserEnteredNameItem();
  },

  /**
   * Update the menu item representing the user-entered name when the user
   * changes the name by typing in the text box.
   *
   */
  _updateUserEnteredNameItem: function MSP__updateUserEnteredNameItem() {
    var nameField = document.getElementById("name");
    var nameItem = document.getElementById("userEnteredNameItem");
    nameItem.label = nameField.value;
  },

  commit: function MSP_commit() {
    var changed = false;
    var menuList = document.getElementById("name");

    // Something should always be selected in the microsummary menu,
    // but if nothing is selected, then conservatively assume we should
    // just display the bookmark title.
    if (menuList.selectedIndex == -1)
      menuList.selectedIndex = 0;

    // This will set microsummary == undefined if the user selected
    // the "don't display a microsummary" item.
    var newMicrosummary = menuList.selectedItem.microsummary;

    if (newMicrosummary == null && this._mss.hasMicrosummary(this._bookmarkID)) {
      this._mss.removeMicrosummary(this._bookmarkID);
      changed = true;
    }
    else if (newMicrosummary != null
             && !this._mss.isMicrosummary(this._bookmarkID, newMicrosummary)) {
      this._mss.setMicrosummary(this._bookmarkID, newMicrosummary);
      changed = true;
    }

    return changed;
  }
};

function debug(str) {
  dump(str + "\n");
}
