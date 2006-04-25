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
 * The Original Code is a service that enabled preference autocomplete
 *
 * The Initial Developer of the Original Code is IBM Corp.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


const PREF_AUTOCOMPLETE_SEARCH_CONTRACTID = "@mozilla.org/autocomplete/search;1?name=prefs";
const PREF_AUTOCOMPLETE_SEARCH_CLSID      = Components.ID('{f0747ed9-764a-4501-a7e5-4d1493605430}');

const nsIAutoCompleteSearch        = Components.interfaces.nsIAutoCompleteSearch;

const gPrefService = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefService);
const gPrefBranch = gPrefService.getBranch(null).QueryInterface(Components.interfaces.nsIPrefBranch2);

function PrefAutoCompleteSearchHandler() {}
PrefAutoCompleteSearchHandler.prototype =
{
  /* nsISupports */
  QueryInterface : function handler_QI(iid) {
    if (iid.equals(Components.interfaces.nsISupports))
      return this;

    if (nsIAutoCompleteSearch && iid.equals(nsIAutoCompleteSearch))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  /* nsIAutoCompleteSearch */
  startSearch : function startSearch(searchString, searchParam, previousResult, listener) {
    var simpleresult;
    if (Components.classes["@mozilla.org/autocomplete/simple-result;1"]) {
      var result = Components.classes["@mozilla.org/autocomplete/simple-result;1"].createInstance();
      simpleresult = result.QueryInterface(Components.interfaces.nsIAutoCompleteSimpleResult);
    } else {
      simpleresult = new AutoCompleteSimpleResult();
    }
    simpleresult.setSearchString(searchString);
    simpleresult.setDefaultIndex(0);
    var prefCount = { value: 0 };
    var prefArray = gPrefBranch.getChildList("", prefCount);
    prefArray = prefArray.sort();
    for (var i = 0; i < prefCount.value; ++i) 
    {
      if (prefArray[i].indexOf(searchString) == 0) { 
       simpleresult.appendMatch(prefArray[i], "");
      }
    }
    if (simpleresult.matchCount > 0) {
      simpleresult.setSearchResult(Components.interfaces.nsIAutoCompleteResult.RESULT_SUCCESS);
    } else {
      simpleresult.setSearchResult(Components.interfaces.nsIAutoCompleteResult.RESULT_NOMATCH);
    }
    listener.onSearchResult(this, simpleresult);  
  },

  stopSearch : function stopSearch() {
  }
};


var PrefAutoCompleteSearchFactory =
{
  createInstance : function(outer, iid)
  {
    if (outer != null) {
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    }

    return new PrefAutoCompleteSearchHandler().QueryInterface(iid);
  }
};


var PrefAutoCompleteSearchModule =
{
  registerSelf : function(compMgr, fileSpec, location, type)
  {
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.registerFactoryLocation(PREF_AUTOCOMPLETE_SEARCH_CLSID,
                                    "Preference Auto Complete Search",
                                    PREF_AUTOCOMPLETE_SEARCH_CONTRACTID,
                                    fileSpec,
                                    location,
                                    type);
  },

  unregisterSelf : function(compMgr, fileSpec, location)
  {
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);

    compMgr.unregisterFactoryLocation(PREF_AUTOCOMPLETE_SEARCH_CLSID, fileSpec);
  },

  getClassObject : function(compMgr, cid, iid)
  {
    if (cid.equals(PREF_AUTOCOMPLETE_SEARCH_CLSID)) {
      return PrefAutoCompleteSearchFactory;
    }

    if (!iid.equals(Components.interfaces.nsIFactory)) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    }

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  canUnload : function(compMgr)
  {
    return true;
  }
};


function NSGetModule(compMgr, fileSpec) {
  return PrefAutoCompleteSearchModule;
}


function AutoCompleteSimpleResult() {
  /* nsIAutoCompleteResult */
  this.searchString = "";
  this.searchResult = this.RESULT_NOMATCH;
  this.defaultIndex = -1;
  this.errorDescription = "";
  this.matchCount = 0;
  /* nsIAutoCompleteSimpleResult */
  this.values = new Array();
  this.comments = new Array();
}

AutoCompleteSimpleResult.prototype = {
  /* nsIAutoCompleteResult */
  RESULT_IGNORED: 1,
  RESULT_FAILURE: 2,
  RESULT_NOMATCH: 3,
  RESULT_SUCCESS: 4,

  getValueAt : function getValueAt(index) {
    return this.values[index];
  },

  getCommentAt : function getCommentAt(index) {
    return this.comments[index];
  },

  getStyleAt : function getStyleAt(index) {
  },

  removeValueAt : function removeValueAt(rowIndex, removeFromDb) {
    this.values.splice(rowIndex, 1);
  },

  /* nsIAutoCompleteSimpleResult */
  setSearchString : function setSearchString(aSearchString) {
    this.searchString = aSearchString;
  },

  setErrorDescription : function setErrorDescription(aErrorDescription) {
    this.errorDescription = aErrorDescription;
  },

  setDefaultIndex : function setDefaultIndex(aDefaultIndex) {
    this.defaultIndex = aDefaultIndex;
  },

  setSearchResult : function setSearchResult(aSearchResult) {
    this.searchResult = aSearchResult;
  },

  appendMatch : function appendMatch(aValue, aComment) {
    this.values.push(aValue);
    this.comments.push(aComment);
    this.matchCount++;
  }
}

