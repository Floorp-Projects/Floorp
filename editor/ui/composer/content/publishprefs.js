/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   cmanske@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


/****************** Get publishing data methods *******************/

// Build an array of all publish site data obtained from prefs
function GetPublishSiteData()
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return null;

  // Array of site names - sorted, but don't put default name first
  var siteNameList = GetSiteNameList(true, false);
  if (!siteNameList)
    return null;

  // Array of all site data
  var siteArray = [];

  // We  rewrite siteName prefs to eliminate names if data is bad
  //  and to be sure order is the same as sorted name list
  try {
    publishBranch.deleteBranch("site_name.");
  } catch (e) {}

  // Get publish data using siteName as the key
  var index = 0;
  for (var i = 0; i < siteNameList.length; i++)
  {
    // Associated data uses site name as key
    var publishData = GetPublishData_internal(publishBranch, siteNameList[i]);
    if (publishData)
    {
      siteArray[index] = publishData;
      SetPublishStringPref(publishBranch, "site_name."+index, siteNameList[i]);
      index++;
    }
    else
    {
      try {
        // Remove bad site prefs now
        publishBranch.deleteBranch("site_data." + siteNameList[i] + ".");
      } catch (e) {}
    }
  }

  SavePrefFile();

  if (index == 0) // No Valid pref records found!
    return null;


  return siteArray;
}

function GetDefaultPublishSiteName()
{
  var publishBranch = GetPublishPrefsBranch();
  var name = "";  
  if (publishBranch)
    name = GetPublishStringPref(publishBranch, "default_site");

  return name;
}

// Return object with all info needed to publish
//   from database of sites previously published to.
function CreatePublishDataFromUrl(docUrl)
{
  if (!docUrl || IsUrlAboutBlank(docUrl) || GetScheme(docUrl) == "file")
    return null;

  var pubSiteData = GetPublishSiteData();
  if (pubSiteData)
  {
    var dirObj = {};
    var index = FindSiteIndexAndDocDir(pubSiteData, docUrl, dirObj);
    var publishData;
    if (index != -1)
    {
      publishData = pubSiteData[index];
      publishData.docDir = FormatDirForPublishing(dirObj.value)

      //XXX Problem: OtherDir: How do we decide when to use the dir in 
      //    publishSiteData (default DocDir) or docDir from current filepath?
      publishData.otherDir = FormatDirForPublishing(pubSiteData[index].otherDir);

      publishData.filename = GetFilename(docUrl);
      publishData.notInSiteData = false;
      return publishData;
    }
  }

  // Document wasn't found in publish site database
  // Create data just from URL

  // Extract username and password from docUrl
  var userObj = {};
  var passObj = {};
  var pubUrl = StripUsernamePassword(docUrl, userObj, passObj);

  // Strip off filename
  var lastSlash = pubUrl.lastIndexOf("\/");
  //XXX Look for "?", "=", and "&" ?
  pubUrl = pubUrl.slice(0, lastSlash+1);

  var siteName = CreateSiteNameFromUrl(pubUrl, pubSiteData);

  publishData = { 
    siteName : siteName,
    previousSiteName : siteName,
    filename : GetFilename(docUrl),
    username : userObj.value,
    password : passObj.value,
    savePassword : false,
    publishUrl : pubUrl,
    browseUrl  : pubUrl,
    docDir     : "",
    otherDir   : "",
    publishOtherFiles : true,
    dirList    : [""],
    saveDirs   : false,
    notInSiteData : true
  }

  return publishData;
}

function CreateSiteNameFromUrl(url, publishSiteData)
{
  var host = GetHost(url);
  var schemePostfix = " (" + GetScheme(url) + ")";
  var siteName = host + schemePostfix;

  if (publishSiteData)
  {
    // Look for duplicates. Append "-1"  etc until unique name found
    var i = 1;
    var exists = false;
    do {
      exists = PublishSiteNameExists(siteName, publishSiteData, -1)  
      if (exists)
        siteName = host + "-" + i + schemePostfix;
      i++;
    }
    while (exists);
  }
  return siteName;
}

