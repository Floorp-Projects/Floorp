/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

/***************************************************************
* inSearchUtils -------------------------------------------------
*  Utilities for helping search modules accomplish common tasks.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

var kSearchLookup = {
  file: "inIFileSearch", 
  cssvalue: "inICSSValueSearch"
};

//////////// global constants ////////////////////

const kISMLNSURI = "http://www.mozilla.org/inspector/isml";

const kSearchHelperCIDPrefix    = "@mozilla.org/inspector/search;1?type=";
const kFileSearchCID            = "@mozilla.org/inspector/search;1?type=file";
const kCSSValueSearchCID        = "@mozilla.org/inspector/search;1?type=cssvalue";
const kLocalFileCID             = "@mozilla.org/file/local;1";
const kInMemoryDataSourceCID    = "@mozilla.org/rdf/datasource;1?name=in-memory-datasource";

////////////////////////////////////////////////////////////////////////////
//// class inSearchUtils

var inSearchUtils = 
{
  // nsISupports
  createSearchHelper: function(aName)
  {
    var cid = kSearchHelperCIDPrefix + aName;
    var iid = kSearchLookup[aName];
    return XPCU.createInstance(cid, iid);
  },
  
  // nsIFile 
  createLocalFile: function(aPath)
  {
    var file = XPCU.createInstance(kLocalFileCID, "nsILocalFile");
    
    if (aPath) {
      try {
        file.initWithPath(aPath);
      } catch (ex) {
        debug("Invalid path in nsILocalFile::initWithPath\n" + ex);
      }
    }
    
    return XPCU.QI(file, "nsIFile");    
  },
  
  // inISearchObserver
  createSearchObserver: function(aTarget, aLabel)
  {
    var observer = {
      mTarget: aTarget,
      onSearchStart: new Function("aModule", "this.mTarget.on"+aLabel+"SearchStart(aModule)"),
      onSearchEnd: new Function("aModule", "aResult", "this.mTarget.on"+aLabel+"SearchEnd(aModule, aResult)"),
      onSearchError: new Function("aModule", "aMsg", "this.mTarget.on"+aLabel+"SearchError(aModule, aMsg)"),
      onSearchResult: new Function("aModule", "this.mTarget.on"+aLabel+"SearchResult(aModule)")
    };
    
    return observer;
  }

};

