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
 *   Charley Manske (cmanske@netscape.com)
 *   Ryan Cassin (rcassin@supernova.org)
 */

var gPublishSiteData;
var gReturnData;
var gDefaultSiteIndex = -1;
var gDefaultSiteName;
var gPreviousDefaultDir;
var gPreviousTitle;
var gSettingsChanged = false;
var gInitialSiteName;
var gInitialSiteIndex = -1;
var gPasswordManagerOn = true;

// Dialog initialization code
function Startup()
{
  window.opener.ok = false;

  // Element to edit is passed in
  gInitialSiteName = window.arguments[1];
  gReturnData = window.arguments[2];
  if (!gReturnData || !GetCurrentEditor())
  {
    dump("Publish: No editor or return data object not supplied\n");
    window.close();
    return;
  }

  gDialog.TabBox              = document.getElementById("TabBox");
  gDialog.PublishTab          = document.getElementById("PublishTab");
  gDialog.SettingsTab         = document.getElementById("SettingsTab");

  // Publish panel
  gDialog.PageTitleInput      = document.getElementById("PageTitleInput");
  gDialog.FilenameInput       = document.getElementById("FilenameInput");
  gDialog.SiteList            = document.getElementById("SiteList");
  gDialog.DocDirList          = document.getElementById("DocDirList");
  gDialog.OtherDirCheckbox    = document.getElementById("OtherDirCheckbox");
  gDialog.OtherDirRadiogroup  = document.getElementById("OtherDirRadiogroup");
  gDialog.SameLocationRadio   = document.getElementById("SameLocationRadio");
  gDialog.UseSubdirRadio      = document.getElementById("UseSubdirRadio");
  gDialog.OtherDirList        = document.getElementById("OtherDirList");

  // Settings Panel
  gDialog.SiteNameInput       = document.getElementById("SiteNameInput");
  gDialog.PublishUrlInput     = document.getElementById("PublishUrlInput");
  gDialog.BrowseUrlInput      = document.getElementById("BrowseUrlInput");
  gDialog.UsernameInput       = document.getElementById("UsernameInput");
  gDialog.PasswordInput       = document.getElementById("PasswordInput");
  gDialog.SavePassword        = document.getElementById("SavePassword");

  gDialog.PublishButton       = document.documentElement.getButton("accept");

  // Change 'Ok' button to 'Publish'
  gDialog.PublishButton.setAttribute("label", GetString("Publish"));
  
  gPasswordManagerOn = GetBoolPref("signon.rememberSignons");
  gDialog.SavePassword.disabled = !gPasswordManagerOn;

  gPublishSiteData = GetPublishSiteData();
  gDefaultSiteName = GetDefaultPublishSiteName();

  var addNewSite = false;
  if (gPublishSiteData)
  {
    FillSiteList();
  }
  else
  {
    // No current site data, start a new item in the Settings panel
    AddNewSite();
    addNewSite = true;
  }

  var docUrl = GetDocumentUrl();
  var scheme = GetScheme(docUrl);
  var filename = "";

  if (scheme)
  {
    filename = GetFilename(docUrl);

    if (scheme != "file")
    {
      var siteFound = false;

      // Editing a remote URL.
      // Attempt to find doc URL in Site Data
      if (gPublishSiteData)
      {
        var dirObj = {};
        var siteIndex = FindSiteIndexAndDocDir(gPublishSiteData, docUrl, dirObj);

        // Select this site only if the same as user's intended site, or there wasn't one
        if (siteIndex != -1 && (gInitialSiteIndex == -1 || siteIndex == gInitialSiteIndex))
        {
          siteFound = true;

          // Select the site we found
          gDialog.SiteList.selectedIndex = siteIndex;
          var docDir = dirObj.value;

          // Use the directory within site in the editable menulist
          gPublishSiteData[siteIndex].docDir = docDir;

          //XXX HOW DO WE DECIDE WHAT "OTHER" DIR TO USE?
          //gPublishSiteData[siteIndex].otherDir = docDir;
        }
      }
      if (!siteFound)
      {
        // Not found in site database 
        // Setup for a new site and use data from a remote URL
        if (!addNewSite)
          AddNewSite();

        addNewSite = true;

        var publishData = CreatePublishDataFromUrl(docUrl);
        if (publishData)
        {
          filename = publishData.filename;
          gDialog.SiteNameInput.value    = publishData.siteName;
          gDialog.PublishUrlInput.value  = publishData.publishUrl;
          gDialog.BrowseUrlInput.value   = publishData.browseUrl;
          gDialog.UsernameInput.value    = publishData.username;
          gDialog.PasswordInput.value    = publishData.password;
          gDialog.SavePassword.checked   = false;
        }
      }
    }
  }
  try {
    gPreviousTitle = GetDocumentTitle();
  } catch (e) {}

  gDialog.PageTitleInput.value = gPreviousTitle;
  gDialog.FilenameInput.value = unescape(filename);
  
  if (!addNewSite)
  {
    // If not adding a site and we haven't selected a site -- use initial or default site
    if (gDialog.SiteList.selectedIndex == -1)
      gDialog.SiteList.selectedIndex = (gInitialSiteIndex != -1) ? gInitialSiteIndex : gDefaultSiteIndex;

    // Fill in  all the site data for currently-selected site
    SelectSiteList();
    SetTextboxFocus(gDialog.PageTitleInput);
  }

  if (gDialog.SiteList.selectedIndex == -1)
  {
    // No selected site -- assume same directory
    gDialog.OtherDirRadiogroup.selectedItem = gDialog.SameLocationRadio;
  }
  else if (gPublishSiteData[gDialog.SiteList.selectedIndex].docDir == 
        gPublishSiteData[gDialog.SiteList.selectedIndex].otherDir)
  {
    // For now, check "same location" if dirs are already set to same directory
    gDialog.OtherDirRadiogroup.selectedItem = gDialog.SameLocationRadio;
  }
  else
  {
    gDialog.OtherDirRadiogroup.selectedItem = gDialog.UseSubdirRadio;
  }

  doEnabling();

  SetWindowLocation();
}

