/***************************************************************
* PrefUtils -------------------------------------------------
*  Utility for easily using the Mozilla preferences system.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

//////////// global variables /////////////////////

//////////// global constants ////////////////////

const nsIPref = Components.interfaces.nsIPref;

////////////////////////////////////////////////////////////////////////////
//// class PrefUtils

var PrefUtils = 
{
  mPrefs: null,
  
  init: function()
  {
    this.mPrefs = XPCU.getService("@mozilla.org/preferences;1", "nsIPref");
  },

  addObserver: function(aDomain, aFunction)
  {
    if (!this.mPrefs) this.init();

    this.mPrefs.addObserver(aDomain, aFunction);
  },

  removeObserver: function(aDomain, aFunction)
  {
    if (!this.mPrefs) this.init();

    this.mPrefs.removeObserver(aDomain, aFunction);
  },

  setPref: function(aName, aValue)
  {
    if (!this.mPrefs) this.init();
    
    var type = this.mPrefs.GetPrefType(aName);
    try {
      if (type == nsIPref.ePrefString) {
        this.mPrefs.SetUnicharPref(aName, aValue);
      } else if (type == nsIPref.ePrefBool) {
        this.mPrefs.SetBoolPref(aName, aValue);
      } else if (type == nsIPref.ePrefInt) {
        this.mPrefs.SetIntPref(aName, aValue);
      }
    } catch(ex) {
      debug("ERROR: Unable to write pref \"" + aName + "\".\n");
    }
  },

  getPref: function(aName)
  {
    if (!this.mPrefs) this.init();

    var type = this.mPrefs.GetPrefType(aName);
    try {
      if (type == nsIPref.ePrefString) {
        return this.mPrefs.CopyUnicharPref(aName);
      } else if (type == nsIPref.ePrefBool) {
        return this.mPrefs.GetBoolPref(aName);
      } else if (type == nsIPref.ePrefInt) {
        return this.mPrefs.GetIntPref(aName);
      }
    } catch(ex) {
      debug("ERROR: Unable to read pref \"" + aName + "\".\n");
    }
    return null;
  }
  
};

