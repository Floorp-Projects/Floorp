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

var accountArray;

var lastServerId;
var lastPageId;

// services used
var RDF;
var accountManager;

// called when the whole document loads
// perform initialization here
function onLoad() {
  accountArray = new Array;
  RDF = Components.classes["component://netscape/rdf/rdf-service"].getService(Components.interfaces.nsIRDFService);

  var mailsession =
    Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession);

  accountManager = mailsession.accountManager;

  doSetOKCancel(onOk, 0);
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

  // make sure the current visible page is saved
  savePage(lastServerId, lastPageId);
  
  for (var accountid in accountArray) {
    var account = getAccountFromServerId(accountid);
    var accountValues = accountArray[accountid];
    
    saveAccount(accountValues, account);
  }
}

function onNewAccount() {
  var result = { refresh: false };
  window.openDialog("chrome://messenger/content/AccountWizard.xul", "wizard", "chrome,modal", result);
  if (result.refresh) {
    refreshAccounts();
    
    // propagate refresh if it's not already on
    // i.e. we'll never turn off refresh once it's on.
    window.arguments[0].refresh = true;
  }

}

// another temporary hack until the account manager
// can refresh the account list itself.
function refreshAccounts()
{
  var tree = document.getElementById("accounttree");
  tree.setAttribute('ref', tree.getAttribute('ref'));
}

function saveAccount(accountValues, account)
{
  var identity = account.defaultIdentity;
  var server = account.incomingServer;
  
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
      } catch (ex) {
        // don't do anything, just means we don't support that
      }
      if (dest == undefined) continue;
      
      if (dest[slot] != typeArray[slot]) {
        //dump("Array->Account: " + slot + " to " + dest + "\n");
        try {
          dest[slot] = typeArray[slot];
        } catch (ex) {
          // hrm... need to handle special types here
        }
      }
    }
  }
}

// called when a prefs page is done loading
function onPageLoad(event, name) {
  // page needs to be filled with values.
  // how do we determine which account we should be using?
}

//
// called when someone clicks on an account
// figure out context by what they clicked on
//
function onAccountClick(event) {

  // get the page to load
  // (stored in the PageTag attribute of this node)
  var node = event.target.parentNode.parentNode;
  if (node.tagName != "treeitem") return;
  var pageId = node.getAttribute('PageTag');


  // get the server's ID
  // (stored in the ID attribute of the server node)
  var servernode = node.parentNode.parentNode;
  
  // for toplevel treeitems, we just use the current treeitem
  //  dump("servernode is " + servernode + "\n");
  if (servernode.tagName != "treeitem") {
    servernode = node;
  }
  var serverid = servernode.getAttribute('id');
  
  //dump("before showPage(" + serverid + "," + pageId + ");\n");
  showPage(serverid, pageId);

}

// show the page for the given server:
// - save the old values
// - initialize the widgets with the new values
function showPage(serverId, pageId) {

  if (pageId == lastPageId &&
      serverId == lastServerId) return;

  savePage(lastServerId, lastPageId);

  restorePage(serverId, pageId);
  showDeckPage(pageId);

  lastServerId = serverId;
  lastPageId = pageId;
}

//
// show the page with the given id
//
function showDeckPage(deckBoxId) {

  /* bring the deck to the front */
  var deckBox = top.document.getElementById(deckBoxId);
  var deck = deckBox.parentNode;
  var children = deck.childNodes;

  // search through deck children, and find the index to load
  for (var i=0; i<children.length; i++) {
    if (children[i] == deckBox) break;
  }

  deck.setAttribute("index", i);

}

//
// save the values of the widgets to the given server
//
function savePage(serverId, pageId) {
  if (!serverId || !pageId) return;

  // tell the page that it's about to save
  if (top.frames[pageId].onSave)
      top.frames[pageId].onSave();
  
  var accountValues = getValueArrayFor(serverId);
  var pageElements = getPageFormElements(pageId);

  // store the value in the account
  for (var i=0; i<pageElements.length; i++) {
      if (pageElements[i].name) {
        var vals = pageElements[i].name.split(".");
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

  dump("Form->Array: accountValues[" + type + "][" + slot + "] = " + value + "\n");
  
  accountValues[type][slot] = value;
}

function getAccountValue(account, accountValues, type, slot) {
  if (!accountValues[type])
    accountValues[type] = new Array;

  // fill in the slot from the account if necessary
  if (accountValues[type][slot]== undefined) {
    // dump("Array->Form: lazily reading in the " + slot + " from the " + type + "\n");
    var server= account.incomingServer;
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

    } catch (ex) {
    }
    
    if (source) {
      accountValues[type][slot] = getStringValueOf(source[slot]);
    }
  }
  var value = accountValues[type][slot];
  dump("Array->Form: accountValues[" + type + "][" + slot + "] = " + value + "\n");
  return value;
}
//
// restore the values of the widgets from the given server
//
function restorePage(serverId, pageId) {
  if (!serverId || !pageId) return;
  
  var accountValues = getValueArrayFor(serverId);
  var pageElements = getPageFormElements(pageId);

  // restore the value from the account
  for (var i=0; i<pageElements.length; i++) {
      if (pageElements[i].name) {
        var vals = pageElements[i].name.split(".");
        var type = vals[0];
        var slot = vals[1];

        var account = getAccountFromServerId(serverId);
        var value = getAccountValue(account, accountValues, type, slot);
        setFormElementValue(pageElements[i], value);
      }
  }

  // tell the page that new values have been loaded
  if (top.frames[pageId].onInit)
      top.frames[pageId].onInit();
}

//
// gets the value of a widget
//
function getFormElementValue(formElement) {
  if (formElement.type=="checkbox") {
    if (formElement.getAttribute("reversed"))
      return !formElement.checked;
    else
      return formElement.checked;
  } else
    return formElement.value;
}

function getStringValueOf(obj) {

  var value = obj;
  if (value && typeof(value) == "object") {
    try {
      var filespec = value.QueryInterface(Components.interfaces.nsIFileSpec);
      realvalue = filespec.nativePath;
    } catch (ex) {}
  }
  return obj;
}
//
// sets the value of a widget
//
function setFormElementValue(formElement, value) {

  if (formElement.type == "checkbox") {
    if (value == undefined) {
      formElement.checked = formElement.defaultChecked;
    } else {
      if (formElement.getAttribute("reversed"))
        formElement.checked = !value;
      else
        formElement.checked = value;
    }
  } else {
    if (value == undefined)
      formElement.value = formElement.defaultValue;
    else
      formElement.value = value;
  }
}


function fillAccountValues(accountValues, account, fields) {

  var identity = account.defaultIdentity;
  var server = account.incomingServer;
  
  for (var i=0; i<fields.length; i++) {
    var fullname = fields.name;
    var vals = fullname.split(".");
    var type = vals[0];
    var slot = fields[1];

  }
  accountValues[type][slot] = identity[slot];

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
  var serverFolder =
    serverResource.QueryInterface(Components.interfaces.nsIMsgFolder);

  var incomingServer = serverFolder.server;

  var account = accountManager.FindAccountForServer(incomingServer);
  return account;
}

//
// get the array of form elements for the given page
//
function getPageFormElements(pageId) {
  var pageFrame = top.frames[pageId];
  var pageDoc = top.frames[pageId].document;
  var pageElements = pageDoc.getElementsByTagName("FORM")[0].elements;

  return pageElements;
}

//
// get the value array for the given serverId
//
function getValueArrayFor(serverId) {
  if (accountArray[serverId] == null) {
    accountArray[serverId] = new Array;
  }
  
  return accountArray[serverId];
}