function FillSiteList()
{
  gDialog.SiteList.removeAllItems();
  gDefaultSiteIndex = -1;

  // Fill the site lists
  var count = gPublishSiteData.length;
  var i;

  for (i = 0; i < count; i++)
  {
    var name = gPublishSiteData[i].siteName;
    var menuitem = gDialog.SiteList.appendItem(name);
    // Highlight the default site
    if (name == gDefaultSiteName)
    {
      gDefaultSiteIndex = i;
      if (menuitem)
      {
        menuitem.setAttribute("class", "menuitem-highlight-1");
        menuitem.setAttribute("default", "true");
      }
    }
    // Find initial site location
    if (name == gInitialSiteName)
      gInitialSiteIndex = i;
  }
}

function doEnabling()
{
  var disableOther = !gDialog.OtherDirCheckbox.checked;
  gDialog.SameLocationRadio.disabled = disableOther;
  gDialog.UseSubdirRadio.disabled = disableOther;
  gDialog.OtherDirList.disabled = (disableOther || gDialog.SameLocationRadio.selected);
}

function SelectSiteList()
{
  var selectedSiteIndex = gDialog.SiteList.selectedIndex;  

  var siteName = "";
  var publishUrl = "";
  var browseUrl = "";
  var username = "";
  var password = "";
  var savePassword = false;
  var publishOtherFiles = true;

  gDialog.DocDirList.removeAllItems();
  gDialog.OtherDirList.removeAllItems();

  if (gPublishSiteData && selectedSiteIndex != -1)
  {
    siteName = gPublishSiteData[selectedSiteIndex].siteName;
    publishUrl = gPublishSiteData[selectedSiteIndex].publishUrl;
    browseUrl = gPublishSiteData[selectedSiteIndex].browseUrl;
    username = gPublishSiteData[selectedSiteIndex].username;
    savePassword = gPasswordManagerOn ? gPublishSiteData[selectedSiteIndex].savePassword : false;
    if (savePassword)
      password = gPublishSiteData[selectedSiteIndex].password;

    // Fill the directory menulists
    if (gPublishSiteData[selectedSiteIndex].dirList.length)
    {
      for (var i = 0; i < gPublishSiteData[selectedSiteIndex].dirList.length; i++)
      {
        gDialog.DocDirList.appendItem(gPublishSiteData[selectedSiteIndex].dirList[i]);
        gDialog.OtherDirList.appendItem(gPublishSiteData[selectedSiteIndex].dirList[i]);
      }
    }
    gDialog.DocDirList.value = FormatDirForPublishing(gPublishSiteData[selectedSiteIndex].docDir);
    gDialog.OtherDirList.value = FormatDirForPublishing(gPublishSiteData[selectedSiteIndex].otherDir);
    publishOtherFiles = gPublishSiteData[selectedSiteIndex].publishOtherFiles;

  }
  else
  {
    gDialog.DocDirList.value = "";
    gDialog.OtherDirList.value = "";
  }

  gDialog.SiteNameInput.value    = siteName;
  gDialog.PublishUrlInput.value  = publishUrl;
  gDialog.BrowseUrlInput.value   = browseUrl;
  gDialog.UsernameInput.value    = username;
  gDialog.PasswordInput.value    = password;
  gDialog.SavePassword.checked   = savePassword;
  gDialog.OtherDirCheckbox.checked = publishOtherFiles;

  doEnabling();
}

