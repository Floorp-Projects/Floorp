/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

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


var accountArray;
var accounttree;

var currentServerId;
var currentPageId;

var pendingServerId;
var pendingPageId;
var Bundle = srGetStrBundle("chrome://messenger/locale/prefs.properties");

// services used
var RDF;
var accountManager;
var smtpService;

// widgets
var duplicateButton;
var deleteButton;
var newAccountButton;
var setDefaultButton;

// called when the whole document loads
// perform initialization here
function onLoad() {
  
  var selectedAccount;
  if (window.arguments && window.arguments[0])
    selectedServer = window.arguments[0].server;
  
  accountArray = new Array;
  RDF = Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);

  accountManager = Components.classes["component://netscape/messenger/account-manager"].getService(Components.interfaces.nsIMsgAccountManager);

  smtpService =
    Components.classes["component://netscape/messengercompose/smtp"].getService(Components.interfaces.nsISmtpService);
  accounttree = document.getElementById("accounttree");
  
  doSetOKCancel(onOk, 0);

  newAccountButton = document.getElementById("newAccountButton");
  duplicateButton = document.getElementById("duplicateButton");
  deleteButton = document.getElementById("deleteButton");
  setDefaultButton = document.getElementById("setDefaultButton");

  sortAccountList(accounttree);
  selectServer(selectedServer)
}

function sortAccountList(accounttree)
{
  var xulSortService = Components.classes["component://netscape/rdf/xul-sort-service"].getService(Components.interfaces.nsIXULSortService);
  
  xulSortService.Sort(accounttree, 'http://home.netscape.com/NC-rdf#FolderTreeName?sort=true', 'ascending');
}

function selectServer(server)
{
  var selectedItem;
  
  if (server)
    selectedItem = document.getElementById(server.serverURI);

  if (!selectedItem)
    selectedItem = getFirstAccount();
  
  accounttree.selectItem(selectedItem);
  
  var result = getServerIdAndPageIdFromTree(accounttree);
  if (result) {
    updateButtons(accounttree,result.serverId);
  }
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

  var children = treechildren.childNodes;
  for (var i=0; i<children.length; i++) {
    if (children[i].localName == "treeitem")
      return children[i];
  }
}

function onOk() {
  onSave();
    // hack hack - save the prefs file NOW in case we crash
    try {
        var prefs = Components.classes["component://netscape/preferences"].getService(Components.interfaces.nsIPref);
        prefs.SavePrefFile();
    } catch (ex) {
        dump("Error saving prefs!\n");
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

    saveAccount(accountValues, account);
  }
}