// Similar to above, but in param is a site profile name
// Note that this is more efficient than getting from a URL,
//   since we don't have to get all the sitedata but can key off of sitename.
// Caller must supply the current docUrl or just a filename
// If doc URL is supplied, we find the publish subdirectory if publishUrl is part of docUrl
function GetPublishDataFromSiteName(siteName, docUrlOrFilename)
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return null;

  var siteNameList = GetSiteNameList(false, false);
  if (!siteNameList)
    return null;
  for (var i = 0; i < siteNameList.length; i++)
  {
    if (siteNameList[i] == siteName)
    {
      var publishData = GetPublishData_internal(publishBranch, siteName);
      if (GetScheme(docUrlOrFilename))
        FillInMatchingPublishData(publishData, docUrlOrFilename);
      else
        publishData.filename = docUrlOrFilename;

      return publishData;
    }
  }
  return null;
}

function GetDefaultPublishData()
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return null;

  var siteName = GetPublishStringPref(publishBranch, "default_site");
  if (!siteName)
    return null;

  return GetPublishData_internal(publishBranch, siteName);
}

function GetPublishData_internal(publishBranch, siteName)
{
  if (!publishBranch || !siteName)
    return null;

  var prefPrefix = "site_data." + siteName + ".";

  // We must have a publish url, else we ignore this site
  // (siteData and siteNames for sites with incomplete data 
  //  will get deleted by SavePublishSiteDataToPrefs)
  var publishUrl = GetPublishStringPref(publishBranch, prefPrefix+"url");
  if (!publishUrl)
    return null;

  var savePassword = false;
  var publishOtherFiles = true;
  try {
    savePassword = publishBranch.getBoolPref(prefPrefix+"save_password");
    publishOtherFiles = publishBranch.getBoolPref(prefPrefix+"publish_other_files");
  } catch (e) {}

  var publishData = { 
    siteName : siteName,
    previousSiteName : siteName,
    filename : "",
    username : GetPublishStringPref(publishBranch, prefPrefix+"username"),
    savePassword : savePassword,
    publishUrl : publishUrl,
    browseUrl  : GetPublishStringPref(publishBranch, prefPrefix+"browse_url"),
    docDir     : FormatDirForPublishing(GetPublishStringPref(publishBranch, prefPrefix+"doc_dir")),
    otherDir   : FormatDirForPublishing(GetPublishStringPref(publishBranch, prefPrefix+"other_dir")),
    publishOtherFiles : publishOtherFiles,
    saveDirs : false
  }

  // Get password from PasswordManager
  publishData.password = GetSavedPassword(publishData);

  // If password was found, user must have checked "Save password"
  //    checkbox in prompt outside of publishing, so override the pref we stored
  if (publishData.password)
  {
    if (!savePassword)
    {
      try {
        publishPrefsBranch.setBoolPref(prefPrefix+"save_password", true);
      } catch (e) {}
    }
    publishData.savePassword = true;
  }

  // Build history list of directories 
  // Always supply the root dir 
  publishData.dirList = [""];

  // Get the rest from prefs
  var dirCount = {value:0};
  var dirPrefs;
  try {
    dirPrefs = publishBranch.getChildList(prefPrefix+"dir.", dirCount);
  } catch (e) {}

  if (dirPrefs && dirCount.value > 0)
  {
    if (dirCount.value > 1)
      dirPrefs.sort();

    for (var j = 0; j < dirCount.value; j++)
    {
      var dirName = GetPublishStringPref(publishBranch, dirPrefs[j]);
      if (dirName)
        publishData.dirList[j+1] = dirName;
    }
  }

  return publishData;
}

/****************** Save publishing data methods *********************/

// Save the siteArray containing all current publish site data
function SavePublishSiteDataToPrefs(siteArray, defaultName)
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return false;

  try {
    if (siteArray)
    {
      var defaultFound = false;

      // Clear existing names and data -- rebuild all site prefs
      publishBranch.deleteBranch("site_name.");
      publishBranch.deleteBranch("site_data.");

      for (var i = 0; i < siteArray.length; i++)
      {
        SavePublishData_Internal(publishBranch, siteArray[i], i);
        if (!defaultFound)
          defaultFound = defaultName == siteArray[i].siteName;
      }
      // Assure that we have a default name
      if (siteArray.length && !defaultFound)
        defaultName = siteArray[0].siteName;
    }

    // Save default site name
    SetPublishStringPref(publishBranch, "default_site", defaultName);
  
    // Force saving to file so next page edited finds these values
    SavePrefFile();
  }
  catch (ex) { return false; }

  return true;
}

