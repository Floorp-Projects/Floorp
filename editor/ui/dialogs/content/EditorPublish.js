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

var gPublishPanel = 0;
var gSettingsPanel = 1;
var gCurrentPanel = gPublishPanel;
var gPublishSiteData;
var gReturnData;
var gPublishDataChanged = false;
var gDefaultSiteIndex = -1;
var gDefaultSiteName;
var gPreviousDefaultSite;
var gPreviousDefaultDir;
var gPreviousTitle;
var gSettingsChanged = false;

// Dialog initialization code
function Startup()
{
  if (!InitEditorShell()) return;

  // Element to edit is passed in
  if (!window.arguments[1])
  {
    dump("Publish: Return data object not supplied\n");
    window.close();
    return;
  }
  gReturnData = window.arguments[gUrlIndex];

  gDialog.TabPanels           = document.getElementById("TabPanels");
  gDialog.PublishTab          = document.getElementById("PublishTab");
  gDialog.SettingsTab         = document.getElementById("SettingsTab");

  // Publish panel
  gDialog.PageTitleInput      = document.getElementById("PageTitleInput");
  gDialog.FilenameInput       = document.getElementById("FilenameInput");
  gDialog.SiteList            = document.getElementById("SiteList");
  gDialog.DirList             = document.getElementById("DirList");

  gDialog.MoreSection         = document.getElementById("MoreSection");
  gDialog.MoreFewerButton     = document.getElementById("MoreFewerButton");

  // Settings Panel
  gDialog.SettingsPanel       = document.getElementById("SettingsPanel");
  gDialog.ServerSettingsBox   = document.getElementById("ServerSettingsBox");
  gDialog.SiteNameInput       = document.getElementById("SiteNameInput");
  gDialog.PublishUrlInput     = document.getElementById("PublishUrlInput");
  gDialog.BrowseUrlInput      = document.getElementById("BrowseUrlInput");
  gDialog.UserNameInput       = document.getElementById("UserNameInput");

  gDialog.PublishButton       = document.documentElement.getButton("accept");

  // Change 'Ok' button to 'Publish'
  gDialog.PublishButton.setAttribute("label", GetString("Publish"));
  
  gPublishSiteData = GetPublishSiteData();
  gDefaultSiteName = GetDefaultPublishSiteName();
  gPreviousDefaultSite = gDefaultSiteName;

  InitDialog();

  SeeMore = (gDialog.MoreFewerButton.getAttribute("more") != "1");
  onMoreFewerPublish();

  // If there's no current site data, start a new item in the Settings panel
  if (!gPublishSiteData)
    AddNewSite();
  else
    SetTextboxFocus(gDialog.PageTitleInput);
  
  // This sets enable states on buttons
  doEnabling();

  SetWindowLocation();
}

function InitDialog()
{
  gPreviousTitle = editorShell.GetDocumentTitle();

  var selectedSiteIndex = -1;
  if (gPublishSiteData)
    FillSiteList();

  var docUrl = editorShell.editorDocument.location.href;
  var scheme = GetScheme(docUrl);
  var filename = "";
  if (scheme)
  {
    filename = GetFilename(docUrl);

    if (scheme.toLowerCase() != "file:")
    {
      // Editing a remote URL.
      // Attempt to find doc URL in Site Data
      if (gPublishSiteData)
      {
        // Remove filename from docUrl
        var lastSlash = docUrl.lastIndexOf("\/");
        var destinationDir = docUrl.slice(0, lastSlash);
        selectedSiteIndex = FindDestinationUrlInPublishData(destinationDir);
      }
    }
  }

  // We didn't find a site -- use default
  if (selectedSiteIndex == -1)
    gDialog.SiteList.selectedIndex = gDefaultSiteIndex;

  gDialog.PageTitleInput.value = gPreviousTitle;
  gDialog.FilenameInput.value = filename;

  // Settings panel widgets are filled in by InitSiteSettings() when we switch to that panel
}

