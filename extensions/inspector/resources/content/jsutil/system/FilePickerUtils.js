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

