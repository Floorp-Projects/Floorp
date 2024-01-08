function getLogString(obj) {
    let log = getWatchtowerLog();
    return log.map(item => {
        assertEq(item.object, obj);
        if (typeof item.extra === "symbol") {
            item.extra = "<symbol>";
        }
        return item.kind + (item.extra ? ": " + item.extra : "");
    }).join("\n");
}

const entry = cacheEntry("");
addWatchtowerTarget(entry);
evaluate(entry, { "saveIncrementalBytecode": true });
let log = getLogString(entry);

// Nothing is logged for the manipulation of reserved slots, as we
// lack infrastructure to track them thus far.
assertEq(log.length, 0);