function AddNewSite()
{
  // Button in Publish panel allows user
  //  to automatically switch to "Settings" panel
  //  to enter data for new site
  SwitchPanel(gDialog.SettingsTab);
  
  gDialog.SiteList.selectedIndex = -1;

  SelectSiteList();
  
  gSettingsChanged = true;

  SetTextboxFocus(gDialog.SiteNameInput);
}

function SelectPublishTab()
{
  if (gSettingsChanged && !ValidateSettings())
    return;

  SwitchPanel(gDialog.PublishTab);
  SetTextboxFocus(gDialog.PageTitleInput);
}

function SelectSettingsTab()
{
  SwitchPanel(gDialog.SettingsTab);
  SetTextboxFocus(gDialog.SiteNameInput);
}

function SwitchPanel(tab)
{
  if (gDialog.TabBox.selectedTab != tab)
    gDialog.TabBox.selectedTab = tab;
}

function onInputSettings()
{
  // TODO: Save current data during SelectSite and compare here
  //       to detect if real change has occured?
  gSettingsChanged = true;
}

function GetPublishUrlInput()
{
  gDialog.PublishUrlInput.value = FormatUrlForPublishing(gDialog.PublishUrlInput.value);
  return gDialog.PublishUrlInput.value;
}

function GetBrowseUrlInput()
{
  gDialog.BrowseUrlInput.value = FormatUrlForPublishing(gDialog.BrowseUrlInput.value);
  return gDialog.BrowseUrlInput.value;
}

function GetDocDirInput()
{
  gDialog.DocDirList.value = FormatDirForPublishing(gDialog.DocDirList.value);
  return gDialog.DocDirList.value;
}

function GetOtherDirInput()
{
  gDialog.OtherDirList.value = FormatDirForPublishing(gDialog.OtherDirList.value);
  return gDialog.OtherDirList.value;
}

function ChooseDir(menulist)
{
  //TODO: For FTP publish destinations, get file listing of just dirs 
  //  and build a tree to let user select dir
}

