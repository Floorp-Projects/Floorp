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
var gDefaultSiteIndex = -1;
var gDefaultSiteName;
var gPreviousDefaultSite;
var gPreviousDefaultDir;
var gPreviousTitle;
var gSettingsChanged = false;
var gInitialSiteName;
var gInitialSiteIndex = -1;

// Dialog initialization code
function Startup()
{
  window.opener.ok = false;

  if (!InitEditorShell()) return;

  // Element to edit is passed in
  gInitialSiteName = window.arguments[1];
  gReturnData = window.arguments[2];
  if (!gReturnData)
  {
    dump("Publish: Return data object not supplied\n");
    window.close();
    return;
  }

  gDialog.TabPanels           = document.getElementById("TabPanels");
  gDialog.PublishTab          = document.getElementById("PublishTab");
  gDialog.SettingsTab         = document.getElementById("SettingsTab");

  // Publish panel
  gDialog.PageTitleInput      = document.getElementById("PageTitleInput");
  gDialog.FilenameInput       = document.getElementById("FilenameInput");
  gDialog.SiteList            = document.getElementById("SiteList");
  gDialog.DocDirList          = document.getElementById("DocDirList");
  gDialog.OtherDirCheckbox    = document.getElementById("OtherDirCheckbox");
  gDialog.OtherDirList        = document.getElementById("OtherDirList");

  gDialog.MoreSection         = document.getElementById("MoreSection");
  gDialog.MoreFewerButton     = document.getElementById("MoreFewerButton");

  // Settings Panel
  gDialog.SettingsPanel       = document.getElementById("SettingsPanel");
  gDialog.ServerSettingsBox   = document.getElementById("ServerSettingsBox");
  gDialog.SiteNameInput       = document.getElementById("SiteNameInput");
  gDialog.PublishUrlInput     = document.getElementById("PublishUrlInput");
  gDialog.BrowseUrlInput      = document.getElementById("BrowseUrlInput");
  gDialog.UsernameInput       = document.getElementById("UsernameInput");
  gDialog.PasswordInput       = document.getElementById("PasswordInput");
  gDialog.SavePassword        = document.getElementById("SavePassword");

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
  if (gPublishSiteData)
    FillSiteList();

  var docUrl = GetDocumentUrl();
  var scheme = GetScheme(docUrl);
  var filename = "";
  if (scheme)
  {
    filename = GetFilename(docUrl);

    if (scheme != "file")
    {
      // Editing a remote URL.
      // Attempt to find doc URL in Site Data
      if (gPublishSiteData)
      {
        var dirObj = {};
        var siteIndex = FindSiteIndexAndDocDir(gPublishSiteData, docUrl, dirObj);
        
        // Select this site only if the same as user's intended site, or there wasnt' one
        if (siteIndex != -1 && (gInitialSiteIndex == -1 || siteIndex == gInitialSiteIndex))
        {
          // Select the site we found
          gDialog.SiteList.selectedIndex = siteIndex;
          var docDir = dirObj.value;

          // Be sure directory found is part of that site's dir list
          AppendDirToSelectedSite(docDir);

          // Use the directory within site in the editable menulist
          gPublishSiteData[siteIndex].docDir = docDir;

          //XXX HOW DO WE DECIDE WHAT "OTHER" DIR TO USE?
          //gPublishSiteData[siteIndex].otherDir = docDir;
        }
      }
    }
  }

  // We haven't selected a site -- use initial or default site
  if (gDialog.SiteList.selectedIndex == -1)
    gDialog.SiteList.selectedIndex = (gInitialSiteIndex != -1) ? gInitialSiteIndex : gDefaultSiteIndex;

  // Fill in  all the site data for currently-selected site
  SelectSiteList();

  try {
    gPreviousTitle = editorShell.GetDocumentTitle();
  } catch (e) {}

  gDialog.PageTitleInput.value = gPreviousTitle;
  gDialog.FilenameInput.value = filename;
  
  //XXX TODO: How do we decide whether or not to save associated files?
  gDialog.OtherDirCheckbox.checked = true;
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
    var name = gPublishSiteData[i].siteName;
    var menuitem = AppendStringToMenulist(gDialog.SiteList, name);
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
  gDialog.OtherDirList.disabled = !gDialog.OtherDirCheckbox.checked;
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

  ClearMenulist(gDialog.DocDirList);
  ClearMenulist(gDialog.OtherDirList);

  if (gPublishSiteData && selectedSiteIndex != -1)
  {
    siteName = gPublishSiteData[selectedSiteIndex].siteName;
    publishUrl = gPublishSiteData[selectedSiteIndex].publishUrl;
    browseUrl = gPublishSiteData[selectedSiteIndex].browseUrl;
    username = gPublishSiteData[selectedSiteIndex].username;
    savePassword = gPublishSiteData[selectedSiteIndex].savePassword;
    if (savePassword)
      password = gPublishSiteData[selectedSiteIndex].password;

    // Fill the directory menulists
    if (gPublishSiteData[selectedSiteIndex].dirList.length)
    {
      for (var i = 0; i < gPublishSiteData[selectedSiteIndex].dirList.length; i++)
      {
        AppendStringToMenulist(gDialog.DocDirList, gPublishSiteData[selectedSiteIndex].dirList[i]);
        AppendStringToMenulist(gDialog.OtherDirList, gPublishSiteData[selectedSiteIndex].dirList[i]);
      }
    }
    gDialog.DocDirList.value = gPublishSiteData[selectedSiteIndex].docDir;
    gDialog.OtherDirList.value = gPublishSiteData[selectedSiteIndex].otherDir;

    gDialog.DocDirList.value = FormatDirForPublishing(gPublishSiteData[selectedSiteIndex].docDir);
    gDialog.OtherDirList.value = FormatDirForPublishing(gPublishSiteData[selectedSiteIndex].otherDir);
  }
  else
  {
    gDialog.DocDirList.value = "/";
    gDialog.OtherDirList.value = "/";
  }

  gDialog.SiteNameInput.value    = siteName;
  gDialog.PublishUrlInput.value  = publishUrl;
  gDialog.BrowseUrlInput.value   = browseUrl;
  gDialog.UsernameInput.value    = username;
  gDialog.PasswordInput.value    = password;
  gDialog.SavePassword.checked   = savePassword;
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
  
  gDialog.SiteList.selectedIndex = -1;

  SelectSiteList();
  
  gSettingsChanged = true;

  SetTextboxFocus(gDialog.SiteNameInput);
}

