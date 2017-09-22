/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

loadRelativeToScript('callgraph.js');

var theFunctionNameToFind;
if (scriptArgs[0] == '--function') {
    theFunctionNameToFind = scriptArgs[1];
    scriptArgs = scriptArgs.slice(2);
}

var typeInfo_filename = scriptArgs[0] || "typeInfo.txt";

var memoized = new Map();
var memoizedCount = 0;

function memo(name)
{
    if (!memoized.has(name)) {
        let id = memoized.size + 1;
        memoized.set(name, "" + id);
        print(`#${id} ${name}`);
    }
    return memoized.get(name);
}

var lastline;
function printOnce(line)
{
    if (line != lastline) {
        print(line);
        lastline = line;
    }
}

// Returns a table mapping function name to lists of [annotation-name,
// annotation-value] pairs: { function-name => [ [annotation-name, annotation-value] ] }
function getAnnotations(body)
{
    var all_annotations = {};
    for (var v of (body.DefineVariable || [])) {
        if (v.Variable.Kind != 'Func')
            continue;
        var name = v.Variable.Name[0];
        var annotations = all_annotations[name] = [];

        for (var ann of (v.Type.Annotation || [])) {
            annotations.push(ann.Name);
        }
    }

    return all_annotations;
}

function getTags(functionName, body) {
    var tags = new Set();
    var annotations = getAnnotations(body);
    if (functionName in annotations) {
        for (var [ annName, annValue ] of annotations[functionName]) {
            if (annName == 'Tag')
                tags.add(annValue);
        }
    }
    return tags;
}

function processBody(functionName, body)
{
    if (!('PEdge' in body))
        return;

    for (var tag of getTags(functionName, body).values())
        print("T " + memo(functionName) + " " + tag);

    // Set of all callees that have been output so far, in order to suppress
    // repeated callgraph edges from being recorded. Use a separate set for
    // suppressed callees, since we don't want a suppressed edge (within one
    // RAII scope) to prevent an unsuppressed edge from being recorded. The
    // seen array is indexed by a boolean 'suppressed' variable.
    var seen = [ new Set(), new Set() ];

    lastline = null;
    for (var edge of body.PEdge) {
        if (edge.Kind != "Call")
            continue;

        // Whether this call is within the RAII scope of a GC suppression class
        var edgeSuppressed = (edge.Index[0] in body.suppressed);

        for (var callee of getCallees(edge)) {
            var suppressed = Boolean(edgeSuppressed || callee.suppressed);
            var prologue = suppressed ? "SUPPRESS_GC " : "";
            prologue += memo(functionName) + " ";
            if (callee.kind == 'direct') {
                if (!seen[+suppressed].has(callee.name)) {
                    seen[+suppressed].add(callee.name);
                    printOnce("D " + prologue + memo(callee.name));
                }
            } else if (callee.kind == 'field') {
                var { csu, field, isVirtual } = callee;
                const tag = isVirtual ? 'V' : 'F';
                printOnce(tag + " " + prologue + "CLASS " + csu + " FIELD " + field);
            } else if (callee.kind == 'resolved-field') {
                // Fully-resolved field (virtual method) call. Record the
                // callgraph edges. Do not consider suppression, since it is
                // local to this callsite and we are writing out a global
                // record here.
                //
                // Any field call that does *not* have an R entry must be
                // assumed to call anything.
                var { csu, field, callees } = callee;
                var fullFieldName = csu + "." + field;
                if (!virtualResolutionsSeen.has(fullFieldName)) {
                    virtualResolutionsSeen.add(fullFieldName);
                    for (var target of callees)
                        printOnce("R " + memo(fullFieldName) + " " + memo(target.name));
                }
            } else if (callee.kind == 'indirect') {
                printOnce("I " + prologue + "VARIABLE " + callee.variable);
            } else if (callee.kind == 'unknown') {
                printOnce("I " + prologue + "VARIABLE UNKNOWN");
            } else {
                printErr("invalid " + callee.kind + " callee");
                debugger;
            }
        }
    }
}

var typeInfo = loadTypeInfo(typeInfo_filename);

loadTypes("src_comp.xdb");

var xdb = xdbLibrary();
xdb.open("src_body.xdb");

printErr("Finished loading data structures");

var minStream = xdb.min_data_stream();
var maxStream = xdb.max_data_stream();

if (theFunctionNameToFind) {
    var index = xdb.lookup_key(theFunctionNameToFind);
    if (!index) {
        printErr("Function not found");
        quit(1);
    }
    minStream = maxStream = index;
}

