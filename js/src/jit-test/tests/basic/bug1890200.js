let triggerGC = false;
let proxy = new Proxy({}, {get: function(target, key) {
    if (key === "sameCompartmentAs" || key === "sameZoneAs") {
        triggerGC = true;
        return newGlobal({newCompartment: true});
    }
    if (triggerGC) {
        gc();
        triggerGC = false;
    }
}});
newGlobal(proxy);
