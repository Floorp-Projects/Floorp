let tgt_load = {};
let tgt_check = {};
Components.utils.import("resource://test/environment_loadscript.jsm", tgt_load);
Components.utils.import("resource://test/environment_checkscript.jsm", tgt_check);

// Check target bindings
var tgt_subscript_bound = "";
for (var name of ["vu", "vq", "vl", "gt", "ed", "ei", "fo", "fi", "fd"])
    if (tgt_load.target.hasOwnProperty(name))
        tgt_subscript_bound += name + ",";

// Expected subscript loader behavior is as follows:
//  - Qualified vars and |this| access occur on target object
//  - Lexical vars occur on ExtensibleLexicalEnvironment of target object
//  - Bareword assignments and global |this| access occur on caller's global
if (tgt_load.bound != "vu,ei,fo,fi,")
    throw new Error("Unexpected global binding set - " + tgt_load.bound);
if (tgt_subscript_bound != "vq,gt,ed,fd,")
    throw new Error("Unexpected target binding set - " + tgt_subscript_bound);

// Components should not share namespace
if (tgt_check.bound != "")
    throw new Error("Unexpected shared binding set - " + tgt_check.bound);