// Update prefs if publish site already exists
//  or add prefs for a new site
function SavePublishDataToPrefs(publishData)
{
  if (!publishData || !publishData.publishUrl)
    return false;

  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return false;

  // Create name from URL if no site name is provided
  if (!publishData.siteName)
    publishData.siteName = CreateSiteNameFromUrl(publishData.publishUrl, publishData);

  var siteCount = {value:0};
  var siteNamePrefs;
  try {
    siteNamePrefs = publishBranch.getChildList("site_name.", siteCount);
  } catch (e) {}

  if (!siteNamePrefs || siteCount.value == 0)
  {
    // We currently have no site prefs, so create them
    var siteData = [publishData];
    return SavePublishSiteDataToPrefs(siteData, publishData.siteName);
  }

  // Use "previous" name if available in case it was changed
  var previousSiteName =  ("previousSiteName" in publishData && publishData.previousSiteName) ? 
                            publishData.previousSiteName : publishData.siteName;

  // Find site number of existing site or fall through at next available one
  // (Number is arbitrary; needed to construct unique "site_name.x" pref string)
  for (var i = 0; i < siteCount.value; i++)
  {
    var siteName = GetPublishStringPref(publishBranch, "site_name."+i);

    if (siteName == previousSiteName)
    {
      // Delete prefs for an existing site
      try {
        publishBranch.deleteBranch("site_data." + siteName + ".");
      } catch (e) {}
      break;
    }
  }

  // We've taken care of finding old duplicate, so be sure 'previous name' is current
  publishData.previousSiteName = publishData.siteName;

  var ret = SavePublishData_Internal(publishBranch, publishData, i);
  if (ret)
  {
    // Check if siteName was the default and we need to update that
    var defaultSiteName = GetPublishStringPref(publishBranch, "default_site");
    if (previousSiteName == defaultSiteName 
        && publishData.siteName != defaultSiteName)
      SetPublishStringPref(publishBranch, "default_site", publishData.siteName);

    SavePrefFile();

    // Clear signal to save these data
    if ("notInSiteData" in publishData && publishData.notInSiteData)
      publishData.notInSiteData = false;
  }
  return ret;
}

// Save data at a particular site number
function SavePublishData_Internal(publishPrefsBranch, publishData, siteIndex)
{
  if (!publishPrefsBranch || !publishData)
    return false;

  SetPublishStringPref(publishPrefsBranch, "site_name."+siteIndex, publishData.siteName);

  FixupUsernamePasswordInPublishData(publishData);

  var prefPrefix = "site_data." + publishData.siteName + "."

  SetPublishStringPref(publishPrefsBranch, prefPrefix+"url", publishData.publishUrl);
  SetPublishStringPref(publishPrefsBranch, prefPrefix+"browse_url", publishData.browseUrl);
  SetPublishStringPref(publishPrefsBranch, prefPrefix+"username", publishData.username);
  
  try {
    publishPrefsBranch.setBoolPref(prefPrefix+"save_password", publishData.savePassword);
    publishPrefsBranch.setBoolPref(prefPrefix+"publish_other_files", publishData.publishOtherFiles);
  } catch (e) {}

  // Save password using PasswordManager 
  // (If publishData.savePassword = false, this clears existing password)
  SavePassword(publishData);

  SetPublishStringPref(publishPrefsBranch, prefPrefix+"doc_dir", 
                       FormatDirForPublishing(publishData.docDir));

  if (publishData.publishOtherFiles && publishData.otherDir)
    SetPublishStringPref(publishPrefsBranch, prefPrefix+"other_dir",
                         FormatDirForPublishing(publishData.otherDir));

  if ("saveDirs" in publishData && publishData.saveDirs)
  {
    if (publishData.docDir)
      AppendNewDirToList(publishData, publishData.docDir);

    if (publishData.publishOtherFiles && publishData.otherDir 
        && publishData.otherDir != publishData.docDir)
      AppendNewDirToList(publishData, publishData.otherDir);
  }

  // Save array of subdirectories with site
  if (publishData.dirList.length)
  {
    publishData.dirList.sort();
    var dirIndex = 0;
    for (var j = 0; j < publishData.dirList.length; j++)
    {
      var dir = publishData.dirList[j];

      // Don't store the root dir
      if (dir && dir != "/")
      {
        SetPublishStringPref(publishPrefsBranch, prefPrefix + "dir." + dirIndex, dir);
        dirIndex++;
      }
    }
  }

  return true;
}

