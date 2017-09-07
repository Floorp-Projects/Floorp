let tgt = {};
Components.utils.import("resource://test/environment_script.js", tgt);
Components.utils.import("resource://test/environment_checkscript.jsm", tgt);


// Components should not share namespace
if (tgt.bound != "")
    throw new Error("Unexpected shared binding set - " + tgt.bound);
