/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

function onInit() {
    initServerType();

}

function initServerType() {
  var serverType = document.getElementById("server.type").value;
  
  var verboseName;
  var index;
  if (serverType == "pop3") {
      verboseName = "POP Mail server";
      index = 0;
  } else if (serverType == "imap") {
      verboseName = "IMAP Mail server";
      index = 1;
  } else if (serverType == "nntp") {
      verboseName = "News server";
      index = 2;
  } else if (serverType == "none") {
      verboseName = "Local Mail store";
      index = 3;
  }

  if (index != undefined) {
      var deck = document.getElementById("serverdeck");
      dump("deck index was " + deck.getAttribute("index") + "\n");
      deck.setAttribute("index", index);
  }

  var hostname = document.getElementById("server.hostName").value;
  var username = document.getElementById("server.username").value;

  setDivText("servertype.verbose", verboseName);
  setDivText("servername.verbose", hostname);
  setDivText("username.verbose", username);
}

function setDivText(divname, value) {
    var div = document.getElementById(divname);
    if (!div) return;
    if (div.firstChild)
        div.removeChild(div.firstChild);
    div.appendChild(document.createTextNode(value));
}
