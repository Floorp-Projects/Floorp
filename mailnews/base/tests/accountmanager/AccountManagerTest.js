function displayAccounts() {

  dump("Here come the accounts: \n");

  var msg = Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession);

  var am = msg.accountManager;
//  describe(am, "AccountManager");
  var servers = am.allServers;

  for (var i=0; i< servers.Count(); i++) {
    var serverSupports = servers.GetElementAt(i);
    var server = serverSupports.QueryInterface(Components.interfaces.nsIMsgIncomingServer);

    dump("Server: " + server.key + "\n");
    describe(server, "\t" + server.key);

    var identities = am.GetIdentitiesForServer(server);
    
    dump("\tThis server has " + identities.Count() + " identities:\n");
    for (var id=0; i<identities.Count; i++) {
      var identity =
        identities.GetElementAt(id).QueryInterface(Components.interfaces.nsIMsgIdentities);

      dump(identity.key + ":\n");
    }
  }

  var identities = am.allIdentities;

  dump("There are " + identities.Count() + " identities\n");
  for (var i=0; i< identities.Count(); i++) {
    var identity = identities.GetElementAt(i).QueryInterface(Components.interfaces.nsIMsgIdentity);

    dump("Identity: " + identity.key + "\n");
    describe(identity, "\t" + identity.key);

    var servers = am.GetServersForIdentity(identity);
    
    dump("\tThis identity has " + servers.Count() + " servers:\n");
    for (var sd=0; i<servers.Count; i++) {
      var server =
        servers.GetElementAt(id).QueryInterface(Components.interfaces.nsIMsgIdentities);

      dump(server.key + ":\n");
    }
  }

  
}


function describe(object, name) {
  for (var i in object) {
    var value = object[i];
    if (object[i] == "function")
      value = "[function]";
    
    dump(name + "." + i + " = " + value + "\n");
  }
}
