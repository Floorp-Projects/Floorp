print("Components.interfaces.nsIJSID = " +
       Components.interfaces.nsIJSID);


print("Components = " +
       Components);

print("Components.interfaces = " +
       Components.interfaces);

print("Components.interfaces.nsISupports = " +
       Components.interfaces.nsISupports);

print("Components.interfaces.nsISupports.name = " +
       Components.interfaces.nsISupports.name);

print("Components.interfaces.nsISupports.number = " +
       Components.interfaces.nsISupports.number);

print("Components.interfaces.nsISupports.id = " +
       Components.interfaces.nsISupports.id);

print("Components.interfaces.nsISupports.valid = " +
       Components.interfaces.nsISupports.valid);

var id1 = Components.interfaces.nsISupports;
var id2 = Components.interfaces.nsISupports;
var id3 = Components.interfaces.nsISupports.id;


print("identity test       "+ (id1 == id2 ?         "passed" : "failed"));
print("non-identity test   "+ (id1 != id3 ?         "passed" : "failed"));
print("equality test       "+ (id1.equals(id2) ?    "passed" : "failed"));

var NS_ISUPPORTS_IID = new Components.ID("{00000000-0000-0000-c000-000000000046}");
print("NS_ISUPPORTS_IID = " + NS_ISUPPORTS_IID);
