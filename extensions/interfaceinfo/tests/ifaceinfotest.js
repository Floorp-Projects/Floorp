
const IDL_GENERATOR = 
    new Components.Constructor("@mozilla.org/interfaceinfotoidl;1",
                               "nsIInterfaceInfoToIDL");

var gen = new IDL_GENERATOR();

function generateIDL(iid) {return gen.generateIDL(iid, false, false);}

print();
print();
print(generateIDL(Components.interfaces.nsISupports));
print("//------------------------------------------------------------");
print(generateIDL(Components.interfaces.nsIDataType));
print("//------------------------------------------------------------");
print(generateIDL(Components.interfaces.nsIScriptableInterfaceInfo));
print("//------------------------------------------------------------");
print(generateIDL(Components.interfaces.nsIVariant));
print();
print();

