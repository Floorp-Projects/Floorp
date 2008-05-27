const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{48a4e946-1f9f-4224-b4b0-9a54183cb81e}");

const NS_CHROME_MANIFESTS_FILE_LIST = "ChromeML";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

function ArrayEnumerator(array)
{
  this.array = array;
}

ArrayEnumerator.prototype = {
  pos: 0,
  
  hasMoreElements: function() {
    return this.pos < this.array.length;
  },
  
  getNext: function() {
    if (this.pos < this.array.length)
      return this.array[this.pos++];
    throw Cr.NS_ERROR_FAILURE;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsISimpleEnumerator)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

function ChromeProvider(manifests)
{
  this._manifests = manifests;
}

ChromeProvider.prototype = {
  getFile: function(prop, persistent) {
    throw Cr.NS_ERROR_FAILURE;
  },

  getFiles: function(prop) {
    if (prop == NS_CHROME_MANIFESTS_FILE_LIST) {
      return new ArrayEnumerator(this._manifests);
    }
    throw Cr.NS_ERROR_FAILURE;
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIDirectoryServiceProvider)
     || iid.equals(Ci.nsIDirectoryServiceProvider2)
     || iid.equals(Ci.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  }
};

function registerManifests(manifests)
{
  var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
    getService(Ci.nsIDirectoryService);
  dirSvc.registerProvider(new ChromeProvider(manifests));
}
