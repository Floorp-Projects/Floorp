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

