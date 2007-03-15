/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Mail Code.
 *
 * The Initial Developer of the Original Code is
 * David Bienvenu.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@mozilla.org>
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

var gIdentity = null;  // the identity we are editing (may be null for a new identity)
var gAccount = null;   // the account the identity is (or will be) associated with

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function onLoadIdentityProperties()
{
  // extract the account
  gIdentity = window.arguments[0].identity;
  gAccount = window.arguments[0].account;

  initIdentityValues(gIdentity);
  initCopiesAndFolder(gIdentity);
  initCompositionAndAddressing(gIdentity);
  loadSMTPServerList();

  // the multiple identities editor isn't an account wizard panel so we have to do this ourselves:
  document.getElementById('identity.smtpServerKey').value = gIdentity ? gIdentity.smtpServerKey 
                                                            : gAccount.defaultIdentity.smtpServerKey;
}

// based on the values of gIdentity, initialize the identity fields we expose to the user
function initIdentityValues(identity)
{
  if (identity)
  {
    document.getElementById('identity.fullName').value = identity.fullName;
    document.getElementById('identity.email').value = identity.email;
    document.getElementById('identity.replyTo').value = identity.replyTo;
    document.getElementById('identity.organization').value = identity.organization;
    document.getElementById('identity.attachSignature').checked = identity.attachSignature;

    if (identity.signature)
      document.getElementById('identity.signature').value = identity.signature.path;

    document.getElementById('identity.attachVCard').checked = identity.attachVCard;
    document.getElementById('identity.escapedVCard').value = identity.escapedVCard;
  }

  setupSignatureItems();
}

function initCopiesAndFolder(identity)
{
  // if we are editing an existing identity, use it...otherwise copy our values from the default identity
  var copiesAndFoldersIdentity = identity ? identity : gAccount.defaultIdentity; 

  document.getElementById('identity.fccFolder').value = copiesAndFoldersIdentity.fccFolder;
  document.getElementById('identity.draftFolder').value = copiesAndFoldersIdentity.draftFolder;
  document.getElementById('identity.stationeryFolder').value = copiesAndFoldersIdentity.stationeryFolder;

  document.getElementById('identity.fccFolderPickerMode').value = copiesAndFoldersIdentity.fccFolderPickerMode ? copiesAndFoldersIdentity.fccFolderPickerMode : 0;  
  document.getElementById('identity.draftsFolderPickerMode').value = copiesAndFoldersIdentity.draftsFolderPickerMode ? copiesAndFoldersIdentity.draftsFolderPickerMode : 0;
  document.getElementById('identity.tmplFolderPickerMode').value = copiesAndFoldersIdentity.tmplFolderPickerMode ? copiesAndFoldersIdentity.tmplFolderPickerMode : 0;

  document.getElementById('identity.doBcc').checked = copiesAndFoldersIdentity.doBcc;
  document.getElementById('identity.doBccList').value = copiesAndFoldersIdentity.doBccList;
  document.getElementById('identity.doFcc').checked = copiesAndFoldersIdentity.doFcc;
  document.getElementById('identity.fccReplyFollowsParent').checked = copiesAndFoldersIdentity.fccReplyFollowsParent;
  document.getElementById('identity.showSaveMsgDlg').checked = copiesAndFoldersIdentity.showSaveMsgDlg;
  onInitCopiesAndFolders(); // am-copies.js method
}

function initCompositionAndAddressing(identity)
{
  createDirectoriesList();

  // if we are editing an existing identity, use it...otherwise copy our values from the default identity
  var addressingIdentity = identity ? identity : gAccount.defaultIdentity;

  document.getElementById('identity.directoryServer').value = addressingIdentity.directoryServer;
  document.getElementById('identity.overrideGlobalPref').value = addressingIdentity.overrideGlobalPref;
  document.getElementById('identity.composeHtml').checked = addressingIdentity.composeHtml;
  document.getElementById('identity.autoQuote').checked = addressingIdentity.autoQuote;
  document.getElementById('identity.replyOnTop').value = addressingIdentity.replyOnTop;
  document.getElementById('identity.sig_bottom').value = addressingIdentity.sigBottom;

  onInitCompositionAndAddressing(); // am-addressing.js method
}

