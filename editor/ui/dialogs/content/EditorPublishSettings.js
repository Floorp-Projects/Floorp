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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 */

var gPublishSiteData;
var gPublishDataChanged = false;
var gDefaultSiteIndex = -1;
var gDefaultSiteName;
var gPreviousDefaultSite;
var gPreviousTitle;
var gSettingsChanged = false;
var gSiteDataChanged = false;

// Dialog initialization code
function Startup()
{
  if (!InitEditorShell()) return;

  gDialog.SiteList            = document.getElementById("SiteList");
  gDialog.SiteNameInput       = document.getElementById("SiteNameInput");
  gDialog.PublishUrlInput     = document.getElementById("PublishUrlInput");
  gDialog.BrowseUrlInput      = document.getElementById("BrowseUrlInput");
  gDialog.UsernameInput       = document.getElementById("UsernameInput");
  gDialog.OkButton            = document.documentElement.getButton("accept");

  gPublishSiteData = GetPublishSiteData();
  gDefaultSiteName = GetDefaultPublishSiteName();
  gPreviousDefaultSite = gDefaultSiteName;

  InitDialog();

  SetWindowLocation();
}

function InitDialog()
{
  // If there's no current site data, start a new item in the Settings panel
  if (!gPublishSiteData)
  {
    AddNewSite();
  }
  else
  {
    FillSiteList();
    InitSiteSettings(gDefaultSiteIndex);
    SetTextboxFocus(gDialog.SiteNameInput);
  }
}

function FillSiteList()
{
  ClearTreelist(gDialog.SiteList);
  gDefaultSiteIndex = -1;

  // Fill the site lists
  var count = gPublishSiteData.length;
  var i;

  for (i = 0; i < count; i++)
  {
    var name = gPublishSiteData[i].siteName;
    var menuitem = AppendStringToTreelist(gDialog.SiteList, name);

    // Add a cell before the text to display a check for default site
    if (menuitem)
    {
      var checkCell = document.createElementNS(XUL_NS, "treecell");
      checkCell.setAttribute("class", "treecell-check");

      // Insert tree cell before the one created by AppendStringToTreelist():
      //   (menuitem=treeitem) -> treerow -> treecell
      menuitem.firstChild.insertBefore(checkCell, menuitem.firstChild.firstChild);      

      // Show checkmark in front of default site
      if (name == gDefaultSiteName)
      {
        gDefaultSiteIndex = i;
dump(" *** Setting checked style on tree item\n");
        checkCell.checked = true;
        menuitem.checked = true;
      }
    }
  }
}

function AddNewSite()
{
  // Save any pending changes locally first
  if (gSettingsChanged && !UpdateSettings())
    return;

  // Initialize Setting widgets to none of the selected sites
  InitSiteSettings(-1);
  SetTextboxFocus(gDialog.SiteNameInput);
}

function RemoveSite()
{
  if (!gPublishSiteData)
    return;

  var count = gPublishSiteData.length;
  var index = gDialog.SiteList.selectedIndex;
  var item;
  if (index >= 0)
    item = gDialog.SiteList.selectedItems[0];

dump(" **** Before remove: count = "+count+"\n");

  // Remove one item from site array
  gPublishSiteData.splice(index, 1);

dump(" **** After remove: count = "+gPublishSiteData.length+"\n");
  count--;

  if (index >= count)
    index--;

  // Remove item from site list
  if (item)
    item.parentNode.removeChild(item);

  InitSiteSettings(index);

  gSiteDataChanged = true;
}

function SetDefault()
{
  if (!gPublishSiteData)
    return;

  var index = gDialog.SiteList.selectedIndex;
  if (index >= 0)
  {
    gDefaultSiteIndex = index;
    gDefaultSiteName = gPublishSiteData[index].siteName;
  }
}

// Recursion prevention: InitSiteSettings() changes selected item
var gIsSelecting = false;

function SelectSiteList()
{
  if (gIsSelecting)
    return;

  gIsSelecting = true;

  // Save any pending changes locally first
  if (gSettingsChanged && !UpdateSettings())
    return;

  InitSiteSettings(gDialog.SiteList.selectedIndex);

  gIsSelecting = false;
}

