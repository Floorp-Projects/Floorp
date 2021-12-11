function check_cl(iface, desc) {
    Assert.equal(iface.QueryInterface(Ci.nsIClassInfo).classDescription, desc);
}

function run_test() {
    check_cl(Ci, 'XPCComponents_Interfaces');
    check_cl(Cc, 'XPCComponents_Classes');
    check_cl(Cr, 'XPCComponents_Results');
}