function onNewAccount() {
  MsgAccountWizard();
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

			var protocolinfo = Components.classes["component://netscape/messenger/protocol/info;type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);
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
				var alertText = Bundle.GetStringFromName("failedDuplicateAccount");
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
}

function onDeleteAccount(event) {
    //dump("onDeleteAccount\n");

    if (event.target.getAttribute("disabled") == "true") return;

	var result = getServerIdAndPageIdFromTree(accounttree);
    if (!result) return;
    
    var account = getAccountFromServerId(result.serverId);
    if (!account) return;

    var server = account.incomingServer;
    var type = server.type; 

    var protocolinfo = Components.classes["component://netscape/messenger/protocol/info;type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);
    if (!protocolinfo.canDelete) return;

    var confirmDeleteAccount =
      Bundle.GetStringFromName("confirmDeleteAccount");
    if (!window.confirm(confirmDeleteAccount)) return;
    
    try {
      // clear cached data out of the account array
      if (accountArray[result.serverId])
        accountArray[result.serverId] = null;
      currentServerId = currentPageId = null;
      
      accountManager.removeAccount(account);
      selectServer(null);
    }
    catch (ex) {
      dump("failure to delete account: " + ex + "\n");
      var alertText = Bundle.GetStringFromName("failedDeleteAccount");
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

      if (dest[slot] != typeArray[slot]) {
        try {
          dest[slot] = typeArray[slot];
        } catch (ex) {
          // hrm... need to handle special types here
        }
      }
    }
  }
}


function updateButtons(tree,serverId) {
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

    if (account.identities.Count() < 1)
      canSetDefault = false;
    
	//dump("servertype = " + type + "\n");

	var protocolinfo = Components.classes["component://netscape/messenger/protocol/info;type=" + type].getService(Components.interfaces.nsIMsgProtocolInfo);
    canDuplicate = protocolinfo.canDuplicate;
	canDelete = protocolinfo.canDelete;

  }
  else {
	// HACK
	// if account is null, we have either selected a SMTP server, or there is a problem
	// either way, we don't want the user to be able to delete it or duplicate it
    canSetDefault = false;
	canDelete = false;
	canDuplicate = false;
  }

  if (tree.selectedItems.length < 1)
    canDuplicate = canSetDefault = canDelete = false;
  
  setEnabled(duplicateButton, canDuplicate);
  setEnabled(setDefaultButton, canSetDefault);
  setEnabled(deleteButton, canDelete);
  
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
// called when someone clicks on an account
// figure out context by what they clicked on
//
function onAccountClick(tree) {
  var result = getServerIdAndPageIdFromTree(tree);
  
  if (result) {
	  showPage(result.serverId, result.pageId);
	  updateButtons(tree,result.serverId);
  }
}

// show the page for the given server:
// - save the old values
// - start loading the new page
function showPage(serverId, pageId) {

  if (pageId == currentPageId &&
      serverId == currentServerId)
    return;


  // save the previous page
  savePage(currentServerId);
  
  // loading a complete different page
  if (pageId != currentPageId) {
    
    // prevent overwriting with bad stuff
    currentServerId = currentPageId = null;
    
    pendingServerId=serverId;
    pendingPageId=pageId;
    loadPage(pageId);
  }
  
  // same page, different server
  else if (serverId != currentServerId) {
    restorePage(pageId, serverId);
  }
  
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
      restorePage(currentServerId, currentPageId);
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
  frames["contentFrame"].location = "chrome://messenger/content/" + pageId;
}

//
// save the values of the widgets to the given server
//
function savePage(serverId) {
  
  if (!serverId) return;

  // tell the page that it's about to save
  if (top.frames["contentFrame"].onSave)
      top.frames["contentFrame"].onSave();
  
  var accountValues = getValueArrayFor(serverId);
  var pageElements = getPageFormElements();

  if (pageElements == null) return;

  // store the value in the account
  for (var i=0; i<pageElements.length; i++) {
      if (pageElements[i].id) {
        var vals = pageElements[i].id.split(".");
        var type = vals[0];
        var slot = vals[1];

        setAccountValue(accountValues,
                        type, slot,
                        getFormElementValue(pageElements[i]));
      }
  }

}

function setAccountValue(accountValues, type, slot, value) {
  if (!accountValues[type])
    accountValues[type] = new Array;

  //dump("Form->Array: accountValues[" + type + "][" + slot + "] = " + value + "\n");
  
  accountValues[type][slot] = value;
}

function getAccountValue(account, accountValues, type, slot) {
  if (!accountValues[type])
    accountValues[type] = new Array;

  // fill in the slot from the account if necessary
  if (accountValues[type][slot]== undefined) {
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
      accountValues[type][slot] = source[slot];
    }
  }
  var value = accountValues[type][slot];
  //dump("Array->Form: accountValues[" + type + "][" + slot + "] = " + value + "\n");
  return value;
}
//
// restore the values of the widgets from the given server
//
function restorePage(pageId, serverId) {
  if (!serverId) return;

  var accountValues = getValueArrayFor(serverId);
  var pageElements = getPageFormElements();

  if (pageElements == null) return;

  var account = getAccountFromServerId(serverId);

  if (top.frames["contentFrame"].onPreInit)
    top.frames["contentFrame"].onPreInit(account, accountValues);
  
  // restore the value from the account
  for (var i=0; i<pageElements.length; i++) {
      if (pageElements[i].id) {
        var vals = pageElements[i].id.split(".");
        var type = vals[0];
        var slot = vals[1];

        var value = getAccountValue(account, accountValues, type, slot);
        setFormElementValue(pageElements[i], value);
      }
  }

  // tell the page that new values have been loaded
  if (top.frames["contentFrame"].onInit)
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
    else
      return formElement.checked;
  }

  else if (type == "radiogroup" || type=="menulist") {
    return formElement.selectedItem.data;
  }
  
  else if (type == "textfield" &&
           formElement.getAttribute("datatype") == "nsIFileSpec") {
    if (formElement.value) {
      var filespec = Components.classes["component://netscape/filespec"].createInstance(Components.interfaces.nsIFileSpec);
      filespec.nativePath = formElement.value;
      return filespec;
    } else {
      return null;
    }
  }

  else if (type == "text") {
    var val = formElement.getAttribute("value");
    if (val) return val;
    else return null;
  }
  
  else {
    return formElement.value;
  }
 }
 catch (ex) {
  dump("getFormElementValue failed, ex="+ex+"\n");
  return null;
 }
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
      if (formElement.defaultChecked)
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
      selectedItem = formElement.getElementsByAttribute("data", value)[0];
    
    formElement.selectedItem = selectedItem;
  }
  // handle nsIFileSpec
  else if (type == "textfield" &&
           formElement.getAttribute("datatype") == "nsIFileSpec") {
    if (value) {
      var filespec = value.QueryInterface(Components.interfaces.nsIFileSpec);
      try {
        formElement.value = filespec.nativePath;
      } catch (ex) {
        dump("Still need to fix uninitialized filespec problem!\n");
      }
    } else {
      if (formElement.defaultValue)
        formElement.value = formElement.defaultValue;
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
      if (formElement.defaultValue)
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
    return null;
  }
}

//
// get the array of form elements for the given page
//
function getPageFormElements() {
 try {
	var pageElements =
      top.frames["contentFrame"].document.getElementsByAttribute("wsm_persist", "true");
	return pageElements;
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
  
  if (accountArray[serverId] == null) {
    accountArray[serverId] = new Array;
  }
  
  return accountArray[serverId];
}

function clearAccountData(serverId, pageId)
{
  getValueArrayFor(serverId)[pageId] = null;
}

function getServerIdAndPageIdFromTree(tree)
{
  var serverId = null;

  if (tree.selectedItems.length < 1) return null;
  var node = tree.selectedItems[0];

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
  serverId = servernode.getAttribute('id');  

  return {"serverId": serverId, "pageId": pageId }
}
