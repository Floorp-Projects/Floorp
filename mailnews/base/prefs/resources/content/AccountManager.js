/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * here's how this dialog works:
 * The main dialog contains a tree on the left (accounttree) and a
 * deck on the right. Each card in the deck on the right contains an
 * IFRAME which loads a particular preference document (such as am-main.xul)
 *
 * when the user clicks on items in the tree on the right, two things have
 * to be determined before the UI can be updated:
 * - the relevant account
 * - the relevant page
 *
 * when both of these are known, this is what happens:
 * - every form element of the previous page is saved in the account value
 *   hashtable for the previous account
 * - the card containing the relevant page is brought to the front
 * - each form element in the page is filled in with an appropriate value
 *   from the current account's hashtable
 * - in the IFRAME inside the page, if there is an onInit() method,
 *   it is called. The onInit method can further update this page based
 *   on values set in the previous step.
 */

var gSmtpHostNameIsIllegal = false;
var accountArray;
var gGenericAttributeTypes;
var accounttree;

var currentServerId;
var currentPageId;

var pendingServerId;
var pendingPageId;
var gPrefsBundle;
var gBrandBundle;

// services used
var RDF;
var accountManager;
var smtpService;
var nsPrefBranch;
var gPromptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(Components.interfaces.nsIPromptService);

// widgets
var duplicateButton;
var removeButton;
var addAccountButton;
var setDefaultButton;

// This sets an attribute in a xul element so that we can later
// know what value to substitute in a prefstring.  Different
// preference types set different attributes.  We get the value
// in the same way as the function getAccountValue() determines it.
function updateElementWithKeys(account, element, type) {
  switch (type)
  {
    case "identity":
      element["identitykey"] = account.defaultIdentity.key;
      break;
    case "pop3":
    case "imap":
    case "nntp":
    case "server":
      element["serverkey"] = account.incomingServer.key;
      break;
    case "smtp":
      if (smtpService.defaultServer)
        element["serverkey"] = smtpService.defaultServer.key;
      break;
    default:
//      dump("unknown element type! "+type+"\n");
  }
}

function hideShowControls(serverType) {
  var controls = document.getElementsByAttribute("hidefor", "*");
  for (var controlNo = 0; controlNo < controls.length; controlNo++) {
    var control = controls[controlNo];
    var hideFor = control.getAttribute("hidefor");

    // Hide unsupported server types using hideFor="servertype1,servertype2".
    var hide = false;
    var hideForTokens = hideFor.split(",");
    for (var tokenNo = 0; tokenNo < hideForTokens.length; tokenNo++) {
      if (hideForTokens[tokenNo] == serverType) {
        hide = true;
        break;
      }
    }

    if (hide)
      control.setAttribute("hidden", "true");
    else
      control.removeAttribute("hidden");
  }
}

// called when the whole document loads
// perform initialization here
function onLoad() {
  gPrefsBundle = document.getElementById("bundle_prefs");
  gBrandBundle = document.getElementById("bundle_brand");

  var selectedServer;
  var selectPage = null;
  if ("arguments" in window && window.arguments[0]) {
    selectedServer = window.arguments[0].server;
    selectPage = window.arguments[0].selectPage;
  }

  accountArray = new Array;
  gGenericAttributeTypes = new Array;

  RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].getService(Components.interfaces.nsIRDFService);

  accountManager = Components.classes["@mozilla.org/messenger/account-manager;1"].getService(Components.interfaces.nsIMsgAccountManager);

  smtpService =
    Components.classes["@mozilla.org/messengercompose/smtp;1"].getService(Components.interfaces.nsISmtpService);
  accounttree = document.getElementById("accounttree");

  var prefService = Components.classes["@mozilla.org/preferences-service;1"];
  prefService = prefService.getService();
  prefService=prefService.QueryInterface(Components.interfaces.nsIPrefService);
  nsPrefBranch = prefService.getBranch(null);

  addAccountButton = document.getElementById("addAccountButton");
  duplicateButton = document.getElementById("duplicateButton");
  removeButton = document.getElementById("removeButton");
  setDefaultButton = document.getElementById("setDefaultButton");

  sortAccountList(accounttree);
  setTimeout(selectServer, 0, selectedServer.serverURI, selectPage);
}

