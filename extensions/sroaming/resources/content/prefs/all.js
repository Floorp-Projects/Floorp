/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* The backend needs the settings in the Mozilla app registry
   (via nsIRegistry), not in the prefs system (nsIPref;
   it is not yet running when needed). This file implements
   the saving of the settings (from the dialog) to the registry and reading
   it back.

   Oh Shit. When the user switches panes, the dialog loses all state. Nothing
   restores it. But we can't save it to the registry either, the user might
   cancel later. So, we have to store all values in our own, internal data
   structure and later save it to the registry, if the user clicked OK.

   To unify matters, I share a data structure between top.xul and files.js.
*/

// for pane switches
function Unload()
{
  UIToData();
  //parent.roaming.verifyData();
}

/* *sigh*!!! when the user clicks OK in another pane,
   all the other functions and even the consts are gone, 
   so creating an object. */

function RoamingPrefs()
{
  // init
  this.loaded = false; // already read from registry
  this.changed = false; // any of the values changed, so save all
  // user data
  this.Enabled = false;
  this.Method = 0; // int, reall enum: 0=stream, 1=copy
  this.Files = new Array(); // of strings, will get another default below
  this.Stream = new Object();
  this.Stream.BaseURL = "";
  this.Stream.Username = "";
  this.Stream.SavePW = false;
  this.Stream.Password = "";
  this.Copy = new Object();
  this.Copy.RemoteDir = "";
  // caching(have to add that here or it will disappear during pane switches)
  this.regkeyProf = undefined;
  this.registry = undefined;

  this.registryToData();
}
RoamingPrefs.prototype =
{
  //from mozSRoaming*.cpp
  kRegTreeProfile :  "Profiles",
  kRegTreeRoaming :  "Roaming",
  kRegKeyEnabled :  "Enabled",
  kRegKeyMethod :  "Method",
  kRegKeyFiles :  "Files",
  kRegValMethodStream :  "stream",
  kRegValMethodCopy :  "copy",
  kRegTreeStream :  "Stream",
  kRegKeyStreamURL :  "BaseURL",
  kRegKeyStreamUsername :  "Username",
  kRegKeyStreamPassword :  "Password",
  kRegKeyStreamSavePW :  "SavePassword",
  kRegTreeCopy :  "Copy",
  kRegKeyCopyDir :  "RemoteDir",
  // Registry layout:
  // (root) (tree)
  // + Profile (tree)
  //   + (current profile) (tree)
  //     + Roaming (tree)
  //       + Enabled (int)
  //       + Method (string, really enum: stream or copy)
  //       + Files (string, comma-separated list)
  //       + Stream (tree)
  //       | + BaseURL (string)
  //       | + Username (string)
  //       | + SavePW (int, really bool)
  //       | + Password (string, encrypted?)
  //       + Copy (tree)
  //         + RemoteDir (string)

  okClicked : function()
  {
    if ("UIToData" in window) // unless a different pane is selected
    {
      UIToData();
    }
    this.verifyData();
    this.dataToRegistry();
  },

  // Registry

  // Read from the Mozilla registry into the internal data structure
  registryToData : function()
  {
    if (this.loaded == true)
      // prevent overwriting of user changes after pane switch back/forth
      return;

    // set default values, matching the hardcoded ones, in case we fail below
    // internal logic

    try
    {
      /* Default files
         In case roaming was never set up, we will fail below at reg reading
         and use the defaults in this. Set the default files list to some
         sensible values, which we will get from the default prefs. */
      var pref = Components.classes["@mozilla.org/preferences;1"]
                 .getService(Components.interfaces.nsIPrefBranch);
      var value = pref.getCharPref("roaming.default.files");
      this.Files = value.split(",");
    } catch (e) {}

    try
    {
      /* to understand the structure of the registry,
         see comment at the const defs above */
      // get the Roaming reg branch
      var registry = Components.classes["@mozilla.org/registry;1"]
                     .createInstance(Components.interfaces.nsIRegistry);
      registry.openWellKnownRegistry(registry.ApplicationRegistry);
      this.registry = registry;
      var profMan = Components.classes["@mozilla.org/profile/manager;1"]
		              .getService(Components.interfaces.nsIProfile);
      var regkey = registry.getKey(registry.Common, this.kRegTreeProfile);
      regkey = registry.getKey(regkey, profMan.currentProfile);
      this.regkeyProf = regkey;
      regkey = registry.getKey(regkey, this.kRegTreeRoaming);

      // enabled
      value = registry.getInt(regkey, this.kRegKeyEnabled);
      this.Enabled = value == 1 ? true : false;

      // method (in the sense of the roaming implementation) selection
      value = registry.getString(regkey, this.kRegKeyMethod);
      if (value == this.kRegValMethodStream)
        this.Method = 0;
      else if (value == this.kRegValMethodCopy)
        this.Method = 1;

      // files
      value = registry.getString(regkey, this.kRegKeyFiles);
      this.Files = value.split(",");
      // remove empty entries
      for (var i = 0, l = this.Files.length; i < l; i++)
      {
        if (this.Files[i] == "")
          this.Files.splice(i, 1);
      }

      // stream
      var regkeyRoaming = regkey; // save for use in "copy"
      regkey = registry.getKey(regkeyRoaming, this.kRegTreeStream);
      this.Stream.BaseURL = registry.getString(regkey, this.kRegKeyStreamURL);
      this.Stream.Username = registry.getString(regkey,
                                                this.kRegKeyStreamUsername);
      this.Stream.Password = registry.getString(regkey,
                                                this.kRegKeyStreamPassword);
      value = registry.getInt(regkey, this.kRegKeyStreamSavePW);
      this.Stream.SavePW = value == 1 ? true : false;

      // copy
      regkey = registry.getKey(regkeyRoaming, this.kRegTreeCopy);
      this.Copy.RemoteDir = registry.getString(regkey, this.kRegKeyCopyDir);

      // Note that read and write all values. We want to remember them,
      // even if not used at the moment.
    }
    catch (e)
    {
      if (!this.regkeyProf)
        this.showError("ErrorRegRead", undefined, e.message);
      // later errors are expected, if we never wrote roaming prefs before,
      // fatal otherwise, but we don't know the difference.
      else
        dump("Error, might be expected (roaming not set up yet): " + e + "\n");
    }
    this.loaded = true;
        /* we might have failed above, but in this case, there is probably no
           sense trying it again, so treat that the same as loaded.
           This is important for the first setup - the first read will fail. */
  },

  // Saves from the internal data structure into the Mozilla registry
  dataToRegistry : function()
  {
    if (!this.changed)
      return;

    var registry = this.registry;
    if (!registry || !this.regkeyProf)
      // arg, we had a fatal error during read, so bail out
      return;
    try
    {
      var regkey = this.saveRegBranch(this.regkeyProf, this.kRegTreeRoaming);

      // enabled
      registry.setInt(regkey, this.kRegKeyEnabled,
                      this.Enabled == true ? 1 : 0);

      // method (in the sense of the roaming implementation) selection
      var value;
      if (this.Method == 0)
        value = this.kRegValMethodStream;
      else if (this.Method == 1)
        value = this.kRegValMethodCopy;
      else // huh??
        value = this.kRegValMethodStream;
      registry.setString(regkey, this.kRegKeyMethod, value);

      // files
      value = "";
      for (var i = 0, l = this.Files.length; i < l; i++)
      {
        var file = this.Files[i];
        if (file && file != "")
          value += file + ",";
      }
      // remove last ","
      if (value.length > 0)
        value = value.substring(0, value.length - 1);
      registry.setString(regkey, this.kRegKeyFiles, value);

      // stream
      var regkeyRoaming = regkey; // save for use in "copy"
      regkey = this.saveRegBranch(regkeyRoaming, this.kRegTreeStream);
      registry.setString(regkey, this.kRegKeyStreamURL, this.Stream.BaseURL);
      registry.setString(regkey, this.kRegKeyStreamUsername,
                         this.Stream.Username);
      registry.setString(regkey, this.kRegKeyStreamPassword,
                    this.Stream.SavePW == true ? this.Stream.Password : "");
      registry.setInt(regkey, this.kRegKeyStreamSavePW,
                      this.Stream.SavePW == true ? 1 : 0);

      // copy
      regkey = this.saveRegBranch(regkeyRoaming, this.kRegTreeCopy);
      registry.setString(regkey, this.kRegKeyCopyDir, this.Copy.RemoteDir);

      this.changed = false;
    }
    catch (e)
    {
      this.showError("ErrorRegWrite", undefined, e.message);
      return;
    }
  },

  // gets or creates registry branch, i.e. drop-in replacement for
  // getKey/addKey returns either the regkey for branch or nothing, if failed
  saveRegBranch : function(baseregkey, branchname)
  {
    try
    {
      return this.registry.getKey(baseregkey, branchname);
    }
    catch (e) // XXX catch selectively
    {
      dumpError("got (expected?) exception " + e + "\n");
      return this.registry.addKey(baseregkey, branchname);
      // if that fails with an exception, throw it to caller
    }
    // throw unexpected errors to caller
  },


  // Logic

  // if some (user-entered) value is bad, shows error dialog and returns false
  verifyData : function()
  {
    if (!this.Enabled)
      return true;

    var errorProp; // see showError();
    var errorVal;  // dito
    var errorException;
    if (this.Files.length <= 0)
    {
      errorProp = "NoFilesSelected";
    }
    if (this.Method == 0) // stream
    {
      // username
      if (!this.Stream.Username || this.Stream.Username == "")
      {
        errorProp = "NoUsername";
      }

      // password
      /* password may be empty despite savepw, we'll then ask for the password
         during the next transfer and store it (by default) then. */

      // base url
      try
      {
        var ioserv = Components.classes["@mozilla.org/network/io-service;1"]
                     .getService(Components.interfaces.nsIIOService);
        var baseURL = ioserv.newURI(this.Stream.BaseURL, null, null);
      }
      catch(e)
      {
        errorProp = "BaseURLMalformed";
        errorVal = this.Stream.BaseURL;
        //errorException = e;
      }
    }
    else if (this.Method == 1) // copy
    {
      // remote dir
      if (this.Copy.RemoteDir == "")
      {
        errorProp = "RemoteDirNonExistant";
        errorVal = " ";
      }
      else
      {
        try
        {
          var lf = Components.classes["@mozilla.org/file/local;1"]
                           .createInstance(Components.interfaces.nsILocalFile);
          lf.initWithPath(this.Copy.RemoteDir);
          if (!lf.exists())
          {
            throw "";
          }
        }
        catch(e)
        {
          errorProp = "RemoteDirNonExistant";
          errorVal = this.Copy.RemoteDir;
          errorException = e.message; // will be undefined, if !lf.exist()
        }
      }
    }

    if (errorProp)
      this.showError(errorProp, errorVal, errorException);

    return errorProp ? true : false;
    // XXX We should prevent closure and force the user to enter valid data,
    // but I don't know how to do it.
  },

  /**
   * Warning: Don't try to show this after pref window disappeared, because
   * we then can't display decendant windows (this error dialog) anymore.
   *
   * @param prop  string  stringbungle string name
   * @param val   string  text to be replaced in string
   * @param tech  string  internal error message (exception etc.). In English.
   */
  showError : function(prop, val, tech)
  {
    try
    {
      var bundle = document.getElementById("bundle_roaming_prefs");
      var dialogTitle = bundle.getString("RoamingErrorTitle");
      var text = dialogTitle + "\n";
      if (val)
        text += bundle.formatStringFromName(prop, [val], 1);
      else
        text += bundle.GetStringFromName(prop);
      if (tech) // should we show exceptions to user?
        text += "\n" + tech;
      GetPromptService().alert(window, dialogTitle, text);
    }
    catch(e)
    {
      dumpError("Error while trying to display an error: " + e
                + " (original error: " + prop + " " + tech + ")\n");
    }
  }
}

