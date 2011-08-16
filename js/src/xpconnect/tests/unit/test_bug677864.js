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
    check_cl(Components.ID, 'XPCComponents_ID');
    check_cl(Components.Exception, 'XPCComponents_Exception');
    check_cl(Components.Constructor, 'XPCComponents_Constructor');
}
