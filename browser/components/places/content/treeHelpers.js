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
 * The Original Code is the Places Tree View Utilities.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <beng@google.com>
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

/**
 * Manages options for a particular view type.
 * @param   pref
 *          The preference that stores these options. 
 * @param   defaultValue
 *          The default value to be used for views of this type. 
 * @param   serializable
 *          An object bearing a serialize and deserialize method that
 *          read and write the object's string representation from/to
 *          preferences.
 * @constructor
 */
function PrefHandler(pref, defaultValue, serializable) {
  this._pref = pref;
  this._defaultValue = defaultValue;
  this._serializable = serializable;

  this._pb = 
    Cc["@mozilla.org/preferences-service;1"].
    getService(Components.interfaces.nsIPrefBranch2);
  this._pb.addObserver(this._pref, this, false);
}
PrefHandler.prototype = {
  /**
   * Clean up when the window is going away to avoid leaks. 
   */
  destroy: function PC_PH_destroy() {
    this._pb.removeObserver(this._pref, this);
  },

  /** 
   * Observes changes to the preferences.
   * @param   subject
   * @param   topic
   *          The preference changed notification
   * @param   data
   *          The preference that changed
   */
  observe: function PC_PH_observe(subject, topic, data) {
    if (topic == "nsPref:changed" && data == this._pref)
      this._value = null;
  },
  
  /**
   * The cached value, null if it needs to be rebuilt from preferences.
   */
  _value: null,

  /** 
   * Get the preference value, reading from preferences if necessary. 
   */
  get value() { 
    if (!this._value) {
      if (this._pb.prefHasUserValue(this._pref)) {
        var valueString = this._pb.getCharPref(this._pref);
        this._value = this._serializable.deserialize(valueString);
      }
      else
        this._value = this._defaultValue;
    }
    return this._value;
  },
  
  /**
   * Stores a value in preferences. 
   * @param   value
   *          The data to be stored. 
   */
  set value(value) {
    if (value != this._value) {
      this._pb.setCharPref(this._pref, this._serializable.serialize(value));
      var ps = this._pb.QueryInterface(Ci.nsIPrefService);
      ps.savePrefFile(null);
    }
    return value;
  }
};

// The preferences where the OptionsFilter stores user settings for different
// view types:
const PREF_PLACES_ORGANIZER_OPTIONS_HISTORY = 
  "browser.places.organizer.options.history";
const PREF_PLACES_ORGANIZER_OPTIONS_BOOKMARKS = 
  "browser.places.organizer.options.bookmarks";
const PREF_PLACES_ORGANIZER_OPTIONS_SUBSCRIPTIONS = 
  "browser.places.organizer.options.subscriptions";

/**
 * OptionsFilter is an object that handles persistence of certain 
 * user-controlled options wrt. the content view. For instance, if the user
 * sorts a view of a certain type, the sort state should be persisted across
 * all instances of that view type, and across browser sessions. Same story for
 * grouping. 
 * 
 * The OptionsFilter is called by any function that is going to load a new
 * result in the content view, by calling |filter| on the options that are to
 * be passed into load():
 * 
 *   this._content.load(queries, OptionsFilter.filter(queries, options, null));
 *
 * The OptionsFilter overrides any options specified by the caller with the
 * last user values.
 *
 * When an event happens in the user interface that causes the view to change
 * outside the control of the OptionsFilter, e.g. the user clicks the sort 
 * headers to sort a column, the caller can update the state held by the 
 * OptionsFilter by calling update:
 *
 *   OptionsFilter.update(this._content.getResult());
 *
 */
