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
* FilePickerUtils -------------------------------------------------
*  A utility for easily dealing with the file picker dialog.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global constants ////////////////////

const kFilePickerCID = "@mozilla.org/filepicker;1";
const kLFileCID = "@mozilla.org/file/local;1";

const nsIFilePicker = Components.interfaces.nsIFilePicker;

////////////////////////////////////////////////////////////////////////////
//// class FilePickerUtils

var FilePickerUtils =
{
  pickFile: function(aTitle, aInitPath, aFilters, aMode)
  {
    try {
      var modeStr = aMode ? "mode" + aMode : "modeOpen";
      var mode;
      try {
        mode = nsIFilePicker[modeStr];
      } catch (ex) { 
        dump("WARNING: Invalid FilePicker mode '"+aMode+"'. Defaulting to 'Open'.\n");
        mode = nsIFilePicker.modeOpen;
      }
      
      var fp = XPCU.createInstance(kFilePickerCID, "nsIFilePicker");
      fp.init(window, aTitle, mode);

      // join array of filter names into bit string
      var filters = this.prepareFilters(aFilters);

      if (aInitPath) {
        var dir = XPCU.createInstance(kLFileCID, "nsILocalFile");
        dir.initWithPath(aInitPath);
        fp.displayDirectory = dir;
      }

      if (fp.show() == nsIFilePicker.returnOK) {
        return fp.file;
      }
    } catch (ex) {
      dump("ERROR: Unable to open file picker.\n" + ex + "\n");
    }
    return null;
  },

  pickDir: function(aTitle, aInitPath)
  {
    try {
      var fp = XPCU.createInstance(kFilePickerCID, "nsIFilePicker");
      fp.init(window, aTitle, nsIFilePicker.modeGetFolder);

      if (aInitPath) {
        var dir = XPCU.createInstance(kLFileCID, "nsILocalFile");
        dir.initWithPath(aInitPath);
        fp.displayDirectory = dir;
      }
      
      if (fp.show() == nsIFilePicker.returnOK) {
        return fp.file;
      }
    } catch (ex) {
      dump("ERROR: Unable to open directory picker.\n" + ex + "\n");
    }

    return null;
  },
  
  prepareFilters: function(aFilters)
  {
    // join array of filter names into bit string
    var filters = 0;
    for (var i = 0; i < aFilters.length; ++i)
      filters = filters | nsIFilePicker[aFilters[i]];
    
    return filters;
  }
   
};