function sortAccountList(accounttree)
{
  var xulSortService = Components.classes["@mozilla.org/xul/xul-sort-service;1"].getService(Components.interfaces.nsIXULSortService);
  xulSortService.sort(accounttree, 'http://home.netscape.com/NC-rdf#FolderTreeName?sort=true', 'ascending');
}

function selectServer(serverId, selectPage)
{
  var selectedServer, selectedItem;

  if (serverId)
    selectedServer = document.getElementById(serverId);
  if (!selectedServer)
    selectedServer = getFirstAccount();

  if (serverId && selectedServer && selectPage)
    selectedItem = findSelectPage(selectedServer, selectPage);
  if (!selectedItem)
    selectedItem = selectedServer;

  var index = accounttree.contentView.getIndexOfItem(selectedItem);
  accounttree.view.selection.select(index);
  accounttree.treeBoxObject.ensureRowIsVisible(index);

  var lastItem = selectedServer.lastChild.lastChild;
  if (lastItem.localName == "treeitem")
    index = accounttree.contentView.getIndexOfItem(lastItem);

  accounttree.treeBoxObject.ensureRowIsVisible(index);
}

function findSelectPage(selectServer, selectPage)
{
  var children = selectServer.childNodes;
  for (i=0; i < children.length; i++) 
  { 
    if (children[i].localName == "treechildren") {
      var pageNodes = children[i].childNodes;
      for (j=0; j < pageNodes.length; j++) {
        if (pageNodes[j].localName == "treeitem") {          
          var page = pageNodes[j].getAttribute('PageTag');
          if (page == selectPage) {
            return pageNodes[j];
          }
        }
      }
    }
  }
  return null;
}

function getFirstAccount()
{
  var tree = document.getElementById("accounttree");
  var firstItem = findFirstTreeItem(tree);

  return firstItem;
}

function findFirstTreeItem(tree) {
  var children = tree.childNodes;

  var treechildren;
  for (var i=0;i<children.length; i++) {
    if (children[i].localName == "treechildren") {
      treechildren = children[i];
      break;
    }
  }

  children = treechildren.childNodes;
  for (i=0; i<children.length; i++) {
    if (children[i].localName == "treeitem")
      return children[i];
  }
  return null;
}

function replaceWithDefaultSmtpServer(deletedSmtpServerKey)
{
  //First we replace the smtpserverkey in every identity

  var allIdentities = accountManager.allIdentities;
  var identitiesCount = allIdentities.Count();
  for (var i = 0; i < identitiesCount; i++) {
    var currentIdentity = allIdentities.QueryElementAt(i, Components.interfaces.nsIMsgIdentity);
    if (currentIdentity.smtpServerKey == deletedSmtpServerKey)
      currentIdentity.smtpServerKey = smtpService.defaultServer.key;
  }

  //When accounts have already been loaded in the panel then the first replacement will be 
  //overwritten when the accountvalues are written out from the pagedata.
  //we get the loaded accounts and check to make sure that the account exists for the accountid
  //and that it has a default identity associated with it (to exclude smtpservers and local folders)
  //Then we check only for the identity[type] and smtpServerKey[slot]
  //and replace that with the default smtpserverkey if necessary.

  for (var accountid in accountArray) {
    var account = getAccountFromServerId(accountid);
    if(account && account.defaultIdentity) {
      var accountValues = accountArray[accountid];
      if (accountValues['identity']['smtpServerKey'] == deletedSmtpServerKey)
        setAccountValue(accountValues,'identity', 'smtpServerKey', smtpService.defaultServer.key);
    }
  }
}

function onAccept() {
  // Check if user/host have been modified.
  if (!checkUserServerChanges(true))
    return false;

  if (gSmtpHostNameIsIllegal) {
    gSmtpHostNameIsIllegal = false;
    return false;
  }

  onSave();
  // hack hack - save the prefs file NOW in case we crash
  try {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefService);
    prefs.savePrefFile(null);
  } catch (ex) {
    dump("Error saving prefs!\n");
  }
  return true;
}