function ValidateSettings()
{
  var siteName = TrimString(gDialog.SiteNameInput.value);
  if (!siteName)
  {
    ShowErrorInPanel(gDialog.SettingsTab, "MissingSiteNameError", gDialog.SiteNameInput);
    return false;
  }
  if (PublishSiteNameExists(siteName, gPublishSiteData, gDialog.SiteList.selectedIndex))
  {
    SwitchPanel(gDialog.SettingsTab);
    ShowInputErrorMessage(GetString("DuplicateSiteNameError").replace(/%name%/, siteName));            
    SetTextboxFocus(gDialog.SiteNameInput);
    return false;
  }

  // Extract username and password while removing them from publishingUrl
  var urlUserObj = {};
  var urlPassObj = {};
  var publishUrl = StripUsernamePassword(gDialog.PublishUrlInput.value, urlUserObj, urlPassObj);
  if (publishUrl)
  {
    publishUrl = FormatUrlForPublishing(publishUrl);

    // Assume scheme = "ftp://" if missing
    // This compensates when user enters hostname w/o scheme (as most ISPs provide)
    if (!GetScheme(publishUrl))
      publishUrl = "ftp://" + publishUrl; 

    gDialog.PublishUrlInput.value = publishUrl;
  }
  else
  {
    ShowErrorInPanel(gDialog.SettingsTab, "MissingPublishUrlError", gDialog.PublishUrlInput);
    return false;
  }
  var browseUrl = GetBrowseUrlInput();

  var username = TrimString(gDialog.UsernameInput.value);
  var savePassword = gDialog.SavePassword.checked;
  var password = gDialog.PasswordInput.value;
  var publishOtherFiles = gDialog.OtherDirCheckbox.checked;
  
  //XXX If there was a username and/or password in the publishUrl 
  //    AND in the input field, which do we use?
  //    Let's use those in url only if input is empty 
  if (!username)
  {
    username = urlUserObj.value;
    gDialog.UsernameInput.value = username;
    gSettingsChanged = true;
  }
  if (!password)
  {
    password = urlPassObj.value;
    gDialog.PasswordInput.value = password;
    gSettingsChanged = true;
  }

  // Update or add data for a site 
  var siteIndex = gDialog.SiteList.selectedIndex;
  var newSite = false;

  if (siteIndex == -1)
  {
    // No site is selected, add a new site at the end
    if (gPublishSiteData)
    {
      siteIndex = gPublishSiteData.length;
    }
    else
    {
      // First time: start entire site array
      gPublishSiteData = new Array(1);
      siteIndex = 0;
      gDefaultSiteIndex = 0;
      gDefaultSiteName = siteName;
    }    
    gPublishSiteData[siteIndex] = {};
    gPublishSiteData[siteIndex].docDir = "";
    gPublishSiteData[siteIndex].otherDir = "";
    gPublishSiteData[siteIndex].dirList = [""];
    gPublishSiteData[siteIndex].publishOtherFiles = true;
    gPublishSiteData[siteIndex].previousSiteName = siteName;
    newSite = true;
  }
  gPublishSiteData[siteIndex].siteName = siteName;
  gPublishSiteData[siteIndex].publishUrl = publishUrl;
  gPublishSiteData[siteIndex].browseUrl = browseUrl;
  gPublishSiteData[siteIndex].username = username;
  // Don't save password in data that will be saved in prefs
  gPublishSiteData[siteIndex].password = savePassword ? password : "";
  gPublishSiteData[siteIndex].savePassword = savePassword;

  if (publishOtherFiles != gPublishSiteData[siteIndex].publishOtherFiles)
    gSettingsChanged = true;

  gPublishSiteData[siteIndex].publishOtherFiles = publishOtherFiles;

  gDialog.SiteList.selectedIndex = siteIndex;
  if (siteIndex == gDefaultSiteIndex)
    gDefaultSiteName = siteName;

  // Should never be empty, but be sure we have a default site
  if (!gDefaultSiteName)
  {
    gDefaultSiteName = gPublishSiteData[0].siteName;
    gDefaultSiteIndex = 0;
  }

  // Rebuild the site menulist if we added a new site
  if (newSite)
  {
    FillSiteList();
    gDialog.SiteList.selectedIndex = siteIndex;
  }
  else
  {
    // Update selected item if sitename changed 
    var selectedItem = gDialog.SiteList.selectedItem;
    if (selectedItem)
    {
      var oldName = selectedItem.getAttribute("label");
      if (oldName != siteName)
      {
        selectedItem.setAttribute("label", siteName);
        gDialog.SiteList.setAttribute("label", siteName);
        gSettingsChanged = true;
        if (oldName == gDefaultSiteName)
          gDefaultSiteName = siteName;
      }
    }
  }
  
  // Get the directory name in site to publish to
  var docDir = GetDocDirInput();

  gPublishSiteData[siteIndex].docDir = docDir;

  // And directory for images and other files
  var otherDir = GetOtherDirInput();
  if (gDialog.SameLocationRadio.selected)
    otherDir = docDir;
  else
    otherDir = GetOtherDirInput();

  gPublishSiteData[siteIndex].otherDir = otherDir;

  // Fill return data object
  gReturnData.siteName = siteName;
  gReturnData.previousSiteName = gPublishSiteData[siteIndex].previousSiteName;
  gReturnData.publishUrl = publishUrl;
  gReturnData.browseUrl = browseUrl;
  gReturnData.username = username;
  // Note that we use the password for the next publish action 
  // even if savePassword is false; but we won't save it in PasswordManager database
  gReturnData.password = password;
  gReturnData.savePassword = savePassword;
  gReturnData.docDir = gPublishSiteData[siteIndex].docDir;
  gReturnData.otherDir = gPublishSiteData[siteIndex].otherDir;
  gReturnData.publishOtherFiles = publishOtherFiles;
  gReturnData.dirList = gPublishSiteData[siteIndex].dirList;
  return true;
}

function ValidateData()
{
  if (!ValidateSettings())
    return false;

  var siteIndex = gDialog.SiteList.selectedIndex;
  if (siteIndex == -1)
    return false;

  var filename = TrimString(gDialog.FilenameInput.value);
  if (!filename)
  {
    ShowErrorInPanel(gDialog.PublishTab, "MissingPublishFilename", gDialog.FilenameInput);
    return false;
  }
  gReturnData.filename = filename;

  return true;
}

function ShowErrorInPanel(tab, errorMsgId, widgetWithError)
{
  SwitchPanel(tab);
  ShowInputErrorMessage(GetString(errorMsgId));
  if (widgetWithError)
    SetTextboxFocus(widgetWithError);
}

function doHelpButton()
{
  if (gDialog.TabBox.selectedTab == gDialog.PublishTab)
    openHelp("comp-doc-publish-publishtab");
  else
    openHelp("comp-doc-publish-settingstab");
}

function onAccept()
{
  if (ValidateData())
  {
    //  DON'T save the docDir and otherDir before trying to publish
    gReturnData.saveDirs = false;

    // We save new site data to prefs only if we are attempting to publish
    if (gSettingsChanged)
      SavePublishDataToPrefs(gReturnData);

    // Set flag to resave data after publishing
    // so we save docDir and otherDir if we published successfully
    gReturnData.savePublishData = true;

    var title = TrimString(gDialog.PageTitleInput.value);
    if (title != gPreviousTitle)
      SetDocumentTitle(title);

    SaveWindowLocation();
    window.opener.ok = true;
    return true;
  }

  return false;
}
