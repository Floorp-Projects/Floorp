/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Number of items in each site profile array
const gSiteDataLength = 4;
// Index to each profile item:
const gNameIndex = 0;
const gUrlIndex = 1;
const gBrowseIndex = 2;
const gUserNameIndex = 3;
const gDefaultDirIndex = 4;
const gDirListIndex = 5;

function GetPublishPrefsBranch()
{
  var prefsService = GetPrefsService();
  if (!prefsService)
    return null;

  return prefsService.getBranch("editor.publish.");
}

// Build an array of publish site data obtained from prefs
function GetPublishSiteData()
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return null;

  var prefCount = {value:0};
  var nameArray = publishBranch.getChildList("site_name.", prefCount);
dump(" *** GetPublishSiteData: nameArray="+nameArray+", count ="+prefCount.value+"\n");

  if (!nameArray || prefCount.value == 0)
    return null;

  // Array of all site data
  var siteArray = new Array();

  var index = 0;
  var i;

  for (i = 0; i < prefCount.value; i++)
  {
    var name = GetPublishCharPref(publishBranch, nameArray[i]);
    if (name)
    {
      // Associated data uses site name as key
      var prefPrefix = "site_data." + name + ".";

      // We must at least have a publish url, else we ignore this site
      // (siteData and siteNames for sites with incomplete data 
      //  will get deleted by SavePublishSiteDataToPrefs)
      var publishUrl = GetPublishCharPref(publishBranch, prefPrefix+"url");
      if (publishUrl)
      {
        siteArray[index] = new Array(gSiteDataLength);
        siteArray[index][gNameIndex] = name;
        siteArray[index][gUrlIndex] = publishUrl;
        siteArray[index][gBrowseIndex] = GetPublishCharPref(publishBranch, prefPrefix+"browse_url");
        siteArray[index][gUserNameIndex] = GetPublishCharPref(publishBranch, prefPrefix+"username");
        siteArray[index][gDefaultDirIndex] = GetPublishCharPref(publishBranch, prefPrefix+"default_dir");

        // Get the list of directories within each site
        var dirCount = {value:0};
        var dirArray = publishBranch.getChildList(prefPrefix+"dir_name.", dirCount);
        if (dirArray && dirCount.value > 0)
        {
          siteArray[index][gDirListIndex] = new Array();
          for (var j = 0; j < dirCount.value; j++)
          {
            var dirName = GetPublishCharPref(publishBranch, prefPrefix+dirArray[j]);
dump("      DirName="+dirName+"\n");
            if (dirName)
              siteArray[index][gDirListIndex][j] = dirName;
          }
        }
        index++;
      }
dump(" Name="+name+", URL="+publishUrl+"\n");
    }
  }
  if (index == 0) // No Valid pref records found!
    return null;

  if (index > 1)
  {
    // NOTE: This is an ASCII sort (not locale-aware)
    siteArray.sort();
  }
  return siteArray;
}

function SavePublishSiteDataToPrefs(siteArray, defaultName)
{
  var publishBranch = GetPublishPrefsBranch();
dump(" *** Saving Publish Data: publishBranch="+publishBranch+"\n");
  if (!publishBranch)
    return;

  // Clear existing names and data -- rebuild all site prefs
  publishBranch.deleteBranch("site_name.");
  publishBranch.deleteBranch("site_data.");

// *****  DEBUG ONLY
  var prefCount = {value:0};
  var pubArray = publishBranch.getChildList("", prefCount);
dump(" *** editor.publish child array ="+pubArray+", prefCount.value ="+prefCount.value+"\n");

// *****  DEBUG ONLY

  var i;
  if (siteArray)
  {
    for (i = 0; i < siteArray.length; i++)
    {
dump("  Saving prefs for "+siteArray[i][gNameIndex]+"\n");
      var prefPrefix = "site_data." + siteArray[i][gNameIndex] + "."
      publishBranch.setCharPref("site_name."+i, siteArray[i][gNameIndex]);
      publishBranch.setCharPref(prefPrefix+"url", siteArray[i][gUrlIndex]);
      publishBranch.setCharPref(prefPrefix+"browse_url", siteArray[i][gBrowseIndex]);
      publishBranch.setCharPref(prefPrefix+"username", siteArray[i][gUserNameIndex]);
    }
  }

  // Save default site name
  publishBranch.setCharPref("default_site", defaultName);
  
  // Force saving to file so next file opened finds these values
  if (gPrefsService)
    gPrefsService.savePrefFile(null);
}

// This doesn't force saving prefs file
// Call "SavePublishSiteDataToPrefs(null, defaultName)" to force saving file
function SetDefaultSiteName(name)
{
  if (name)
  {
    var publishBranch = GetPublishPrefsBranch();
    if (publishBranch)
      publishBranch.setCharPref("default_site", name);
  }
}

function GetDefaultPublishSiteName()
{
  var publishBranch = GetPublishPrefsBranch();
  var name = "";  
  if (publishBranch)
    try {
      name = GetPublishCharPref(publishBranch, "default_site");
    } catch (ex) {}

  return name;
}

function GetDefaultPublishSiteData()
{
  var publishBranch = GetPublishPrefsBranch();
  if (!publishBranch)
    return null;

  var name = GetPublishCharPref(publishBranch, "default_site");
  if (!name)
    return null;

  var prefPrefix = "site_data." + name + ".";

  var publishData = { 
    SiteName : name,
    UserName : GetPublishCharPref(publishBranch, prefPrefix+"username"),
    Password : "",
    Filename : "",
    DestinationDir : GetPublishCharPref(publishBranch, prefPrefix+"url"),
    BrowseDir : GetPublishCharPref(publishBranch, prefPrefix+"browse_url"),
    RelatedDocs : {}
  }

  return publishData;
}

// Prefs that don't exist will through an exception,
//  so just return empty string for those cases
function GetPublishCharPref(prefBranch, name)
{
  var prefStr = "";
  try {
    prefStr = prefBranch.getCharPref(name);
  } catch (ex) {}

  return prefStr;
}
