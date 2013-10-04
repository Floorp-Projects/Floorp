
var config = getBuildConfiguration();

// FIXME: ASAN debug builds run this too slowly for now.  Re-enable
// after bug 919948 lands.
if (!(config.debug && config.asan)) {
    let a = [];
    a.length = getMaxArgs() + 1;

    let f = function() {
    };

    try {
        f(...a);
    } catch (e) {
        assertEq(e.message, "too many function arguments");
    }

    try {
        new f(...a);
    } catch (e) {
        assertEq(e.message, "too many constructor arguments");
    }

    try {
        eval(...a);
    } catch (e) {
        assertEq(e.message, "too many function arguments");
    }
}
