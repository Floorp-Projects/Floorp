
function ltnCreateInstance(cid, iface) {
    if (!iface)
        iface = "nsISupports";
    return Components.classes[cid].createInstance(Components.interfaces[iface]);
}

function ltnGetService(cid, iface) {
    if (!iface)
        iface = "nsISupports";
    return Components.classes[cid].getService(Components.interfaces[iface]);
}

var atomSvc;
function ltnGetAtom(str) {
    if (!atomSvc) {
        atomSvc = ltnGetService("@mozilla.org/atom-service;1", "nsIAtomService");
    }

    return atomSvc.getAtom(str);
}