function AppendNewDirToList(publishData, newDir)
{
  newDir = FormatDirForPublishing(newDir);
  if (!publishData || !newDir)
    return;

  if (!publishData.dirList)
  {
    publishData.dirList = [newDir];
    return;
  }

  // Check if already in the list
  for (var i = 0; i < publishData.dirList.length; i++)
  {
    // Don't add if already in the list
    if (newDir == publishData.dirList[i])
      return;
  }
  // Add to end of list
  publishData.dirList[publishData.dirList.length] = newDir;
}

function RemovePublishSubdirectoryFromPrefs(publishData, removeDir)
{
  removeDir = FormatDirForPublishing(removeDir);
  if (!publishData || !publishData.siteName || !removeDir)
    return false;

  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return false;

  var prefPrefix = "site_data." + publishData.siteName + ".";

  // Remove dir from the default dir prefs
  if (publishData.docDir == removeDir)
  {
    publishData.docDir = "";
    SetPublishStringPref(publishBranch, prefPrefix+"doc_dir", "");
  }

  if (publishData.otherDir == removeDir)
  {
    publishData.otherDir = "";
    SetPublishStringPref(publishBranch, prefPrefix+"other_dir", "");
  }

  prefPrefix += "dir.";

  // Delete entire subdir list
  try {
    publishBranch.deleteBranch(prefPrefix);
  } catch (e) {}

  // Rebuild prefs, skipping over site to remove
  if (publishData.dirList.length)
  {
    var dirIndex = 0;
    var docDirInList = false;
    var otherDirInList = false;
    for (var i = 0; i < publishData.dirList.length; i++)
    {
      var dir = publishData.dirList[i];
      if (dir == removeDir)
      {
        // Remove item from the dirList array
        publishData.dirList.splice(i, 1);
        --i;
      }
      else if (dir && dir != "/") // skip empty or root dir
      {
        // Save to prefs
        SetPublishStringPref(publishBranch, prefPrefix + dirIndex, dir);
        dirIndex++;
      }
    }
  }
  SavePrefFile();
  return true;
}

function SetDefaultSiteName(name)
{
  if (name)
  {
    var publishBranch = GetPublishPrefsBranch();
    if (publishBranch)
      SetPublishStringPref(publishBranch, "default_site", name);

    SavePrefFile();
  }
}

function SavePrefFile()
{
  try {
    if (gPrefsService)
      gPrefsService.savePrefFile(null);
  }
  catch (e) {}
}

/***************** Helper / utility methods ********************/

function GetPublishPrefsBranch()
{
  var prefsService = GetPrefsService();
  if (!prefsService)
    return null;

  return prefsService.getBranch("editor.publish.");
}

function GetSiteNameList(doSort, defaultFirst)
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return null;

  var siteCountObj = {value:0};
  var siteNamePrefs;
  try {
    siteNamePrefs = publishBranch.getChildList("site_name.", siteCountObj);
  } catch (e) {}

  if (!siteNamePrefs || siteCountObj.value == 0)
    return null;

  // Array of site names
  var siteNameList = [];
  var index = 0;
  var defaultName = "";
  if (defaultFirst)
  {
    defaultName = GetPublishStringPref(publishBranch, "default_site");
    // This always sorts to top -- replace with real string below
    siteNameList[0] = "";
    index++;
  }

  for (var i = 0; i < siteCountObj.value; i++)
  {
    var siteName = GetPublishStringPref(publishBranch, siteNamePrefs[i]);
    // Skip if siteName pref is empty or is default name
    if (siteName && siteName != defaultName)
    {
      siteNameList[index] = siteName;
      index++;
    }
  }

  if (siteNameList.length && doSort)
    siteNameList.sort();

  if (defaultName)
  {
    siteNameList[0] = defaultName;
    index++;
  }

  return siteNameList.length? siteNameList : null;
}

function PublishSiteNameExists(name, publishSiteData, skipSiteIndex)
{
  if (!name)
    return false;

  if (!publishSiteData)
  {
    publishSiteData = GetPublishSiteData();
    skipSiteIndex = -1;
  }

  if (!publishSiteData)
    return false;

  // Array of site names - sorted, but don't put default name first
  for (var i = 0; i < publishSiteData.length; i++)
  {
    if (i != skipSiteIndex && name == publishSiteData[i].siteName)
      return true;
  }
  return false;
}