// Check if the user and/or host names have been changed and
// if so check if the new names already exists for an account.
//
function checkUserServerChanges(showAlert) {

  if (smtpService.defaultServer) {
    try {
      var smtpHostName = top.frames["contentFrame"].document.getElementById("smtp.hostname");
      if (hostnameIsIllegal(smtpHostName.value)) {
        var alertTitle = gBrandBundle.getString("brandShortName");
        var alertMsg = gPrefsBundle.getString("enterValidHostname");

        if (gPromptService)
          gPromptService.alert(window, alertTitle, alertMsg);
        else
          window.alert(alertMsg);
       gSmtpHostNameIsIllegal = true;
      }
    }
    catch (ex) {}
  }

  var accountValues = getValueArrayFor(currentServerId);
  if (!accountValues) 
    return true;
  var pageElements = getPageFormElements();

  if (pageElements == null) return true;

  // Get the new username, hostname and type from the page
  var newUser, newHost, newType, oldUser, oldHost;
  var uIndx, hIndx;
  for (var i=0; i<pageElements.length; i++) {
    if (pageElements[i].id) {
      var vals = pageElements[i].id.split(".");
      if (vals.length >= 2) {
        var type = vals[0];
        var slot = vals[1];
        //dump("In checkUserServerChanges() ***: accountValues[" + type + "][" + slot + "] = " + getFormElementValue(pageElements[i]) + "/" + accountValues[type][slot] + "\n");

        // if this type doesn't exist (just removed) then return.
        if (!(type in accountValues) || !accountValues[type]) return true;

        if (slot == "realHostName") {
          oldHost = accountValues[type][slot];
          newHost = getFormElementValue(pageElements[i]);
          hIndx = i;
        }
        else if (slot == "realUsername") {
          oldUser = accountValues[type][slot];
          newUser = getFormElementValue(pageElements[i]);
          uIndx = i;
        }
        else if (slot == "type")
          newType = getFormElementValue(pageElements[i]);
      }
    }
  }

  // There is no username defined for news so reset it.
  if (newType == "nntp")
    oldUser = newUser = "";

  //dump("In checkUserServerChanges() *** Username = " + newUser + "/" + oldUser + "\n");
  //dump("In checkUserServerChanges() *** Hostname = " + newHost + "/" + oldHost + "\n");
  //dump("In checkUserServerChanges() *** Type     = " + newType + "\n");

  // If something is changed then check if the new user/host already exists.
  if ( (oldUser != newUser) || (oldHost != newHost) ) {
    var newServer = accountManager.findRealServer(newUser, newHost, newType, 0);
    if (newServer) {
      if (showAlert) {
        var alertText = gPrefsBundle.getString("modifiedAccountExists");
        window.alert(alertText);
      }
      // Restore the old values before return
      if (newType != "nntp")
        setFormElementValue(pageElements[uIndx], oldUser);
      setFormElementValue(pageElements[hIndx], oldHost);
      return false;
    }

    //dump("In checkUserServerChanges() Ah, Server not found !!!" + "\n");
    // If username is changed remind users to change Your Name and Email Address.
    // If serve name is changed and has defined filters then remind users to edit rules.
    if (showAlert) {
      var account = getAccountFromServerId(currentServerId);
      var filterList;
      if (account && (newType != "nntp")) {
        var server = account.incomingServer;
        filterList = server.getFilterList(null);
      }
      var userChangeText, serverChangeText;
      if ( (oldHost != newHost) && (filterList != undefined) && filterList.filterCount )
        serverChangeText = gPrefsBundle.getString("serverNameChanged");
      if (oldUser != newUser)
        userChangeText = gPrefsBundle.getString("userNameChanged");

      if ( (serverChangeText != undefined) && (userChangeText != undefined) )
        serverChangeText = serverChangeText + "\n\n" + userChangeText;
      else
      if (userChangeText != undefined)
        serverChangeText = userChangeText;

      if (serverChangeText != undefined)
        window.alert(serverChangeText);
    }
  }
  return true;
}

function onSave() {
  if (pendingPageId) {
    dump("ERROR: " + pendingPageId + " hasn't loaded yet! Not saving.\n");
    return;
  }

  // make sure the current visible page is saved
  savePage(currentServerId);

  for (var accountid in accountArray) {
    var account = getAccountFromServerId(accountid);
    var accountValues = accountArray[accountid];

    if (accountValues)
      saveAccount(accountValues, account);
  }
}

function onAddAccount() {
  MsgAccountWizard();
}