function onOk()
{
  if (!validEmailAddress())
    return false;

  // if we are adding a new identity, create an identity, set the fields and add it to the
  // account. 
  if (!gIdentity)
  {
    // ask the account manager to create a new identity for us
    var accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"]
        .getService(Components.interfaces.nsIMsgAccountManager);
    gIdentity = accountManager.createIdentity();
    
    // copy in the default identity settings so we inherit lots of stuff like the defaul drafts folder, etc.
    gIdentity.copy(gAccount.defaultIdentity);

    // assume the identity is valid by default?
    gIdentity.valid = true;

    // add the identity to the account
    gAccount.addIdentity(gIdentity);

    // now fall through to saveFields which will save our new values        
  }

  // if we are modifying an existing identity, save the fields
  saveIdentitySettings(gIdentity);
  saveCopiesAndFolderSettings(gIdentity);
  saveAddressingAndCompositionSettings(gIdentity);

  window.arguments[0].result = true;

  return true;
}

// returns false and prompts the user if
// the identity does not have an email address
function validEmailAddress()
{
  var emailAddress = document.getElementById('identity.email').value;
  
  // quickly test for an @ sign to test for an email address. We don't have
  // to be anymore precise than that.
  if (emailAddress.lastIndexOf("@") < 0)
  {
    // alert user about an invalid email address

    var prefBundle = document.getElementById("bundle_prefs");

    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                        .getService(Components.interfaces.nsIPromptService);
    promptService.alert(window, prefBundle.getString("identity-edit-req-title"), 
                        prefBundle.getString("identity-edit-req"));
    return false;
  }

  return true;
}

function saveIdentitySettings(identity)
{
  if (identity)
  {
    identity.fullName = document.getElementById('identity.fullName').value;
    identity.email = document.getElementById('identity.email').value;
    identity.replyTo = document.getElementById('identity.replyTo').value;
    identity.organization = document.getElementById('identity.organization').value;
    identity.attachSignature = document.getElementById('identity.attachSignature').checked;

    identity.attachVCard = document.getElementById('identity.attachVCard').checked;
    identity.escapedVCard = document.getElementById('identity.escapedVCard').value;
    identity.smtpServerKey = document.getElementById('identity.smtpServerKey').value;
    
    var attachSignaturePath = document.getElementById('identity.signature').value;
    if (attachSignaturePath)
    {
      // convert signature path back into a nsIFile
      var sfile = Components.classes["@mozilla.org/file/local;1"]
                  .createInstance(Components.interfaces.nsILocalFile);
      sfile.initWithPath(attachSignaturePath);
      identity.signature = sfile;
    }
    else
      identity.signature = null; // this is important so we don't accidentally inherit the default
  }
}

function saveCopiesAndFolderSettings(identity)
{
  onSaveCopiesAndFolders(); // am-copies.js routine

  identity.fccFolder =  document.getElementById('identity.fccFolder').value;
  identity.draftFolder = document.getElementById('identity.draftFolder').value;
  identity.stationeryFolder = document.getElementById('identity.stationeryFolder').value;
  identity.fccFolderPickerMode = document.getElementById('identity.fccFolderPickerMode').value;
  identity.draftsFolderPickerMode = document.getElementById('identity.draftsFolderPickerMode').value;
  identity.tmplFolderPickerMode = document.getElementById('identity.tmplFolderPickerMode').value;
  identity.doBcc = document.getElementById('identity.doBcc').checked;
  identity.doBccList = document.getElementById('identity.doBccList').value;
  identity.doFcc = document.getElementById('identity.doFcc').checked;
  identity.fccReplyFollowsParent = document.getElementById('identity.fccReplyFollowsParent').checked;
  identity.showSaveMsgDlg = document.getElementById('identity.showSaveMsgDlg').checked;
}

function saveAddressingAndCompositionSettings(identity)
{
  onSaveCompositionAndAddressing(); // am-addressing.js routine

  identity.directoryServer = document.getElementById('identity.directoryServer').value;
  identity.overrideGlobalPref = document.getElementById('identity.overrideGlobalPref').value;
  identity.composeHtml = document.getElementById('identity.composeHtml').checked;
  identity.autoQuote = document.getElementById('identity.autoQuote').checked;
  identity.replyOnTop = document.getElementById('identity.replyOnTop').value;
  identity.sigBottom = document.getElementById('identity.sig_bottom').value == 'true';  
}

