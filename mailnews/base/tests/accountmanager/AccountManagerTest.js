function displayAccounts() {

  dump("Here come the accounts: \n");

  var msg = Components.classes["component://netscape/messenger/services/session"].getService(Components.interfaces.nsIMsgMailSession);

  var am = msg.accountManager;
  var servers = am.allServers;

  for (var i=0; i< servers.Count(); i++) {
    var serverSupports = servers.GetElementAt(i);
    var server = serverSupports.QueryInterface(Components.interfaces.nsIMsgIncomingServer);

    dump("Account: " + server.key + "\n");
    describe(server, "\t" + server.key);

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