// Find index of a site record in supplied publish site database
// docUrl: Document URL with or without filename
//         (Must end in "/" if no filename)
// dirObj.value =  the directory of the document URL 
//      relative to the base publishing URL, using "" if none
//
// XXX: Currently finds the site with the longest-matching url;
//      should we look for the shortest instead? Or match just the host portion?
function FindSiteIndexAndDocDir(publishSiteData, docUrl, dirObj)
{
  if (dirObj)
    dirObj.value = "";

  if (!publishSiteData || !docUrl || GetScheme(docUrl) == "file")
    return -1;

  var siteIndex = -1;
  var siteUrlLen = 0;
  
  for (var i = 0; i < publishSiteData.length; i++)
  {
    // Site publish or browse url needs to be contained in document URL,
    //  but that may also have a directory after the site base URL
    //  So we must examine all records to find the site URL that best
    //    matches the document URL: the longest-matching substring (XXX is this right?)
    var lenObj = {value:0};
    var tempData = Clone(publishSiteData[i]);

    // Check if this site matches docUrl (returns length of match if found)
    var len = FillInMatchingPublishData(tempData, docUrl);

    if (len > siteUrlLen)
    {
      siteIndex = i;
      siteUrlLen = len;
      if (dirObj)
        dirObj.value = tempData.docDir;

      // Continue to find the site with longest-matching publishUrl
    }
  }
  return siteIndex;
}

// Look for a matching publish url within the document url
// (We need to look at both "publishUrl" and "browseUrl" in case we are editing
//  an http: document but using ftp: to publish.)
// If match is found:
//    Fill in the filename and subirectory based on the docUrl and 
//    return the length of the docUrl with username+password stripped out
function FillInMatchingPublishData(publishData, docUrl)
{
  if (!publishData || !docUrl)
    return 0;

  // Separate docUrl into the base url and filename
  var lastSlash = docUrl.lastIndexOf("\/");
  var baseUrl = docUrl.slice(0, lastSlash+1);
  var filename = docUrl.slice(lastSlash+1);
    
  // Strip username+password from docUrl because these
  //  are stored separately in publishData, never embedded in the publishUrl
  // If both docUrl and publishData contain usernames,
  //   we must match that as well as the url
  var username = {value:""};
  baseUrl = StripUsernamePassword(baseUrl, username); 
  username = username.value;

  var matchedLength = 0;
  var pubUrlFound = publishData.publishUrl ?
                      baseUrl.indexOf(publishData.publishUrl) == 0 : false;
  var browseUrlFound = publishData.browseUrl ?
                          baseUrl.indexOf(publishData.browseUrl) == 0 : false;

  if ((pubUrlFound || browseUrlFound) 
      && (!username || !publishData.username || username == publishData.username))
  {
    // We found a match
    matchedLength = pubUrlFound ? publishData.publishUrl.length 
                            : publishData.browseUrl.length;

    if (matchedLength > 0)
    {
      publishData.filename = filename;

      // Subdirectory within the site is what's left in baseUrl after the matched portion
      publishData.docDir = FormatDirForPublishing(baseUrl.slice(matchedLength));
    }
  }
  return matchedLength;
}

// Prefs that don't exist will through an exception,
//  so just return an empty string
function GetPublishStringPref(prefBranch, name)
{
  if (prefBranch && name)
  {
    try {
      return prefBranch.getComplexValue(name, Components.interfaces.nsISupportsString).data;
    } catch (e) {}
  }
  return "";
}

function SetPublishStringPref(prefBranch, name, value)
{
  if (prefBranch && name)
  {
    try {
        var str = Components.classes["@mozilla.org/supports-string;1"]
                            .createInstance(Components.interfaces.nsISupportsString);
        str.data = value;
        prefBranch.setComplexValue(name, Components.interfaces.nsISupportsString, str);
    } catch (e) {}
  }
}

// Assure that a publishing URL ends in "/", "=", "&" or "?"
// Username and password should always be extracted as separate fields
//  and are not allowed to remain embedded in publishing URL
function FormatUrlForPublishing(url)
{
  url = TrimString(StripUsernamePassword(url));
  if (url)
  {
    var lastChar = url.charAt(url.length-1);
    if (lastChar != "/" && lastChar != "=" && lastChar != "&" && lastChar  != "?")
      return (url + "/");
  }
  return url;
}

