// Bug 758509 changed things so that a JSScript is partially initialized when
// it is created, which is prior to bytecode generation; full initialization
// only occurs after bytecode generation.  This means that
// JSScript::markChildren() must deal with partially-initialized JSScripts.
// This test forces that to happen, because each let block allocates a
// StaticBlockObject.  All that should happen is that we don't crash.

let t = 0;
gczeal(2,1);
eval("\
let x = 3, y = 4;\
let (x = x+0, y = 12) { t += (x + y); }  \
let (x = x+1, y = 12) { t += (x + y); }  \
let (x = x+2, y = 12) { t += (x + y); }  \
let (x = x+3, y = 12) { t += (x + y); }  \
let (x = x+4, y = 12) { t += (x + y); }  \
let (x = x+5, y = 12) { t += (x + y); }  \
let (x = x+6, y = 12) { t += (x + y); }  \
let (x = x+7, y = 12) { t += (x + y); }  \
let (x = x+8, y = 12) { t += (x + y); }  \
let (x = x+9, y = 12) { t += (x + y); }  \
t += (x + y);\
assertEq(t, 202);\
");
