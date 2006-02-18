
function ltnCreateInstance(cid, iface) {
    if (!iface)
        iface = "nsISupports";
    try {
        return Components.classes[cid].createInstance(Components.interfaces[iface]);
    } catch(e) {
        dump("#### ltnCreateInstance failed for: " + cid + "\n");
        var frame = Components.stack;
        for (var i = 0; frame && (i < 4); i++) {
            dump(i + ": " + frame + "\n");
            frame = frame.caller;
        }
        throw e;
    }
}

function ltnGetService(cid, iface) {
    if (!iface)
        iface = "nsISupports";
    try {
        return Components.classes[cid].getService(Components.interfaces[iface]);
    } catch(e) {
        dump("#### ltnGetService failed for: " + cid + "\n");
        var frame = Components.stack;
        for (var i = 0; frame && (i < 4); i++) {
            dump(i + ": " + frame + "\n");
            frame = frame.caller;
        }
        throw e;
    }
}

var atomSvc;
function ltnGetAtom(str) {
    if (!atomSvc) {
        atomSvc = ltnGetService("@mozilla.org/atom-service;1", "nsIAtomService");
    }

    return atomSvc.getAtom(str);
}

var CalDateTime = new Components.Constructor("@mozilla.org/calendar/datetime;1",
                                             Components.interfaces.calIDateTime);

// Use these functions, since setting |element.collapsed = true| is NOT reliable
function collapseElement(element) {
    element.style.visibility = "collapse";
}

function uncollapseElement(element) {
    element.style.visibility = "";
}

// These are simply placeholder functions until Lightning gets real undo/redo
function startBatchTransaction() {}
function endBatchTransaction() {}
