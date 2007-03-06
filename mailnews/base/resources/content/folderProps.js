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
 * Portions created by the Initial Developer are Copyright (C) 2000-2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Bienvenu <bienvenu@mozilla.org>
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

var gMsgFolder;
var gServerTypeFolder = null;
var gPreselectedFolderURI = null;
var gParentMsgWindow = null;
var gNameTextbox;
var gOldName;
var gOkButton;
var gLockedPref = null;

var gRebuildSummaryFileCallback;

// services used
var RDF;

// corresponds to MSG_FOLDER_FLAG_OFFLINE
const MSG_FOLDER_FLAG_OFFLINE = 0x8000000

// corresponds to MSG_FOLDER_FLAG_CHECK_NEW
const MSG_FOLDER_FLAG_CHECK_NEW = 0x20000000

// corresponds to MSG_FOLDER_FLAG_INBOX
const MSG_FOLDER_FLAG_INBOX = 0x1000

// The folderPropsSink is the class that gets notified of an imap folder's properties

var gFolderPropsSink = {
    setFolderType: function(folderTypeString)
    {
      var typeLabel = document.getElementById("folderType.text");
      if (typeLabel)
      {
        typeLabel.setAttribute("value",folderTypeString);
      }
      // get the element for the folder type label and set value on it.
    },

    setFolderTypeDescription: function(folderDescription)
    {
      var folderTypeLabel = document.getElementById("folderDescription.text");
      if (folderTypeLabel)
        folderTypeLabel.setAttribute("value", folderDescription);
    },

    setFolderPermissions: function(folderPermissions)
    {
      var permissionsLabel = document.getElementById("folderPermissions.text");
      var descTextNode =  document.createTextNode(folderPermissions);
      permissionsLabel.appendChild(descTextNode);
    },

    serverDoesntSupportACL : function()
    {
      var typeLabel = document.getElementById("folderTypeLabel");
      if (typeLabel)
        typeLabel.setAttribute("hidden", "true");
      var permissionsLabel = document.getElementById("permissionsDescLabel");
      if (permissionsLabel)
        permissionsLabel.setAttribute("hidden", "true");

    },

    setQuotaStatus : function(folderQuotaStatus)
    {
      var quotaStatusLabel = document.getElementById("folderQuotaStatus");
      if(quotaStatusLabel)
        quotaStatusLabel.setAttribute("value", folderQuotaStatus);
    },

    showQuotaData : function(showData)
    {
      var quotaStatusLabel = document.getElementById("folderQuotaStatus");
      var folderQuotaData = document.getElementById("folderQuotaData");

      if(quotaStatusLabel && folderQuotaData)
      {
        quotaStatusLabel.hidden = showData;
        folderQuotaData.hidden = ! showData;
      }
    },

    setQuotaData : function(root, usedKB, maxKB)
    {
      var quotaRoot = document.getElementById("quotaRoot");
      if (quotaRoot)
        quotaRoot.setAttribute("value", '"' + root + '"');

      var percentage = (maxKB != 0) ? Math.round(usedKB / maxKB * 100) : 0;

      var quotaPercentageBar = document.getElementById("quotaPercentageBar");
      if (quotaPercentageBar)
        quotaPercentageBar.setAttribute("value", percentage);

      var bundle = document.getElementById("bundle_messenger");
      if(bundle)
      {
        var usedFreeCaption = bundle.getFormattedString("quotaUsedFree", [usedKB, maxKB], 2);
        quotaCaption = document.getElementById("quotaUsedFree");
        if(quotaCaption)
          quotaCaption.setAttribute("value", usedFreeCaption);

        var percentUsedCaption = bundle.getFormattedString("quotaPercentUsed", [percentage], 1);
        var percentUsed = document.getElementById("quotaPercentUsed");
        if(percentUsed)
          percentUsed.setAttribute("value", percentUsedCaption);
      }
    }

};

function doEnabling()
{
  gOkButton.disabled = !gNameTextbox.value;
}

