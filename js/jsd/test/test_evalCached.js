// This test must be run with debugging already enabled

function run_test() {
    const Cc = Components.classes;
    const Ci = Components.interfaces;
    const DebuggerService = Cc["@mozilla.org/js/jsd/debugger-service;1"];
    const jsdIDebuggerService = Ci.jsdIDebuggerService;
    var jsd = DebuggerService.getService(jsdIDebuggerService);

    do_check_true(jsd.isOn);

    jsd.scriptHook = {
        onScriptCreated: function(script) {
            // Just the presence of this will trigger the script to be handed
            // to JSD and trigger the crash
        },
        onScriptDestroyed: function(script) {
        }
    }

    eval("4+4");
    eval("4+4"); // Will be found in the eval cache
}