function ReloadSmtpPanel()
{
  var smtpUsername = top.frames["contentFrame"].document.getElementById("smtp.username");
  var smtpHostname = top.frames["contentFrame"].document.getElementById("smtp.hostname");
  var smtpPort = top.frames["contentFrame"].document.getElementById("smtp.port");
  var smtpUseUsername = top.frames["contentFrame"].document.getElementById("smtp.useUsername");
  var smtpAuthMethod = top.frames["contentFrame"].document.getElementById("smtp.authMethod");
  var smtpTrySSL = top.frames["contentFrame"].document.getElementById("smtp.trySSL");

  var defaultServer = smtpService.defaultServer;

  smtpUsername.value = defaultServer.username;
  smtpHostname.value = defaultServer.hostname;
  smtpPort.value = defaultServer.port ? defaultServer.port : "";
  smtpAuthMethod.setAttribute("value", defaultServer.authMethod);
  if (smtpAuthMethod.getAttribute("value") == "1")
    smtpUseUsername.checked = true;
  var elements = smtpTrySSL.getElementsByAttribute("value", defaultServer.trySSL);
  if (!elements.item(0))
    elements = smtpTrySSL.getElementsByAttribute("value", "1");
  smtpTrySSL.selectedItem = elements[0];
}

function onDuplicateAccount() {
  //dump("onDuplicateAccount\n");

  if (duplicateButton.getAttribute("disabled") == "true") return;

  var result = getServerIdAndPageIdFromTree(accounttree);
  if (result) {
    var canDuplicate = true;
    var account = getAccountFromServerId(result.serverId);
    if (account) {
      var server = account.incomingServer;
      var type = server.type;

      var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);
      canDuplicate = protocolinfo.canDuplicate;
    }
    else {
      canDuplicate = false;
    }

    if (canDuplicate) {
      try {
        accountManager.duplicateAccount(account);
      }
      catch (ex) {
        var alertText = gPrefsBundle.getString("failedDuplicateAccount");
        window.alert(alertText);
      }
    }
  }
}

function onSetDefault(event) {
  if (event.target.getAttribute("disabled") == "true") return;

  var result = getServerIdAndPageIdFromTree(accounttree);
  if (!result) return;

  var account = getAccountFromServerId(result.serverId);
  if (!account) return;

  accountManager.defaultAccount = account;
  setEnabled(setDefaultButton, false);
}

function onRemoveAccount(event) {
  //dump("onRemoveAccount\n");

  if (event.target.getAttribute("disabled") == "true") return;

  var result = getServerIdAndPageIdFromTree(accounttree);
  if (!result) 
    return;

  if (result.serverId == "http://home.netscape.com/NC-rdf#PageTitleFakeAccount") {
    nsPrefBranch.setBoolPref("mailnews.fakeaccount.show", false);

    // select the default account
    selectServer(null, null); 
  }

  var account = getAccountFromServerId(result.serverId);
  if (!account) 
    return;

  var server = account.incomingServer;
  var type = server.type;
  var prettyName = server.prettyName;

  var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);
  var canDelete = protocolinfo.canDelete;
  if (!canDelete) {
    canDelete = server.canDelete;
  }
  if (!canDelete) 
    return;

  var confirmRemoveAccount =
    gPrefsBundle.getFormattedString("confirmRemoveAccount", [prettyName]);

  var confirmTitle = gPrefsBundle.getString("confirmRemoveAccountTitle");

  var promptService =
    Components.classes["@mozilla.org/embedcomp/prompt-service;1"].
               getService(Components.interfaces.nsIPromptService);
  if (!promptService ||
      !promptService.confirm(window, confirmTitle, confirmRemoveAccount)) {
    return;
  }

  try {
    // clear cached data out of the account array
    if (accountArray[result.serverId])
      accountArray[result.serverId] = null;
    currentServerId = currentPageId = null;

    accountManager.removeAccount(account);
    selectServer(null, null);
  }
  catch (ex) {
    dump("failure to remove account: " + ex + "\n");
    var alertText = gPrefsBundle.getString("failedRemoveAccount");
    window.alert(alertText);
  }
}