function FindDestinationUrlInPublishData(url)
{
  if (!url)
    return -1;
  var siteIndex = -1;
  var siteUrlLen = 0;
  var urlLen = url.length;

  if (gPublishSiteData)
  {
    for (var i = 0; i < gPublishSiteData.length; i++)
    {
      // Site url needs to be contained in document URL,
      //  but that may also have a directory after the site base URL
      //  So we must examine all records to find the site URL that best
      //    matches the document URL
      if (url.indexOf(gPublishSiteData[i][gUrlIndex]) == 0)
      {
        var len = gPublishSiteData[i][gUrlIndex].length;
        if (len > siteUrlLen)
        {
          siteIndex = i;
          siteUrlLen = len;
          // We must be done if doc exactly matches the site URL
          if (len = urlLen)
            break;
        }
      }
    }
    if (siteIndex >= 0)
    {
      // Select the site we found
      gDialog.SiteList.selectedIndex = siteIndex;
      
      // Set extra directory name from the end of url
      if (urlLen > siteUrlLen)
        gDialog.DirList.value = url.slice(siteUrlLen);
    }
  }
  return siteIndex;
}

function FillSiteList()
{
  ClearMenulist(gDialog.SiteList);
  gDefaultSiteIndex = -1;

  // Fill the site lists
  var count = gPublishSiteData.length;
  var i;

  for (i = 0; i < count; i++)
  {
    var name = gPublishSiteData[i][gNameIndex];
    var menuitem = AppendStringToMenulist(gDialog.SiteList, name);
    // Show checkmark in front of default site
    if (name == gDefaultSiteName)
    {
      gDefaultSiteIndex = i;
      if (menuitem)
      {
        menuitem.setAttribute("class", "menuitem-highlight-1");
        menuitem.setAttribute("default", "true");
      }
    }
  }
}

function doEnabling()
{

}

function SelectSiteList()
{
dump(" *** SelectSiteList called\n");
  // Fill the Directory list
  if (!gPublishSiteData)
    return;

  var selectedSiteIndex = gDialog.SiteList.selectedIndex;  
  if (selectedSiteIndex == -1)
    return;

  var dirArray = gPublishSiteData[selectedSiteIndex][gDirListIndex];
  for (var i = 0; i < dirArray.length; i++)
    AppendStringToMenulist(gDialog.DirList, dirArray[i]);

  gDialog.DirList.value = gPublishSiteData[selectedSiteIndex][gDefaultDirIndex];
}

function SetDefault()
{
  if (!gPublishSiteData)
    return;

  var index = gDialog.SiteList.selectedIndex;
  if (index >= 0)
  {
    gDefaultSiteIndex = index;
    gDefaultSiteName = gPublishSiteData[index][gNameIndex];
    gSettingsChanged = false;
  }
}

function onMoreFewerPublish()
{
  if (SeeMore)
  {
    gDialog.MoreSection.setAttribute("collapsed","true");
    gDialog.ServerSettingsBox.setAttribute("collapsed","true");
    window.sizeToContent();

    gDialog.MoreFewerButton.setAttribute("more","0");
    gDialog.MoreFewerButton.setAttribute("label",GetString("More"));
    SeeMore = false;
  }
  else
  {
    gDialog.MoreSection.removeAttribute("collapsed");
    gDialog.ServerSettingsBox.removeAttribute("collapsed");
    window.sizeToContent();

    gDialog.MoreFewerButton.setAttribute("more","1");
    gDialog.MoreFewerButton.setAttribute("label",GetString("Less"));
    SeeMore = true;
  }
}

function AddNewSite()
{
  // Button in Publish panel allows user
  //  to automatically switch to "Settings" panel
  //  to enter data for new site
  SwitchPanel(gSettingsPanel);

  // Initialize Setting widgets to none of the selected sites
  InitSiteSettings(-1);
  SetTextboxFocus(gDialog.SiteNameInput);
}

function SelectPublishTab()
{
  if (gSettingsChanged)
  {
    gCurrentPanel = gPublishPanel;
    if (!SaveSettingsData())
      return;

    gCurrentPanel = gSettingsPanel;
  }

  SwitchPanel(gPublishPanel);
  SetTextboxFocus(gDialog.PageTitleInput);
}

function SelectSettingsTab()
{
  // Initialize Setting widgets based on Selectd Site from Publish panel
  var index = gDialog.SiteList.selectedIndex;
  if (index >= 0)
    InitSiteSettings(index);
  
  SwitchPanel(gSettingsPanel);
  SetTextboxFocus(gDialog.SiteNameInput);
}