function selectFile()
{
  var fp = Components.classes["@mozilla.org/filepicker;1"]
                     .createInstance(nsIFilePicker);

  var prefBundle = document.getElementById("bundle_prefs");
  var title = prefBundle.getString("choosefile");
  fp.init(window, title, nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll);

  // Get current signature folder, if there is one.
  // We can set that to be the initial folder so that users 
  // can maintain their signatures better.
  var sigFolder = GetSigFolder();
  if (sigFolder)
      fp.displayDirectory = sigFolder;

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
      var folderField = document.getElementById("identity.signature");
      folderField.value = fp.file.path;
  }
}

function GetSigFolder()
{
  var sigFolder = null;
  try 
  {
    var account = parent.getCurrentAccount();
    var identity = account.defaultIdentity;
    var signatureFile = identity.signature;

    if (signatureFile) 
    {
      signatureFile = signatureFile.QueryInterface( Components.interfaces.nsILocalFile );
      sigFolder = signatureFile.parent;

      if (!sigFolder.exists) 
          sigFolder = null;
    }
  }
  catch (ex) {
      dump("failed to get signature folder..\n");
  }
  return sigFolder;
}

// If a signature is need to be attached, the associated items which
// displays the absolute path to the signature (in a textbox) and the way
// to select a new signature file (a button) are enabled. Otherwise, they
// are disabled. Check to see if the attachSignature is locked to block
// broadcasting events.
function setupSignatureItems()
{ 
  var signature = document.getElementById("identity.signature");
  var browse = document.getElementById("identity.sigbrowsebutton");
  var attachSignature = document.getElementById("identity.attachSignature");
  var checked = attachSignature.checked;

  if (checked && !getAccountValueIsLocked(signature))
    signature.removeAttribute("disabled");
  else
    signature.setAttribute("disabled", "true");

  if (checked && !getAccountValueIsLocked(browse))
    browse.removeAttribute("disabled");
  else
    browse.setAttribute("disabled", "true"); 
}

function editVCardCallback(escapedVCardStr)
{
  var escapedVCard = document.getElementById("identity.escapedVCard");
  escapedVCard.value = escapedVCardStr;
}

function editVCard()
{
  var escapedVCard = document.getElementById("identity.escapedVCard");

  // read vCard hidden value from UI
  window.openDialog("chrome://messenger/content/addressbook/abNewCardDialog.xul",
                    "",
                    "chrome,resizable=no,titlebar,modal",
                    {escapedVCardStr:escapedVCard.value, okCallback:editVCardCallback,
                     titleProperty:"editVCardTitle", hideABPicker:true});
}

function getAccountForFolderPickerState()
{ 
  return gAccount;
}

// when the identity panel is loaded, the smpt-list is created
// and the in prefs.js configured smtp is activated
function loadSMTPServerList()
{
  var smtpService = Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);
  fillSmtpServers(document.getElementById('identity.smtpServerKey'), smtpService.smtpServers, smtpService.defaultServer);
}

function fillSmtpServers(smtpServerList, servers, defaultServer)
{
  if (!smtpServerList || !servers) 
    return;

  var smtpPopup = document.getElementById('smtpPopup');
  while (smtpPopup.lastChild.nodeName != "menuseparator")
    smtpPopup.removeChild(smtpPopup.lastChild);

  var serverCount = servers.Count();
  for (var i = 0; i < serverCount; i++) 
  {
    var server = servers.QueryElementAt(i, Components.interfaces.nsISmtpServer);
    //ToDoList: add code that allows for the redirector type to specify whether to show values or not
    if (!server.redirectorType)
    {
      var serverName = "";
      if (server.description)
        serverName = server.description + ' - ';
      else if (server.username)
        serverName = server.username + ' - ';    
      serverName += server.hostname;

      if (defaultServer.key == server.key)
        serverName += " " + document.getElementById("bundle_messenger").getString("defaultServerTag");

      smtpServerList.appendItem(serverName, server.key);
    }
  }
}
