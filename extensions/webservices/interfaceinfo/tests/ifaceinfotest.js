
const IDL_GENERATOR = 
    new Components.Constructor("@mozilla.org/interfaceinfotoidl;1",
                               "nsIInterfaceInfoToIDL");


var gen = new IDL_GENERATOR();

print();
print();
print(gen.generateIDL(Components.interfaces.nsISupports));
print("//------------------------------------------------------------");
print(gen.generateIDL(Components.interfaces.nsIDataType));
print("//------------------------------------------------------------");
print(gen.generateIDL(Components.interfaces.nsIScriptableInterfaceInfo));
print("//------------------------------------------------------------");
print(gen.generateIDL(Components.interfaces.nsIVariant));
print();
print();

