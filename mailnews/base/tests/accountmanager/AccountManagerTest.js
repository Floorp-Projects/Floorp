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

function displayAccounts() {

  dump("Here come the accounts: \n");

  var msg = Components.classes["@mozilla.org/messenger/services/session;1"].getService(Components.interfaces.nsIMsgMailSession);

  var am = msg.accountManager;
//  describe(am, "AccountManager");
  var servers = am.allServers;
  arrayDescribe(servers, Components.interfaces.nsIMsgIncomingServer, "Server");
  
  var identities = am.allIdentities;
  arrayDescribe(identities,
                Components.interfaces.nsIMsgIdentity,
                "Identity", describeIdentity);

  var accounts = am.accounts;
  arrayDescribe(accounts, Components.interfaces.nsIMsgAccount, "Account");
  
}


function arrayDescribe(array, iface, name, describeCallback) {
    dump("There are " + array.Count() + " " + name + "(s).\n");
    for (var i=0; i<array.Count(); i++) {
      var obj = array.GetElementAt(i).QueryInterface(iface);
      dump(name + " #" + i + ":\n");
      describe(obj, "\t" + name);
      if (describeCallback) describeCallback(obj);
    }
}

function describe(object, name) {
  for (var i in object) {
    var value;
    try {
      value = object[i];
    } catch (e) {
      dump("the " + i + " slot of " + object + "returned an error. Please file a bug.\n");
    }
      
    if (typeof(object[i]) == "function")
      value = "[function]";
    
    dump(name + "." + i + " = " + value + "\n");
  }
}

function describeIdentity(identity) {
  dump("Additional Identity Info\n");
}

function nsISupportsArray2JSArray(array, IID) {
    var result = new Array;
    var length = array.Count();
    for (var i=0; i<length; i++) {
      result[i] = array.GetElement(i).QueryInterface(IID);
    }
    return result;
}