function saveAccount(accountValues, account)
{
  var identity = null;
  var server = null;

  if (account) {
    identity = account.defaultIdentity;
    server = account.incomingServer;
  }

  for (var type in accountValues) {
    var typeArray = accountValues[type];

    for (var slot in typeArray) {
      var dest;
      try {
      if (type == "identity")
        dest = identity;
      else if (type == "server")
        dest = server;
      else if (type == "pop3")
        dest = server.QueryInterface(Components.interfaces.nsIPop3IncomingServer);
      else if (type == "imap")
        dest = server.QueryInterface(Components.interfaces.nsIImapIncomingServer);
      else if (type == "none")
        dest = server.QueryInterface(Components.interfaces.nsINoIncomingServer);
      else if (type == "nntp")
        dest = server.QueryInterface(Components.interfaces.nsINntpIncomingServer);
      else if (type == "smtp")
        dest = smtpService.defaultServer;

      } catch (ex) {
        // don't do anything, just means we don't support that
      }
      if (dest == undefined) continue;

      if ((type in gGenericAttributeTypes) && (slot in gGenericAttributeTypes[type])) {
        switch (gGenericAttributeTypes[type][slot]) {
          case "int":
            if (dest.getIntAttribute(slot) != typeArray[slot])
              dest.setIntAttribute(slot, typeArray[slot]);
            break;
          case "wstring":
            if (dest.getUnicharAttribute(slot) != typeArray[slot])
              dest.setUnicharAttribute(slot, typeArray[slot]);
            break;
          case "string":
            if (dest.getCharAttribute(slot) != typeArray[slot])
              dest.setCharAttribute(slot, typeArray[slot]);
            break;
          case "bool":
            // in some cases
            // like for radiogroups of type boolean
            // the value will be "false" instead of false
            // we need to convert it.
            if (typeArray[slot] == "false")
              typeArray[slot] = false;
            else if (typeArray[slot] == "true")
              typeArray[slot] = true;

            if (dest.getBoolAttribute(slot) != typeArray[slot])
              dest.setBoolAttribute(slot, typeArray[slot]);
            break;
          default:
            dump("unexpected preftype: " + preftype + "\n");
            break;
        }
      }
      else {
      if (slot in dest && dest[slot] != typeArray[slot]) {
        try {
          dest[slot] = typeArray[slot];
          } 
          catch (ex) {
          // hrm... need to handle special types here
        }
      }
    }
  }
}
}

