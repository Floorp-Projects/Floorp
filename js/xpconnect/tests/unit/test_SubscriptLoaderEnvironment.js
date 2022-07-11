let tgt = {};

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

Services.scriptloader.loadSubScript("resource://test/environment_script.js", tgt);

var bound = "";
var tgt_bound = "";

// Check global bindings
try { void vu; bound += "vu,"; } catch (e) {}
try { void vq; bound += "vq,"; } catch (e) {}
try { void vl; bound += "vl,"; } catch (e) {}
try { void gt; bound += "gt,"; } catch (e) {}
try { void ed; bound += "ed,"; } catch (e) {}
try { void ei; bound += "ei,"; } catch (e) {}
try { void fo; bound += "fo,"; } catch (e) {}
try { void fi; bound += "fi,"; } catch (e) {}
try { void fd; bound += "fd,"; } catch (e) {}

// Check target bindings
for (var name of ["vu", "vq", "vl", "gt", "ed", "ei", "fo", "fi", "fd"])
    if (tgt.hasOwnProperty(name))
        tgt_bound += name + ",";


// Expected subscript loader behavior is as follows:
//  - Qualified vars and |this| access occur on target object
//  - Lexical vars occur on ExtensibleLexicalEnvironment of target object
//  - Bareword assignments and global |this| access occur on caller's global
if (bound != "vu,ei,fo,fi,")
    throw new Error("Unexpected global binding set - " + bound);
if (tgt_bound != "vq,gt,ed,fd,")
    throw new Error("Unexpected target binding set - " + tgt_bound);