function folderPropsOKButton()
{
  if (gMsgFolder)
  {

    // set charset attributes
    var folderCharsetList = document.getElementById("folderCharsetList");
    gMsgFolder.charset = folderCharsetList.getAttribute("value");
    gMsgFolder.charsetOverride = document.getElementById("folderCharsetOverride").checked;

    if(document.getElementById("offline.selectForOfflineFolder").checked ||
      document.getElementById("offline.selectForOfflineNewsgroup").checked)
      gMsgFolder.setFlag(MSG_FOLDER_FLAG_OFFLINE);
    else
      gMsgFolder.clearFlag(MSG_FOLDER_FLAG_OFFLINE);

    if(document.getElementById("folderCheckForNewMessages").checked)
      gMsgFolder.setFlag(MSG_FOLDER_FLAG_CHECK_NEW);
    else
      gMsgFolder.clearFlag(MSG_FOLDER_FLAG_CHECK_NEW);

    var retentionSettings = saveCommonRetentionSettings();
    retentionSettings.useServerDefaults = document.getElementById("retention.useDefault").checked;
    gMsgFolder.retentionSettings = retentionSettings;

  }

  try
  {
    // This throws an exception when an illegal folder name was entered.
    okCallback(gNameTextbox.value, gOldName, gPreselectedFolderURI);

    return true;
  }
  catch (e)
  {
    return false;
  }
}

function folderPropsOnLoad()
{
  gOkButton = document.documentElement.getButton("accept");

  RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  // look in arguments[0] for parameters
  if (window.arguments && window.arguments[0]) {
    if ( window.arguments[0].title ) {
      document.title = window.arguments[0].title;
    }
    if ( window.arguments[0].okCallback ) {
      top.okCallback = window.arguments[0].okCallback;
    }
    if (window.arguments[0].rebuildSummaryCallback)
      gRebuildSummaryFileCallback = window.arguments[0].rebuildSummaryCallback;
  }

  // fill in folder name, based on what they selected in the folder pane
  if (window.arguments[0].preselectedURI) {
    try {
      gPreselectedFolderURI = window.arguments[0].preselectedURI;
    }
    catch (ex) {
    }
  }
  else {
    dump("passed null for preselectedURI, do nothing\n");
  }

  if(window.arguments[0].name)
  {
    // Initialize name textbox with the given name and remember this
    // value so we can tell whether the folder needs to be renamed
    // when the dialog is accepted.
    gNameTextbox = document.getElementById("name");
    gNameTextbox.value = gOldName = window.arguments[0].name;

//  name.setSelectionRange(0,-1);
//  name.focusTextField();
  }

  gServerTypeFolder = window.arguments[0].serverType;

  if (window.arguments && window.arguments[0]) {
    if (window.arguments[0].msgWindow) {
      gParentMsgWindow = window.arguments[0].msgWindow;
    }
  }

  // this hex value come from nsMsgFolderFlags.h
  var folderResource = RDF.GetResource(gPreselectedFolderURI);
    
  if (folderResource)
    gMsgFolder = folderResource.QueryInterface(Components.interfaces.nsIMsgFolder);
  if (!gMsgFolder)
    dump("no gMsgFolder preselectfolder uri = "+gPreselectedFolderURI+'\n');

  if (gMsgFolder) {
    if (gMsgFolder.canRename)
      gNameTextbox.removeAttribute("readonly");

    if (gMsgFolder.flags & MSG_FOLDER_FLAG_OFFLINE) {

      if(gServerTypeFolder == "imap" || gServerTypeFolder == "pop3")
        document.getElementById("offline.selectForOfflineFolder").checked = true;

      if(gServerTypeFolder == "nntp")
        document.getElementById("offline.selectForOfflineNewsgroup").checked = true;
    }
    else {
      if(gServerTypeFolder == "imap" || gServerTypeFolder == "pop3")
        document.getElementById("offline.selectForOfflineFolder").checked = false;

      if(gServerTypeFolder == "nntp")
        document.getElementById("offline.selectForOfflineNewsgroup").checked = false;
    }

    // select the menu item 
    var folderCharsetList = document.getElementById("folderCharsetList");
    var elements = folderCharsetList.getElementsByAttribute("value", gMsgFolder.charset);
    folderCharsetList.selectedItem = elements[0];


    // set override checkbox
    document.getElementById("folderCharsetOverride").checked = gMsgFolder.charsetOverride;

    // set check for new mail checkbox
    document.getElementById("folderCheckForNewMessages").checked = gMsgFolder.flags & MSG_FOLDER_FLAG_CHECK_NEW;
  }

  if (gServerTypeFolder == "imap")
  {
    var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
    if (imapFolder)
      imapFolder.fillInFolderProps(gFolderPropsSink);

  }

  var retentionSettings = gMsgFolder.retentionSettings;
  initCommonRetentionSettings(retentionSettings);
  document.getElementById("retention.useDefault").checked = retentionSettings.useServerDefaults;

  // select the initial tab
  if (window.arguments[0].tabID) {
    try {
      document.getElementById("folderPropTabBox").selectedTab =
                           document.getElementById(window.arguments[0].tabID);
    }
    catch (ex) {}
  }
  hideShowControls(gServerTypeFolder);
  onCheckKeepMsg();
  onUseDefaultRetentionSettings();

  moveToAlertPosition();
}