function InitSiteSettings(selectedSiteIndex)
{
  var siteName = "";
  var publishUrl = "";
  var browseUrl = "";
  var username = "";
  var password = "";
  var savePassord = false;

  if (gPublishSiteData && selectedSiteIndex >= 0)
  {
    siteName = gPublishSiteData[selectedSiteIndex][gNameIndex];
    publishUrl = gPublishSiteData[selectedSiteIndex][gUrlIndex];
    browseUrl = gPublishSiteData[selectedSiteIndex][gBrowseIndex];
    username = gPublishSiteData[selectedSiteIndex][gUserNameIndex];
    // TODO: HOW TO GET PASSWORD???
  }

  gDialog.SiteNameInput.value    = siteName;
  gDialog.PublishUrlInput.value  = publishUrl;
  gDialog.BrowseUrlInput.value   = browseUrl;
  gDialog.UserNameInput.value    = username;
  gDialog.SiteList.selectedIndex = selectedSiteIndex;

  gSettingsChanged = false;
}

function SwitchPanel(panel)
{
  if (gCurrentPanel != panel)
  {
    // Set index for starting panel on the <tabpanels> element
    gDialog.TabPanels.selectedIndex = panel;
    if (panel == gSettingsPanel)
    {
      // Trigger setting of style for the tab widgets
      gDialog.SettingsTab.selected = "true";
      gDialog.PublishTab.selected = null;

      // We collapse part of the Settings panel so the Publish Panel can be more compact
      if (gDialog.ServerSettingsBox.getAttribute("collapsed"))
      {
        gDialog.ServerSettingsBox.removeAttribute("collapsed");
        window.sizeToContent();
      }
    } else {
      gDialog.PublishTab.selected = "true";
      gDialog.SettingsTab.selected = null;
      
      if (!SeeMore)
      {
        gDialog.ServerSettingsBox.setAttribute("collapsed","true");
        window.sizeToContent();
      }
    }
    gCurrentPanel = panel;
  }
}

function onInputSettings()
{
  // TODO: Save current data during SelectSite and compare here
  //       to detect if real change has occured?
  gSettingsChanged = true;
}

function ShowErrorInPanel(panelId, errorMsgId, widgetWithError)
{
  SwitchPanel(panelId);
  ShowInputErrorMessage(GetString(errorMsgId));
  if (widgetWithError)
    SetTextboxFocus(widgetWithError);
}

function ValidatePublishData()
{
  var selectedSiteIndex = gDialog.SiteList.selectedIndex;  
  
  if (selectedSiteIndex == -1)
  {
    if (gPublishSiteData)
      selectedSiteIndex = gDefaultSiteIndex;
    else
    {
      // No site selected!
      ShowErrorInPanel(gPublishPanel, "MissingPublishSiteError", null);
      AddNewSite();
    }
  }

  var title = TrimString(gDialog.PageTitleInput.value);
  var filename = TrimString(gDialog.FilenameInput.value);
  if (!filename)
  {
    ShowErrorInPanel(gPublishPanel, "MissingPublishFilename", gDialog.FilenameInput);
    return false;
  }
  gReturnData.Filename = filename;

  if (selectedSiteIndex >=0 )
  {
    // Get rest of the data from the Site database
    gReturnData.DestinationDir = gPublishSiteData[selectedSiteIndex][gUrlIndex];
    gReturnData.BrowseDir = gPublishSiteData[selectedSiteIndex][gBrowseIndex];
    gReturnData.UserName = gPublishSiteData[selectedSiteIndex][gUserNameIndex];
    // TODO: HOW TO GET PASSWORD?
    gReturnData.Password = "";
  }
  else
  {
    gReturnData.DestinationDir = "";
    gReturnData.BrowseDir = "";
    gReturnData.UserName = "";
    gReturnData.Password = "";
  }
  // TODO; RELATED DOCS (IMAGES)

  return true;
}

function ValidateSettingsData()
{
  if (gSettingsChanged)
    return SaveSettingsData();

  return true;
}