// UI

function SwitchDeck(page, deckElement)
{
  deckElement.setAttribute("selectedIndex", page);
}

function EnableTree(enabled, element)
{
  if (!element || !element.setAttribute)
    return;

  if(!enabled)
    element.setAttribute("disabled", "true");
  else
    element.removeAttribute("disabled");

  // EnableTree direct children (recursive)
  var children = element.childNodes;
  for (var i = 0; i < children.length; i++)
    EnableTree(enabled, children.item(i));
}

function E(elementID)
{
  return document.getElementById(elementID);
}

function Browse()
{
  const nsIFilePicker = Components.interfaces.nsIFilePicker;

  var fp = Components.classes["@mozilla.org/filepicker;1"]
           .createInstance(nsIFilePicker);
  var currentFolder = Components.classes["@mozilla.org/file/local;1"]
                      .createInstance(Components.interfaces.nsILocalFile);
  var currentFolderTextbox = document.getElementById("copyDir");
  
  try {
    currentFolder.initWithPath(currentFolderTextbox.value);
    fp.displayDirectory = currentFolder;
  }
  catch (ex) {
    dump("initWithPath failed. Reason: " + ex);
  }

  fp.init(window, E("browseButton").getAttribute("filepickertitle"),
          nsIFilePicker.modeGetFolder);
  
  if (fp.show() == nsIFilePicker.returnOK)
    // convert the nsILocalFile into a nsIFileSpec 
    currentFolderTextbox.value = fp.file.path;
}
