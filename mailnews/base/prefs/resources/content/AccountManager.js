/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

var serverArray;

var accounts;
// called when the whole document loads
function onLoad() {
  
  serverArray = new Array;

  var mailsession =
    Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession);

  var am = mailsession.accountManager;
  accounts = am.accounts;
}

// called when a prefs page is done loading
function onPageLoad(event, name) {
  // page needs to be filled with values.
  // how do we determine which account we should be using?
  dump("loaded " + name + "\n");
}

// called when someone clicks on a 
function onAccountClick(event) {

  // get the page to load
  // (stored in the PageTag attribute of this node)
  var node = event.target.parentNode.parentNode;
  if (node.tagName != "treeitem") return;
  
  var pageUrl = node.getAttribute('PageTag');


  // get the server's ID
  // (stored in the ID attribute of the server node)
  var servernode = node.parentNode.parentNode;
  
  // for toplevel treeitems, we just use the current treeitem
  dump("servernode is " + servernode + "\n");
  if (servernode.tagName != "treeitem") {
    servernode = node;
  }
  var serverid = servernode.getAttribute('id');
  
  dump("before showPage(" + serverid + "," + pageUrl + ");\n");
  showPage(serverid, pageUrl);

}
// load pageUrl in the deck for the given server

function showPage(serverid, pageUrl) {

  dump("showPage(" + serverid + "," + pageUrl + ");\n");
  // find the IFRAME that corresponds to this server
  var destFrame = top.window.frames[serverid];

  var newLocation="";
  var oldLocation = new String(destFrame.location);

  // this is a quick hack to make this work for now.
  //  newLocation = pageURI;

  var lastSlashPos = oldLocation.lastIndexOf('/');
  
  if (lastSlashPos >= 0) {
    newLocation = oldLocation.slice(0, lastSlashPos+1);
  }
  newLocation += pageUrl;
  
  if (oldLocation != newLocation) {
    destFrame.location = newLocation;
  }

  // now see if we need to bring this server into view

  // find the box's index in the <deck> and bring it forward
  var deckBox= top.document.getElementById(serverid);
  var deck = deckBox.parentNode;
  var children = deck.childNodes;
  var i;
  for (i=0; i<children.length; i++) {
    if (children[i] == deckBox) break;
  }

  deck.setAttribute("value", i-1);
}