function updateButtons(tree,serverId) {
  var canCreate = true;
  var canDuplicate = true;
  var canDelete = true;
  var canSetDefault = true;

  //dump("updateButtons\n");
  //dump("serverId = " + serverId + "\n");
  var account = getAccountFromServerId(serverId);
  //dump("account = " + account + "\n");

  if (account) {
    var server = account.incomingServer;
    var type = server.type;

    if (account == accountManager.defaultAccount ||
        !server.canBeDefaultServer ||
        account.identities.Count() < 1)
      canSetDefault = false;

    //dump("servertype = " + type + "\n");

    var protocolinfo = Components.classes["@mozilla.org/messenger/protocol/info;1?type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);
    canDuplicate = protocolinfo.canDuplicate;
    canDelete = protocolinfo.canDelete;
    if (!canDelete) {
      canDelete = server.canDelete;
    }
  }
  else {
    // HACK
    // if account is null, we have either selected a SMTP server, or there is a problem
    // either way, we don't want the user to be able to delete it or duplicate it

    canSetDefault = false;
    canDelete = (serverId == "http://home.netscape.com/NC-rdf#PageTitleFakeAccount");
    canDuplicate = false;
  }

  if (tree.view.selection.count < 1)
    canDuplicate = canSetDefault = canDelete = false;

  // check for disabled preferences on the account buttons.  
  //  Not currently handled by WSM or the main loop yet since these buttons aren't
  //  under the IFRAME
  if (nsPrefBranch.prefIsLocked(addAccountButton.getAttribute("prefstring")))
    canCreate = false;
  if (nsPrefBranch.prefIsLocked(setDefaultButton.getAttribute("prefstring")))
    canSetDefault = false;
  if (nsPrefBranch.prefIsLocked(removeButton.getAttribute("prefstring")))
    canDelete = false;

  setEnabled(addAccountButton, canCreate);
  setEnabled(duplicateButton, canDuplicate);
  setEnabled(setDefaultButton, canSetDefault);
  setEnabled(removeButton, canDelete);

}

function setEnabled(control, enabled)
{
  if (!control) return;
  if (enabled)
    control.removeAttribute("disabled");
  else
    control.setAttribute("disabled", true);
}

//
// Called when someone clicks on an account. Figure out context by what they
// clicked on. This is also called when an account is removed. In this case,
// nothing is selected.
//
function onAccountClick(tree)
{
  var currentSelection = getServerIdAndPageIdFromTree(tree);

  // Nothing is selected. This is the case when an account was removed.
  // A call of selectServer(null,null) after the removal ensures that
  // onAccountClick() is called again after that with a valid selection.
  if (!currentSelection)
    return;

  if (showPage(currentSelection.serverId, currentSelection.pageId))
    updateButtons(tree, currentSelection.serverId);
}

// show the page for the given server:
// - save the old values
// - start loading the new page
function showPage(serverId, pageId) 
{
  if (pageId == currentPageId &&
      serverId == currentServerId)
    return false;

  // check if user/host names have been changed
  checkUserServerChanges(false);

  if (gSmtpHostNameIsIllegal) {
    gSmtpHostNameIsIllegal = false;
    selectServer(currentServerId, currentPageId);
    return false;
  }

  // save the previous page
  savePage(currentServerId);

  var changeServerId = (serverId != currentServerId);
  // loading a complete different page
  if (pageId != currentPageId) {

    // prevent overwriting with bad stuff
    currentServerId = currentPageId = null;

    pendingServerId=serverId;
    pendingPageId=pageId;
    loadPage(pageId);
  }

  // same page, different server
  else if (changeServerId) {
    restorePage(pageId, serverId);
  }
  return changeServerId;
}

// page has loaded
function onPanelLoaded(pageId) {
  if (pageId != pendingPageId) {

    // if we're reloading the current page, we'll assume the
    // page has asked itself to be completely reloaded from
    // the prefs. to do this, clear out the the old entry in
    // the account data, and then restore theh page
    if (pageId == currentPageId) {
      clearAccountData(currentServerId, currentPageId);
      restorePage(currentPageId, currentServerId);
    }
  } else {

    restorePage(pendingPageId, pendingServerId);
  }

  // probably unnecessary, but useful for debugging
  pendingServerId = null;
  pendingPageId = null;
}

function loadPage(pageId)
{
  var chromePackageName;
  try 
  {
    // we could compare against "main","server","copies","offline","addressing",
    // "smtp","advanced", and "fakeaccount" first to save the work, but don't
    // as some of these might be turned into extensions (for thunderbird)
    chromePackageName = accountManager.getChromePackageName(pageId.split("am-")[1].split(".xul")[0]);
  }
  catch (ex) 
  {
    chromePackageName = "messenger";
  }
  const LOAD_FLAGS_NONE = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  document.getElementById("contentFrame").webNavigation.loadURI("chrome://" + chromePackageName + "/content/" + pageId, LOAD_FLAGS_NONE, null, null, null);
}

//
// save the values of the widgets to the given server
//
function savePage(serverId) 
{
  if (!serverId) 
    return;

  // tell the page that it's about to save
  if ("onSave" in top.frames["contentFrame"])
    top.frames["contentFrame"].onSave();

  var accountValues = getValueArrayFor(serverId);
  if (!accountValues) 
    return;

  var pageElements = getPageFormElements();
  if (!pageElements) 
    return;

  // store the value in the account
  for (var i=0; i<pageElements.length; i++) {
    if (pageElements[i].id) {
      var vals = pageElements[i].id.split(".");
      if (vals.length >= 2) {
        var type = vals[0];
        var slot = vals[1];

        setAccountValue(accountValues,
                        type, slot,
                        getFormElementValue(pageElements[i]));
      }
    }
  }
}

function setAccountValue(accountValues, type, slot, value) {
  if (!(type in accountValues))
    accountValues[type] = new Array;

  //dump("Form->Array: accountValues[" + type + "][" + slot + "] = " + value + "\n");

  accountValues[type][slot] = value;
}

function getAccountValue(account, accountValues, type, slot, preftype, isGeneric) {
  if (!(type in accountValues))
    accountValues[type] = new Array;

  // fill in the slot from the account if necessary
  if (!(slot in accountValues[type]) || accountValues[type][slot] == undefined) {
    // dump("Array->Form: lazily reading in the " + slot + " from the " + type + "\n");
    var server;
    if (account)
      server= account.incomingServer;
    var source = null;
    try {
    if (type == "identity")
      source = account.defaultIdentity;
    else if (type == "server")
      source = account.incomingServer;
    else if (type == "pop3")
      source = server.QueryInterface(Components.interfaces.nsIPop3IncomingServer);
    else if (type == "imap")
      source = server.QueryInterface(Components.interfaces.nsIImapIncomingServer);
    else if (type == "none")
      source = server.QueryInterface(Components.interfaces.nsINoIncomingServer);
    else if (type == "nntp")
      source = server.QueryInterface(Components.interfaces.nsINntpIncomingServer);
    else if (type == "smtp")
      source = smtpService.defaultServer;
    } catch (ex) {
    }

    if (source) {
      if (isGeneric) {
        if (!(type in gGenericAttributeTypes))
          gGenericAttributeTypes[type] = new Array;

        // we need the preftype later, for setting when we save.
        gGenericAttributeTypes[type][slot] = preftype;
        switch (preftype) {
          case "int":
            accountValues[type][slot] = source.getIntAttribute(slot);
            break;
          case "wstring":
            accountValues[type][slot] = source.getUnicharAttribute(slot);
            break;
          case "string":
            accountValues[type][slot] = source.getCharAttribute(slot);
            break;
          case "bool":
            accountValues[type][slot] = source.getBoolAttribute(slot);
            break;
          default:
            dump("unexpected preftype: " + preftype + "\n");
            break;
        }
      }
      else if (slot in source) {
        accountValues[type][slot] = source[slot];
      } else {
        accountValues[type][slot] = null;
      }
    }
    else {
      accountValues[type][slot] = null;
    }
  }
  var value = accountValues[type][slot];
  //dump("Array->Form: accountValues[" + type + "][" + slot + "] = " + value + "\n");
  return value;
}

//
// restore the values of the widgets from the given server
//
function restorePage(pageId, serverId) 
{
  if (!serverId) 
    return;

  var accountValues = getValueArrayFor(serverId);
  if (!accountValues) 
    return;

  var pageElements = getPageFormElements();
  if (!pageElements) 
    return;

  var account = getAccountFromServerId(serverId);

  if ("onPreInit" in top.frames["contentFrame"])
    top.frames["contentFrame"].onPreInit(account, accountValues);

  // restore the value from the account
  for (var i=0; i<pageElements.length; i++) {
    if (pageElements[i].id) {
      var vals = pageElements[i].id.split(".");
      if (vals.length >= 2) {
        var type = vals[0];
        var slot = vals[1];
        // buttons are lockable, but don't have any data so we skip that part.
        // elements that do have data, we get the values at poke them in.
        if (pageElements[i].localName != "button") {
          var value = getAccountValue(account, accountValues, type, slot, pageElements[i].getAttribute("preftype"), (pageElements[i].getAttribute("genericattr") == "true"));
          setFormElementValue(pageElements[i], value);
        }
        updateElementWithKeys(account,pageElements[i],type);
        var isLocked = getAccountValueIsLocked(pageElements[i]);
        setEnabled(pageElements[i],!isLocked);
      }
    }
  }

  // tell the page that new values have been loaded
  if ("onInit" in top.frames["contentFrame"])
    top.frames["contentFrame"].onInit();

  // everything has succeeded, vervied by setting currentPageId
  currentPageId = pageId;
  currentServerId = serverId;

}

//
// gets the value of a widget
//
function getFormElementValue(formElement) {
  try {
    var type = formElement.localName;
    if (type=="checkbox") {
      if (formElement.getAttribute("reversed"))
        return !formElement.checked;
      return formElement.checked;
    }
    if (type == "radiogroup" || type=="menulist") {
      return formElement.selectedItem.value;
    }
    if (type == "textbox" &&
        formElement.getAttribute("datatype") == "nsIFileSpec") {
      if (formElement.value) {
        var filespec = Components.classes["@mozilla.org/filespec;1"].createInstance(Components.interfaces.nsIFileSpec);
        filespec.unicodePath = formElement.value;
        return filespec;
      }
      return null;
    }
    if (type == "textbox" &&
        formElement.getAttribute("datatype") == "nsILocalFile") {
      if (formElement.value) {
        var localfile = Components.classes["@mozilla.org/file/local;1"].createInstance(Components.interfaces.nsILocalFile);

        localfile.initWithPath(formElement.value);
        return localfile;
      }
      return null;
    }
    if (type == "text") {
      var val = formElement.getAttribute("value");
      if (val) return val;
      return null;
    }
    if ("value" in formElement) {
      return formElement.value;
    }
    return null;
  }
  catch (ex) {
    dump("getFormElementValue failed, ex="+ex+"\n");
  }
  return null;
}

//
// sets the value of a widget
//
function setFormElementValue(formElement, value) {

  //formElement.value = formElement.defaultValue;
  //  formElement.checked = formElement.defaultChecked;
  var type = formElement.localName;
  if (type == "checkbox") {
    if (value == undefined) {
      if ("defaultChecked" in formElement && formElement.defaultChecked)
        formElement.checked = formElement.defaultChecked;
      else
        formElement.checked = false;
    } else {
      if (formElement.getAttribute("reversed"))
        formElement.checked = !value;
      else
        formElement.checked = value;
    }
  }

  else if (type == "radiogroup" || type =="menulist") {

    var selectedItem;
    if (value == undefined) {
      if (type == "radiogroup")
        selectedItem = formElement.firstChild;
      else
        selectedItem = formElement.firstChild.firstChild;
    }
    else
      selectedItem = formElement.getElementsByAttribute("value", value)[0];

    formElement.selectedItem = selectedItem;
  }
  // handle nsIFileSpec
  else if (type == "textbox" &&
           formElement.getAttribute("datatype") == "nsIFileSpec") {
    if (value) {
      var filespec = value.QueryInterface(Components.interfaces.nsIFileSpec);
      try {
        formElement.value = filespec.unicodePath;
      } catch (ex) {
        dump("Still need to fix uninitialized filespec problem!\n");
      }
    } else {
      if ("defaultValue" in formElement)
        formElement.value = formElement.defaultValue;
      else
        formElement.value = "";
    }
  }

  else if (type == "textbox" &&
           formElement.getAttribute("datatype") == "nsILocalFile") {
    if (value) {
      var localfile = value.QueryInterface(Components.interfaces.nsILocalFile);
      try {
        formElement.value = localfile.path;
      } catch (ex) {
        dump("Still need to fix uninitialized nsIFile problem!\n");
      }

    } else {
      if ("defaultValue" in formElement)
        formElement.value = formElement.defaultValue;
      else
        formElement.value = "";
    }
  }

  else if (type == "text") {
    if (value == null || value == undefined)
      formElement.removeAttribute("value");
    else
      formElement.setAttribute("value",value);
  }

  // let the form figure out what to do with it
  else {
    if (value == undefined) {
      if ("defaultValue" in formElement && formElement.defaultValue)
        formElement.value = formElement.defaultValue;
    }
    else
      formElement.value = value;
  }
}

//
// conversion routines - get data associated
// with a given pageId, serverId, etc
//

// helper routine for account manager panels to get the current account for the selected server
function getCurrentAccount()
{
  var result = getServerIdAndPageIdFromTree(accounttree);
  return getAccountFromServerId(result.serverId);
}

//
// get the account associated with this serverId
//
function getAccountFromServerId(serverId) {
  // get the account by dipping into RDF and then into the acount manager
  var serverResource = RDF.GetResource(serverId);
  try {
    var serverFolder =
      serverResource.QueryInterface(Components.interfaces.nsIMsgFolder);
    var incomingServer = serverFolder.server;
    var account = accountManager.FindAccountForServer(incomingServer);
    return account;
  } catch (ex) {
  }
  return null;
}

//
// get the array of form elements for the given page
//
function getPageFormElements() {
  try {
    if("getElementsByAttribute" in top.frames["contentFrame"].document) {
      var pageElements =
             top.frames["contentFrame"].document.getElementsByAttribute("wsm_persist", "true");
      return pageElements;
    }
  }
  catch (ex) {
    dump("getPageFormElements() failed: " + ex + "\n");
  }
  return null;
}

//
// get the value array for the given serverId
//
function getValueArrayFor(serverId) {
  if (serverId == undefined) serverId="global";

  if (!(serverId in accountArray)) {
    accountArray[serverId] = new Array;
  }

  return accountArray[serverId];
}

function clearAccountData(serverId, pageId)
{
  accountArray[serverId] = null;
}

function getServerIdAndPageIdFromTree(tree)
{
  var serverId = null;

  if (tree.view.selection.count < 1) return null;
  var node = tree.contentView.getItemAtIndex(tree.currentIndex);

  // get the page to load
  // (stored in the PageTag attribute of this node)
  var pageId = node.getAttribute('PageTag');

  // get the server's Id
  // (stored in the Id attribute of the server node)
  var servernode = node.parentNode.parentNode;

  // for toplevel treeitems, we just use the current treeitem
  //  dump("servernode is " + servernode + "\n");
  if (servernode.localName != "treeitem") {
    servernode = node;
  }
  return {"serverId": servernode.id, "pageId": pageId }
}
