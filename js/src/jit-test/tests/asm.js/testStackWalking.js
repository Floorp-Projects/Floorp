load(libdir + "asm.js");
load(libdir + "asserts.js");

function matchStack(stackString, stackArray)
{
    var match = 0;
    for (name of stackArray) {
        match = stackString.indexOf(name, match);
        if (match === -1)
            throw name + " not found in the stack " + stack;
    }
}

var stack;
function dumpStack()
{
    stack = new Error().stack
}

setJitCompilerOption("ion.usecount.trigger", 10);
setJitCompilerOption("baseline.usecount.trigger", 0);
setJitCompilerOption("offthread-compilation.enable", 0);
setCachingEnabled(true);

var callFFI = asmCompile('global', 'ffis', USE_ASM + "var ffi=ffis.ffi; function f() { return ffi()|0 } return f");

var f = asmLink(callFFI, null, {ffi:dumpStack});
for (var i = 0; i < 15; i++) {
    stack = null;
    f();
    matchStack(stack, ['dumpStack', 'f']);
}

if (isAsmJSCompilationAvailable() && isCachingEnabled()) {
    var callFFI = asmCompile('global', 'ffis', USE_ASM + "var ffi=ffis.ffi; function f() { return ffi()|0 } return f");
    assertEq(isAsmJSModuleLoadedFromCache(callFFI), true);
    stack = null;
    f();
    matchStack(stack, ['dumpStack', 'f']);
}

var f1 = asmLink(callFFI, null, {ffi:dumpStack});
var f2 = asmLink(callFFI, null, {ffi:function middle() { f1() }});
stack = null;
(function outer() { f2() })();
matchStack(stack, ["dumpStack", "f", "middle", "f"]);

function returnStackDumper() { return { valueOf:function() { stack = new Error().stack } } }
var f = asmLink(callFFI, null, {ffi:returnStackDumper});
for (var i = 0; i < 15; i++) {
    stack = null;
    f();
    matchStack(stack, ['valueOf', 'f']);
}

var caught = false;
try {
    stack = null;
    asmLink(asmCompile(USE_ASM + "function asmRec() { asmRec() } return asmRec"))();
} catch (e) {
    caught = true;
    matchStack(e.stack, ['asmRec', 'asmRec', 'asmRec', 'asmRec']);
}
assertEq(caught, true);

var caught = false;
try {
    callFFI(null, {ffi:Object.preventExtensions})();
} catch (e) {
    caught = true;
}
assertEq(caught, true);

asmLink(callFFI, null, {ffi:eval})();
asmLink(callFFI, null, {ffi:Function})();
asmLink(callFFI, null, {ffi:Error})();

var manyCalls = asmCompile('global', 'ffis',
    USE_ASM +
    "var ffi=ffis.ffi;\
     function f1(a,b,c,d,e,f,g,h,i,j,k) { \
       a=a|0;b=b|0;c=c|0;d=d|0;e=e|0;f=f|0;g=g|0;h=h|0;i=i|0;j=j|0;k=k|0; \
       ffi(); \
       return (a+b+c+d+e+f+g+h+i+j+k)|0; \
     } \
     function f2() { \
       return f1(1,2,3,4,5,6,7,8,f1(1,2,3,4,5,6,7,8,9,10,11)|0,10,11)|0; \
     } \
     function f3() { return 13 } \
     function f4(i) { \
       i=i|0; \
       return TBL[i&3]()|0; \
     } \
     var TBL=[f3, f3, f2, f3]; \
     return f4;");
stack = null;
assertEq(asmLink(manyCalls, null, {ffi:dumpStack})(2), 123);
matchStack(stack, ['dumpStack', 'f1', 'f2', 'f4']);
