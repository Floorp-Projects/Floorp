
var name = "@mozilla.org/messenger/server;1?type=pop3";

try {
    var foo = Components.classes[name].createInstance();
    print(foo);
    print(name);
    for(p in Components.interfaces) {
        try {
            iface = foo.QueryInterface(Components.interfaces[p]);
            if(typeof(iface) == 'undefined')
                continue;
            print("\t"+p);
            for(m in iface) {
                print("\t\t"+m+" = "+typeof(iface[m]));
            }
        } catch(e) {
            
        }       
    }

    foo = null;
    gc();
} catch(e) {
    print("caught+e");
}