function hideShowControls(serverType)
{
  var controls = document.getElementsByAttribute("hidable", "true");
  var len = controls.length;
  for (var i=0; i<len; i++) {
    var control = controls[i];
    var hideFor = control.getAttribute("hidefor");
    if (!hideFor)
      throw "this should not happen, things that are hidable should have hidefor set";

    var box = getEnclosingContainer(control);

    if (!box)
      throw "this should not happen, things that are hidable should be in a box";

    // hide unsupported server type
    // adding support for hiding multiple server types using hideFor="server1,server2"
    var hideForBool = false;
    var hideForTokens = hideFor.split(",");
    for (var j = 0; j < hideForTokens.length; j++) {
      if (hideForTokens[j] == serverType) {
        hideForBool = true;
        break;
      }
    }

    if (hideForBool) {
      box.setAttribute("hidden", "true");
    }
    else {
      box.removeAttribute("hidden");
    }
  }
  // hide the priviliges button if the imap folder doesn't have an admin url
  // mabye should leave this hidden by default and only show it in this case instead
  try {
    var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
    if (imapFolder)
    {
      var privilegesButton = document.getElementById("imap.FolderPrivileges");
      if (privilegesButton)
      {
        if (!imapFolder.hasAdminUrl)
          privilegesButton.setAttribute("hidden", "true");
      }
    }
  }
  catch (ex) {}

  // hide "check for new mail" checkbox if this is inbox
  if (gMsgFolder)
  {
    if (gMsgFolder.flags & MSG_FOLDER_FLAG_INBOX)
    {
      document.getElementById("folderCheckForNewMessages").setAttribute("hidden", "true");
    }
  }

}

function getEnclosingContainer(startNode) 
{
  var parent = startNode;
  var box; 
  while (parent && parent != document) 
  {
    var isContainer = (parent.getAttribute("iscontrolcontainer") == "true");

    // remember the FIRST container we encounter, or the first controlcontainer
    if (!box || isContainer) box=parent;
        
    // break out with a controlcontainer
    if (isContainer) break;
      parent = parent.parentNode;
  }
  return box;
}

function onOfflineFolderDownload()
{
  // we need to create a progress window and pass that in as the second parameter here.
  gMsgFolder.downloadAllForOffline(null, gParentMsgWindow);
}

function onFolderPrivileges()
{
  var imapFolder = gMsgFolder.QueryInterface(Components.interfaces.nsIMsgImapMailFolder);
  if (imapFolder)
    imapFolder.folderPrivileges(gParentMsgWindow);
  // let's try closing the modal dialog to see if it fixes the various problems running this url
  window.close(); 
}


function onUseDefaultRetentionSettings()
{
  var useDefault = document.getElementById("retention.useDefault").checked;
  document.getElementById('retention.keepMsg').disabled = useDefault;
  document.getElementById('retention.keepNewMsgMinLabel').disabled = useDefault;
  document.getElementById('retention.keepOldMsgMinLabel').disabled = useDefault;
}

function RebuildSummaryInformation()
{
  gRebuildSummaryFileCallback(gMsgFolder);
}