function process(functionName, functionBodies)
{
    for (var body of functionBodies)
        body.suppressed = [];

    for (var body of functionBodies) {
        for (var [pbody, id] of allRAIIGuardedCallPoints(typeInfo, functionBodies, body, isSuppressConstructor))
            pbody.suppressed[id] = true;
    }

    for (var body of functionBodies)
        processBody(functionName, body);

    // GCC generates multiple constructors and destructors ("in-charge" and
    // "not-in-charge") to handle virtual base classes. They are normally
    // identical, and it appears that GCC does some magic to alias them to the
    // same thing. But this aliasing is not visible to the analysis. So we'll
    // add a dummy call edge from "foo" -> "foo *INTERNAL* ", since only "foo"
    // will show up as called but only "foo *INTERNAL* " will be emitted in the
    // case where the constructors are identical.
    //
    // This is slightly conservative in the case where they are *not*
    // identical, but that should be rare enough that we don't care.
    var markerPos = functionName.indexOf(internalMarker);
    if (markerPos > 0) {
        var inChargeXTor = functionName.replace(internalMarker, "");
        printOnce("D " + memo(inChargeXTor) + " " + memo(functionName));
    }

    // Further note: from https://itanium-cxx-abi.github.io/cxx-abi/abi.html the
    // different kinds of constructors/destructors are:
    // C1	# complete object constructor
    // C2	# base object constructor
    // C3	# complete object allocating constructor
    // D0	# deleting destructor
    // D1	# complete object destructor
    // D2	# base object destructor
    //
    // In actual practice, I have observed C4 and D4 xtors generated by gcc
    // 4.9.3 (but not 4.7.3). The gcc source code says:
    //
    //   /* This is the old-style "[unified]" constructor.
    //      In some cases, we may emit this function and call
    //      it from the clones in order to share code and save space.  */
    //
    // Unfortunately, that "call... from the clones" does not seem to appear in
    // the CFG we get from GCC. So if we see a C4 constructor or D4 destructor,
    // inject an edge to it from C1, C2, and C3 (or D1, D2, and D3). (Note that
    // C3 isn't even used in current GCC, but add the edge anyway just in
    // case.)
    //
    // from gcc/cp/mangle.c:
    //
    // <special-name> ::= D0 # deleting (in-charge) destructor
    //                ::= D1 # complete object (in-charge) destructor
    //                ::= D2 # base object (not-in-charge) destructor
    // <special-name> ::= C1   # complete object constructor
    //                ::= C2   # base object constructor
    //                ::= C3   # complete object allocating constructor
    //
    // Currently, allocating constructors are never used.
    //
    if (functionName.indexOf("C4") != -1) {
        var [ mangled, unmangled ] = splitFunction(functionName);
        // E terminates the method name (and precedes the method parameters).
        // If eg "C4E" shows up in the mangled name for another reason, this
        // will create bogus edges in the callgraph. But it will affect little
        // and is somewhat difficult to avoid, so we will live with it.
        //
        // Another possibility! A templatized constructor will contain C4I...E
        // for template arguments.
        //
        for (let [synthetic, variant] of [
            ['C4E', 'C1E'],
            ['C4E', 'C2E'],
            ['C4E', 'C3E'],
            ['C4I', 'C1I'],
            ['C4I', 'C2I'],
            ['C4I', 'C3I']])
        {
            if (mangled.indexOf(synthetic) == -1)
                continue;

            let variant_mangled = mangled.replace(synthetic, variant);
            let variant_full = variant_mangled + "$" + unmangled;
            printOnce("D " + memo(variant_full) + " " + memo(functionName));
        }
    }

    // For destructors:
    //
    // I've never seen D4Ev() + D4Ev(int32), only one or the other. So
    // for a D4Ev of any sort, create:
    //
    //   D0() -> D1()  # deleting destructor calls complete destructor, then deletes
    //   D1() -> D2()  # complete destructor calls base destructor, then destroys virtual bases
    //   D2() -> D4(?) # base destructor might be aliased to unified destructor
    //                 # use whichever one is defined, in-charge or not.
    //                 # ('?') means either () or (int32).
    //
    // Note that this doesn't actually make sense -- D0 and D1 should be
    // in-charge, but gcc doesn't seem to give them the in-charge parameter?!
    //
    if (functionName.indexOf("D4Ev") != -1 && functionName.indexOf("::~") != -1) {
        const not_in_charge_dtor = functionName.replace("(int32)", "()");
        const D0 = not_in_charge_dtor.replace("D4Ev", "D0Ev");
        const D1 = not_in_charge_dtor.replace("D4Ev", "D1Ev");
        const D2 = not_in_charge_dtor.replace("D4Ev", "D2Ev");
        printOnce("D " + memo(D0) + " " + memo(D1));
        printOnce("D " + memo(D1) + " " + memo(D2));
        printOnce("D " + memo(D2) + " " + memo(functionName));
    }
}

for (var nameIndex = minStream; nameIndex <= maxStream; nameIndex++) {
    var name = xdb.read_key(nameIndex);
    var data = xdb.read_entry(name);
    process(name.readString(), JSON.parse(data.readString()));
    xdb.free_string(name);
    xdb.free_string(data);
}
