function check_cl(iface, desc) {
    do_check_eq(iface.QueryInterface(Components.interfaces.nsIClassInfo).classDescription, desc);
}

function run_test() {
    check_cl(Components, 'XPCComponents');
    check_cl(Components.interfaces, 'XPCComponents_Interfaces');
    check_cl(Components.interfacesByID, 'XPCComponents_InterfacesByID');
    check_cl(Components.classes, 'XPCComponents_Classes');
    check_cl(Components.classesByID, 'XPCComponents_ClassesByID');
    check_cl(Components.results, 'XPCComponents_Results');
}
