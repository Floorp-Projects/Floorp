var iface_count = 0;
var constant_count = 0;
for(var iface_name in Components.interfaces) {
    var iface = Components.interfaces[iface_name];
    print(iface_name);
    var iface_name;
    for(var const_name in iface) {
        print("  "+const_name+" = "+iface[const_name])
        constant_count++; 
    }
    iface_count++; 
}
print("\n"+iface_count+" interfaces with "+constant_count+" constants");


