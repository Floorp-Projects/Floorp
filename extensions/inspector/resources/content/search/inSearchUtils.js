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

