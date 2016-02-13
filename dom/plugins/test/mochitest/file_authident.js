var Ci = Components.interfaces;
var Cc = Components.classes;
var am = Cc["@mozilla.org/network/http-auth-manager;1"].
         getService(Ci.nsIHttpAuthManager);
am.setAuthIdentity("http", "mochi.test", 8888, "basic", "testrealm", "",
    "mochi.test", "user1", "password1");