function SaveSettingsData()
{
  // Validate and add new site
  var newName = TrimString(gDialog.SiteNameInput.value);
  if (!newName)
  {
    ShowErrorInPanel(gSettingsPanel, "MissingSiteNameError", gDialog.SiteNameInput);
    return false;
  }
  var newUrl = TrimString(gDialog.PublishUrlInput.value);
  if (!newUrl)
  {
    ShowErrorInPanel(gSettingsPanel, "MissingPublishUrlError", gDialog.PublishUrlInput);
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
dump(" * Updating site data for list index = "+siteIndex+"\n");
    if (siteIndex == -1)
    {
      // No site is selected -- add new data at the end
      siteIndex = gPublishSiteData.length;
    }
  }
dump(" * SaveSettingsData: NEW DATA AT index="+siteIndex+", gPublishSiteData.length="+gPublishSiteData.length+"\n");

  gPublishSiteData[siteIndex] = new Array(gSiteDataLength);
  gPublishSiteData[siteIndex][gNameIndex] = newName;
  gPublishSiteData[siteIndex][gUrlIndex] = newUrl;
  gPublishSiteData[siteIndex][gBrowseIndex] = TrimString(gDialog.BrowseUrlInput.value);
  gPublishSiteData[siteIndex][gUserNameIndex] = TrimString(gDialog.UserNameInput.value);

  if (siteIndex == gDefaultSiteIndex)
    gDefaultSiteName = newName;

dump("  Default SiteName = "+gDefaultSiteName+", Index="+gDefaultSiteIndex+"\n");

dump("New Site Array: data="+gPublishSiteData[siteIndex][gNameIndex]+","+gPublishSiteData[siteIndex][gUrlIndex]+","+gPublishSiteData[siteIndex][gBrowseIndex]+","+gPublishSiteData[siteIndex][gUserNameIndex]+"\n");

  var publishSiteName = gPublishSiteData[siteIndex][gNameIndex];

  var count = gPublishSiteData.length;
  if (count > 1)
  {
    // XXX Ascii sort, not locale-aware
    gPublishSiteData.sort();

    // Find previously-selected item in sorted list
    for (var i = 0; i < count; i++)
    {
  dump(" Name #"+i+" = "+gPublishSiteData[i][gNameIndex]+"\n");

      if (gPublishSiteData[i][gNameIndex] == publishSiteName)
      {
        siteIndex = i;
        break;
      }
    }
  }

  // When adding the very first site, assume that's the default
  if (count == 1 && !gDefaultSiteName)
  {
    gDefaultSiteName = gPublishSiteData[0][gNameIndex];
    gDefaultSiteIndex = 0;
  }

  FillSiteList();
  gDialog.SiteList.selectedIndex = siteIndex;

  // Get the directory name -- add to database if not already there
  // Because of the autoselect behavior in editable menulists,
  //   selectedIndex = -1 means value in input field is not already in the list
  var dirIndex = gDialog.DirList.selectedIndex;
  var dirName = TrimString(gDialog.DirList.value);
  if (dirName && dirIndex == -1)
  {
    var dirListLen = gPublishSiteData[siteIndex][gDirListIndex].length;
dump(" *** Current directory list length = "+dirListLen+"\n");
    gPublishSiteData[siteIndex][gDirListIndex][dirListLen] = dirName;
  }
  gPublishSiteData[siteIndex][gDefaultDirIndex] = dirName;
    

  gSettingsChanged = false;

  SavePublishSiteDataToPrefs(gPublishSiteData, gDefaultSiteName);

  return true;
}

function ChooseDir()
{
}

function ValidateData()
{
  var result = true;;
  var savePanel = gCurrentPanel;

  // Validate current panel first
  if (gCurrentPanel == gPublishPanel)
  {
    result = ValidatePublishData();
    if (result)
      result = ValidateSettingsData();

  } else {
    result = ValidateSettingsData();
    if (result)
      result = ValidatePublishData();
  }
  return result;
}

function doHelpButton()
{
  openHelp("chrome://help/content/help.xul?publish");
}

function onAccept()
{
  if (ValidateData())
  {
    var title = TrimString(gDialog.PageTitleInput.value);
    if (title != gPreviousTitle)
      editorShell.SetDocumentTitle(title);
    
    SaveWindowLocation();
    return true;
  }

  return false;
}
