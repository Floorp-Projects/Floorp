/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

