/***************************************************************
* DiskSearch -------------------------------------------------
*  A utility for handily searching the disk for files matching
*  a certain criteria.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

////////////////////////////////////////////////////////////////////////////
//// class DiskSearch

var DiskSearch =
{
  findFiles: function(aRootDir, aExtList, aRecurse)
  {
    // has the file extensions so we don't have to 
    // linear search the array every time through
    var extHash = {};
    for (var i = 0; i < aExtList.length; i++) {
      extHash[aExtList[i]] = true;
    }

    // recursively build the list of results
    var results = [];
    this.recurseDir(aRootDir, extHash, aRecurse, results);
    return results;
  },

  recurseDir: function(aDir, aExtHash, aRecurse, aResults)
  {
    debug("("+aResults.length+") entering " + aDir.path + "\n");
    var entries = aDir.directoryEntries;
    var entry, ext;
    while (entries.hasMoreElements()) {
      entry = XPCU.QI(entries.getNext(), "nsIFile");
      if (aRecurse && entry.isDirectory())
        this.recurseDir(entry, aExtHash, aRecurse, aResults);
      ext = this.getExtension(entry.leafName);
      if (ext) {
        if (aExtHash[ext])
          aResults.push(entry.URL);
      }
    }
  },

  getExtension: function(aFileName)
  {
    var dotpt = aFileName.lastIndexOf(".");
    if (dotpt)
      return aFileName.substr(dotpt+1).toLowerCase();

    return null;
  }
};