var OptionsFilter = {
  /**
   * Deserializes query options from a place: URI stored in preferences
   * @param   string
   *          The serialized options in place: URI format from preferences
   * @returns A nsINavHistoryQueryOptions object representing the last saved
   *          options
   */
  deserialize: function OF_deserialize(string) {
    var optionsRef = { };
    PlacesController.history.queryStringToQueries(string, {}, {}, optionsRef);
    return optionsRef.value;    
  },
  
  /**
   * Serializes query options to a place: URI stored in preferences
   * @param   options
   *          A nsINavHistoryQueryOptions object representing the options to 
   *          serialize.
   * @returns A place: URI string that can be stored in preferences.
   */
  serialize: function OF_serialize(options) {
    var query = PlacesController.history.getNewQuery();
    return PlacesController.history.queriesToQueryString([query], 1, options);
  },
  
  /**
   * Handler for History and other generic queries
   */
  historyHandler: null,

  /**
   * Handler for Bookmark Folders
   */
  bookmarksHandler: null,

  /**
   * Substring->PrefHandler hash for results that show content with specific
   * annotations to register custom handlers under. 
   */
  overrideHandlers: { },
  
  /**
   * A grouping utility object that is notified when the view is filtered.
   */
  _grouper: null,

  /**
   * Initializes the OptionsFilter, sets up the default handlers.
   */
  init: function OF_init(grouper) {
    this._grouper = grouper;
  
    var history = PlacesController.history;
    const NHQO = Ci.nsINavHistoryQueryOptions;
    
    var defaultHistoryOptions = history.getNewQueryOptions();
    defaultHistoryOptions.sortingMode = NHQO.SORT_BY_DATE_DESCENDING;
    var defaultBookmarksOptions = history.getNewQueryOptions();
    defaultBookmarksOptions.setGroupingMode([NHQO.GROUP_BY_FOLDER], 1);
    var defaultSubscriptionsOptions = history.getNewQueryOptions();
  
    this.historyHandler = 
      new PrefHandler(PREF_PLACES_ORGANIZER_OPTIONS_HISTORY, 
                      defaultHistoryOptions, this);
    this.bookmarksHandler = 
      new PrefHandler(PREF_PLACES_ORGANIZER_OPTIONS_BOOKMARKS, 
                      defaultBookmarksOptions, this);
    this.overrideHandlers["livemark/"] = 
      new PrefHandler(PREF_PLACES_ORGANIZER_OPTIONS_SUBSCRIPTIONS, 
                      defaultSubscriptionsOptions, this);
  },
  
  /**
   * Destroy the OptionsFilter handlers (to avoid leaks).
   */
  destroy: function OF_destroy() {
    this.historyHandler.destroy();
    this.bookmarksHandler.destroy();
    this.overrideHandlers["livemark/"].destroy();
  },
  
  /**
   * Gets the handler best able to store options for a set of queries that are
   * about to be executed.
   * @param   queries
   *          An array of queries that are about to be executed
   * @returns A PrefHandler object that is the best handler to load/save options
   *          for the given queries.
   */
  getHandler: function OF_getHandler(queries) {
    var countRef = { };
    queries[0].getFolders(countRef);
    if (countRef.value > 0 || queries[0].onlyBookmarks)
      return this.bookmarksHandler;
    for (var substr in this.overrideHandlers) {
      if (queries[0].annotation.substr(0, substr.length) == substr) 
        return this.overrideHandlers[substr];
    }
    return this.historyHandler;
  },
  
  /**
   * Filters a set of queryOptions according to the rules for the type of 
   * result that will be generated.
   * @param   queries
   *          The set of queries that is about to be executed.
   * @param   options
   *          The set of query options for the queries that need to be 
   *          filtered.
   * @param   handler
   *          Optionally specify which handler to use to filter the options.
   *          If null, the default handler for the queries will be used.
   */
  filter: function OF_filter(queries, options, handler) {
    if (!handler)
      handler = this.getHandler(queries);
    var overrideOptions = handler.value;
    
    var overrideGroupings = overrideOptions.getGroupingMode({});
    options.setGroupingMode(overrideGroupings, overrideGroupings.length);
    options.sortingMode = overrideOptions.sortingMode;
    
    this._grouper.updateGroupingUI(queries, options, handler);

    return options;
  },
  
  /**
   * Updates an override state with preferences so that it is shared across
   * views/sessions.
   * @param   result
   *          The nsINavHistoryResult to update options for
   */
  update: function OF_update(result) {
    var queryNode = asQuery(result.root);
    var queries = queryNode.getQueries({});
    
    var options = queryNode.queryOptions.clone();
    options.sortingMode = result.sortingMode;
    this.getHandler(queries).value = options;
  },
};