function InitSiteSettings(selectedSiteIndex)
{
  var savePassord = false;

  var haveData = (gPublishSiteData && selectedSiteIndex != -1);

  gDialog.SiteNameInput.value = haveData ? gPublishSiteData[selectedSiteIndex].siteName : "";
  gDialog.PublishUrlInput.value = haveData ? gPublishSiteData[selectedSiteIndex].publishUrl : "";
  gDialog.BrowseUrlInput.value = haveData ? gPublishSiteData[selectedSiteIndex].browseUrl : "";
  gDialog.UsernameInput.value = haveData ? gPublishSiteData[selectedSiteIndex].username : "";

  gDialog.SiteList.selectedIndex = selectedSiteIndex;

  gSettingsChanged = false;
}

function onInputSettings()
{
  // TODO: Save current data during SelectSite1 and compare here
  //       to detect if real change has occurred?
  gSettingsChanged = true;
}

function UpdateSettings()
{
  // Validate and add new site
  var newName = TrimString(gDialog.SiteNameInput.value);
  if (!newName)
  {
    ShowInputErrorMessage(GetString("MissingSiteNameError"), gDialog.SiteNameInput);
    return false;
  }
  var newUrl = FormatUrlForPublishing(gDialog.PublishUrlInput.value);
  if (!newUrl)
  {
    ShowInputErrorMessage(GetString("MissingPublishUrlError"), gDialog.PublishUrlInput);
    return false;
  }

  var siteIndex = -1;
  if (!gPublishSiteData)
  {
dump(" * Create new gPublishSiteData\n");
    // Create the first site profile
    gPublishSiteData = new Array(1);
    siteIndex = 0;
  }
  else
  {
    // Update existing site profile
    siteIndex = gDialog.SiteList.selectedIndex;
    if (siteIndex == -1)
    {
      // But if none selected, add new data at the end
      siteIndex = gPublishSiteData.length;
    }
  }
dump(" * UpdateSettings: NEW DATA AT index="+siteIndex+", gPublishSiteData.length="+gPublishSiteData.length+"\n");

  gPublishSiteData[siteIndex] = {};
  gPublishSiteData[siteIndex].siteName = newName;
  gPublishSiteData[siteIndex].publishUrl = newUrl;
  gPublishSiteData[siteIndex].browseUrl = FormatUrlForPublishing(gDialog.BrowseUrlInput.value);
  gPublishSiteData[siteIndex].username = TrimString(gDialog.UsernameInput.value);

  if (siteIndex == gDefaultSiteIndex)
    gDefaultSiteName = newName;

dump("  Default SiteName = "+gDefaultSiteName+", Index="+gDefaultSiteIndex+"\n");

dump("New Site Array: data="+gPublishSiteData[siteIndex].siteName+","+gPublishSiteData[siteIndex].publishUrl+","+gPublishSiteData[siteIndex].browseUrl+","+gPublishSiteData[siteIndex].username+"\n");

  var count = gPublishSiteData.length;
  if (count > 1)
  {
    // XXX Ascii sort, not locale-aware
    gPublishSiteData.sort();

    //Find previous items in sorted list
    for (var i = 0; i < count; i++)
    {
  dump(" Name #"+i+" = "+gPublishSiteData[i].siteName+"\n");

      if (gPublishSiteData[i].siteName == newName)
      {
        siteIndex = i;
        break;
      }
    }
  }

  // When adding the very first site, assume that's the default
  if (count == 1 && !gDefaultSiteName)
  {
    gDefaultSiteName = gPublishSiteData[0].siteName;
    gDefaultSiteIndex = 0;
  }

  FillSiteList();

  gDialog.SiteList.selectedIndex = siteIndex;

  gSettingsChanged = false;

  gSiteDataChanged = true;

  return true;
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?publishSettings");
}

function onAccept()
{
  // Save any pending changes locally first
  if (gSettingsChanged && !UpdateSettings())
    return false;

  if (gSiteDataChanged)
  {
    // Save all local data to prefs
    SavePublishSiteDataToPrefs(gPublishSiteData, gDefaultSiteName);
  }
  else if (gPreviousDefaultSite != gDefaultSiteName)
  {
    // only the default site was changed
    SavePublishSiteDataToPrefs(null, gDefaultSiteName);
  }

  SaveWindowLocation();

  return true;
}
