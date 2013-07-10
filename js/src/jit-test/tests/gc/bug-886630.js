function errorToString(e) {
    try {} catch (e2) {}
}
Object.getOwnPropertyNames(this);
if (false) {
    for (let x of constructors)
    print(x);
}
var tryRunning = tryRunningDirectly;
function unlikelyToHang(code) {
    var codeL = code.replace(/\s/g, " ");
    return true && code.indexOf("infloop") == -1 && !(codeL.match(/const.*for/)) // can be an infinite loop: function() { const x = 1; for each(x in ({a1:1})) dumpln(3); }
    && !(codeL.match(/for.*const/)) // can be an infinite loop: for each(x in ...); const x;
    && !(codeL.match(/for.*in.*uneval/)) // can be slow to loop through the huge string uneval(this), for example
    && !(codeL.match(/for.*for.*for/)) // nested for loops (including for..in, array comprehensions, etc) can take a while
    && !(codeL.match(/for.*for.*gc/))
}
function whatToTestSpidermonkeyTrunk(code) {
    var codeL = code.replace(/\s/g, " ");
    return {
        allowParse: true,
        allowExec: unlikelyToHang(code),
        allowIter: true,
        expectConsistentOutput: true && code.indexOf("Date") == -1 // time marches on
        && code.indexOf("random") == -1 && code.indexOf("dumpObject") == -1 // shows heap addresses
        && code.indexOf("oomAfterAllocations") == -1 && code.indexOf("ParallelArray") == -1,
        expectConsistentOutputAcrossIter: true && code.indexOf("options") == -1 // options() is per-cx, and the js shell doesn't create a new cx for each sandbox/compartment
        ,
        expectConsistentOutputAcrossJITs: true && code.indexOf("'strict") == -1 // bug 743425
        && code.indexOf("preventExtensions") == -1 // bug 887521
        && !(codeL.match(/\/.*[\u0000\u0080-\uffff]/)) // doesn't stay valid utf-8 after going through python (?)
    };
}
function tryRunningDirectly(f, code, wtt) {
    try {
        eval(code);
    } catch (e) {}
    try {
        var rv = f();
        tryIteration(rv);
    } catch (runError) {
        var err = errorToString(runError);
    }
    tryEnsureSanity();
}
var realEval = eval;
var realMath = Math;
var realFunction = Function;
var realGC = gc;
var realUneval = uneval;
function tryEnsureSanity() {
    try {
        delete this.Math;
        delete this.Function;
        delete this.gc;
        delete this.uneval;
        this.Math = realMath;
        this.eval = realEval;
        this.Function = realFunction;
        this.gc = realGC;
        this.uneval = realUneval;
    } catch (e) {}
}
function tryIteration(rv) {
    try {
        var iterCount = 0;
        for /* each */
        ( /* let */ iterValue in rv)
        print("Iterating succeeded, iterCount == " + iterCount);
    } catch (iterError) {}
}
function failsToCompileInTry(code) {
    try {
        new Function(" try { " + code + " } catch(e) { }");
    } catch (e) {}
}
function tryItOut(code) {
    if (count % 1000 == 0) {
        gc();
    }
    var wtt = whatToTestSpidermonkeyTrunk(code);
    code = code.replace(/\/\*DUPTRY\d+\*\//, function(k) {
        var n = parseInt(k.substr(8), 10);
        print(n);
        return strTimes("try{}catch(e){}", n);
    })
    try {
        f = new Function(code);
    } catch (compileError) {}
    if (code.indexOf("\n") == -1 && code.indexOf("\r") == -1 && code.indexOf("\f") == -1 && code.indexOf("\0") == -1 && code.indexOf("\u2028") == -1 && code.indexOf("\u2029") == -1 && code.indexOf("<--") == -1 && code.indexOf("-->") == -1 && code.indexOf("//") == -1) {
        var nCode = code;
        if (nCode.indexOf("return") != -1 || nCode.indexOf("yield") != -1 || nCode.indexOf("const") != -1 || failsToCompileInTry(nCode)) nCode = "(function(){" + nCode + "})()"
    }
    tryRunning(f, code, false);
}
var count = 0;
tryItOut("");
count = 2
tryItOut("");
tryItOut("");
tryItOut("o")
tryItOut("")
tryItOut("")
tryItOut("\
         with((/ /-7))\
         {\
         for(let mjcpxc=0;mjcpxc<9;++mjcpxc)\
         {\
         e=mjcpxc;\
         yield/x/\
         }}")