function SelectPublishTab()
{
  if (gSettingsChanged)
  {
    gCurrentPanel = gPublishPanel;
    if (!ValidateSettings())
      return;

    gCurrentPanel = gSettingsPanel;
  }

  SwitchPanel(gPublishPanel);
  SetTextboxFocus(gDialog.PageTitleInput);
}

function SelectSettingsTab()
{
  SwitchPanel(gSettingsPanel);
  SetTextboxFocus(gDialog.SiteNameInput);
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

function AppendDirToSelectedSite(dir)
{
  var selectedSiteIndex = gDialog.SiteList.selectedIndex;
  if (selectedSiteIndex == -1)
    return;

  var i;
  var dirFound = false;
  for (i = 0; i < gPublishSiteData[selectedSiteIndex].dirList.length; i++)
  {
    dirFound = (dir == gPublishSiteData[selectedSiteIndex].dirList[i]);
    if (dirFound)
      break;
  }
  if (!dirFound)
  {
    // Append dir to end and sort
    gPublishSiteData[selectedSiteIndex].dirList[i] = dir;
    if (gPublishSiteData[selectedSiteIndex].dirList.length > 1)
      gPublishSiteData[selectedSiteIndex].dirList.sort();
  }
}

function ValidateSettings()
{
  var siteName = TrimString(gDialog.SiteNameInput.value);
  if (!siteName)
  {
    ShowErrorInPanel(gSettingsPanel, "MissingSiteNameError", gDialog.SiteNameInput);
    return false;
  }

  var publishUrl = GetPublishUrlInput();
  if (!publishUrl)
  {
    ShowErrorInPanel(gSettingsPanel, "MissingPublishUrlError", gDialog.PublishUrlInput);
    return false;
  }

  //TODO: If publish scheme = "ftp" we should encourage user to supply the http BrowseUrl
  var browseUrl = GetBrowseUrlInput();

  //XXXX We don't get a prompt dialog if username is missing (bug ?????)
  //     If not, we must force user to supply one here
  var username = TrimString(gDialog.UsernameInput.value);
  var savePassword = gDialog.SavePassword.checked;
  var password = gDialog.PasswordInput.value;

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
      gPublishSiteData = new Array(1);
      siteIndex = 0;
      gDefaultSiteIndex = 0;
      gDefaultSiteName = siteName;
    }    
    gPublishSiteData[siteIndex] = {};
    gPublishSiteData[siteIndex].docDir = "/";
    gPublishSiteData[siteIndex].otherDir = "/";
    gPublishSiteData[siteIndex].dirList = ["/"];
    newSite = true;
  }
  gPublishSiteData[siteIndex].siteName = siteName;
  gPublishSiteData[siteIndex].publishUrl = publishUrl;
  gPublishSiteData[siteIndex].browseUrl = browseUrl;
  gPublishSiteData[siteIndex].username = username;
  // Don't save password in data that will be saved in prefs
  gPublishSiteData[siteIndex].password = savePassword ? password : "";
  gPublishSiteData[siteIndex].savePassword = savePassword;

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
    if (selectedItem && selectedItem.getAttribute("label") != siteName)
    {
      selectedItem.setAttribute("label", siteName);
      gDialog.SiteList.setAttribute("label", siteName);
    }
  }
  
  // Get the directory name in site to publish to
  var docDir = GetDocDirInput();

  // Because of the autoselect behavior in editable menulists,
  //   selectedIndex = -1 means value in input field is not already in the list, 
  //   so add it to the list of site directories
  if (gDialog.DocDirList.selectedIndex == -1)
    AppendDirToSelectedSite(docDir);

  gPublishSiteData[siteIndex].docDir = docDir;

  // And directory for images and other files
  var otherDir = GetOtherDirInput();
  if (gDialog.OtherDirList.selectedIndex == -1)
    AppendDirToSelectedSite(otherDir);

  gPublishSiteData[siteIndex].otherDir = otherDir;

  // Fill return data object
  gReturnData.siteName = siteName;
  gReturnData.publishUrl = publishUrl;
  gReturnData.browseUrl = browseUrl;
  gReturnData.username = username;
  // Note that we use the password for the next publish action 
  // even if savePassword is false; but we won't save it in PasswordManager database
  gReturnData.password = password;
  gReturnData.savePassword = savePassword;
  gReturnData.docDir = gPublishSiteData[siteIndex].docDir;

  // If "Other dir" is not checked, return empty string to indicate "don't save associated files"
  gReturnData.otherDir = gDialog.OtherDirCheckbox.checked ? gPublishSiteData[siteIndex].otherDir : "";

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
    ShowErrorInPanel(gPublishPanel, "MissingPublishFilename", gDialog.FilenameInput);
    return false;
  }

  gReturnData.filename = filename;

  return true;
}

function ShowErrorInPanel(panelId, errorMsgId, widgetWithError)
{
  SwitchPanel(panelId);
  ShowInputErrorMessage(GetString(errorMsgId));
  if (widgetWithError)
    SetTextboxFocus(widgetWithError);
}

function doHelpButton()
{
  if (gCurrentPanel == gPublishPanel)
    openHelp("publish_tab");
  else
    openHelp("settings_tab");
}

function onAccept()
{
  if (ValidateData())
  {
    // We save new site data to prefs only if we are attempting to publish
    if (gSettingsChanged)
      SavePublishSiteDataToPrefs(gPublishSiteData, gDefaultSiteName);

    var title = TrimString(gDialog.PageTitleInput.value);
    if (title != gPreviousTitle)
      editorShell.SetDocumentTitle(title);

    SaveWindowLocation();
    window.opener.ok = true;
    return true;
  }

  return false;
}
