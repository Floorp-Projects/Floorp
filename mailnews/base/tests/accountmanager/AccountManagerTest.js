function displayAccounts() {

  dump("Here come the accounts: \n");

  var msg = Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession);

  var am = msg.accountManager;
//  describe(am, "AccountManager");
  var servers = am.allServers;
  arrayDescribe(servers, Components.interfaces.nsIMsgIncomingServer, "Server");
  
  var identities = am.allIdentities;
  arrayDescribe(identities, Components.interfaces.nsIMsgIdentity, "Identity");

  var accounts = am.accounts;
  arrayDescribe(accounts, Components.interfaces.nsIMsgAccount, "Account");
  
}


function arrayDescribe(array, iface, name) {
    dump("There are " + array.Count() + " " + name + "(s).\n");
    for (var i=0; i<array.Count(); i++) {
      var obj = array.GetElementAt(i).QueryInterface(iface);
      dump(name + " #" + i + ":\n");
      describe(obj, "\t" + name);
    }
}

function describe(object, name) {
  for (var i in object) {
    var value = object[i];
    if (typeof(object[i]) == "function")
      value = "[function]";
    
    dump(name + "." + i + " = " + value + "\n");
  }
}
