/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