// Username and password present in publish url are
//  extracted into the separate "username" and "password" fields 
//  of the publishData object
// Returns true if we did change the publishData
function FixupUsernamePasswordInPublishData(publishData)
{
  var ret = false;
  if (publishData && publishData.publishUrl)
  {
    var userObj = {value:""};
    var passObj = {value:""};
    publishData.publishUrl = FormatUrlForPublishing(StripUsernamePassword(publishData.publishUrl, userObj, passObj));
    if (userObj.value)
    {
      publishData.username = userObj.value;
      ret = true;
    }
    if (passObj.value)
    {
      publishData.password = passObj.value;
      ret = true;
    }
    // While we're at it, be sure browse URL is proper format
    publishData.browseUrl = FormatUrlForPublishing(publishData.browseUrl);
  }
  return ret;
}

// Assure that a publishing directory ends with "/" and does not begin with "/"
// Input dir is assumed to be a subdirectory string, not a full URL or pathname
function FormatDirForPublishing(dir)
{
  dir = TrimString(dir);

  // The "//" case is an expected "typo" filter
  //  that simplifies code below!
  if (!dir || dir == "/" || dir == "//")
    return "";

  // Remove leading "/"
  if (dir.charAt(0) == "/")
    dir = dir.slice(1);

  // Append "/" at the end if necessary
  var dirLen = dir.length;
  var lastChar = dir.charAt(dirLen-1);
  if (dirLen > 1 && lastChar != "/" && lastChar != "=" && lastChar != "&" && lastChar  != "?")
    return (dir + "/");

  return dir;
}


var gPasswordManager;
function GetPasswordManager()
{
  if (!gPasswordManager)
  {
    var passwordManager = Components.classes["@mozilla.org/passwordmanager;1"].createInstance();
    if (passwordManager)
      gPasswordManager = passwordManager.QueryInterface(Components.interfaces.nsIPasswordManager);
  }
  return gPasswordManager;
}

var gPasswordManagerInternal;
function GetPasswordManagerInternal()
{
  if (!gPasswordManagerInternal)
  {
    try {
      gPasswordManagerInternal =
        Components.classes["@mozilla.org/passwordmanager;1"].createInstance(
          Components.interfaces.nsIPasswordManagerInternal);
    } catch (e) {
    }
  }
  return gPasswordManagerInternal;
}

function GetSavedPassword(publishData)
{
  if (!publishData)
    return "";
  var passwordManagerInternal = GetPasswordManagerInternal();
  if (!passwordManagerInternal)
    return "";

  var host = {value:""};
  var user =  {value:""};
  var password = {value:""}; 
  var url = GetUrlForPasswordManager(publishData);
  
  try {
    passwordManagerInternal.findPasswordEntry
      (url, publishData.username, "", host, user, password);
    return password.value;
  } catch (e) {}

  return "";
}

function SavePassword(publishData)
{
  if (!publishData || !publishData.publishUrl || !publishData.username)
    return false;

  var passwordManager = GetPasswordManager();
  if (passwordManager)
  {
    var url = GetUrlForPasswordManager(publishData);

    // Remove existing entry
    // (Note: there is no method to update a password for an existing entry)
    try {
      passwordManager.removeUser(url, publishData.username);
    } catch (e) {}

    // If SavePassword is true, add new password
    if (publishData.savePassword)
    {
      try {
        passwordManager.addUser(url, publishData.username, publishData.password);
      } catch (e) {}
    }
    return true;
  }
  return false;
}

function GetUrlForPasswordManager(publishData)
{
  if (!publishData || !publishData.publishUrl)
    return false;

  var url;

  // For FTP, we must embed the username into the url for a site address
  // XXX Maybe we should we do this for HTTP as well???
  if (publishData.username && GetScheme(publishData.publishUrl) == "ftp")
    url = InsertUsernameIntoUrl(publishData.publishUrl, publishData.username);
  else
    url = publishData.publishUrl;

  // Strip off terminal "/"
  var len = url.length;
  if (len && url.charAt(len-1) == "\/")
    url = url.slice(0, len-1);
  
  return url;
}
