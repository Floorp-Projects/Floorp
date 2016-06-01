/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');
loadRelativeToScript('annotations.js');
loadRelativeToScript('CFG.js');

var theFunctionNameToFind;
if (scriptArgs[0] == '--function') {
    theFunctionNameToFind = scriptArgs[1];
    scriptArgs = scriptArgs.slice(2);
}

var typeInfo_filename = scriptArgs[0] || "typeInfo.txt";

var subclasses = new Map(); // Map from csu => set of immediate subclasses
var superclasses = new Map(); // Map from csu => set of immediate superclasses
var classFunctions = new Map(); // Map from "csu:name" => set of full method name

var virtualResolutionsSeen = new Set();

function addEntry(map, name, entry)
{
    if (!map.has(name))
        map.set(name, new Set());
    map.get(name).add(entry);
}

// CSU is "Class/Struct/Union"
function processCSU(csuName, csu)
{
    if (!("FunctionField" in csu))
        return;
    for (var field of csu.FunctionField) {
        if (1 in field.Field) {
            var superclass = field.Field[1].Type.Name;
            var subclass = field.Field[1].FieldCSU.Type.Name;
            assert(subclass == csuName);
            addEntry(subclasses, superclass, subclass);
            addEntry(superclasses, subclass, superclass);
        }
        if ("Variable" in field) {
            // Note: not dealing with overloading correctly.
            var name = field.Variable.Name[0];
            var key = csuName + ":" + field.Field[0].Name[0];
            addEntry(classFunctions, key, name);
        }
    }
}

// Return the nearest ancestor method definition, or all nearest definitions in
// the case of multiple inheritance.
function nearestAncestorMethods(csu, method)
{
    var key = csu + ":" + method;

    if (classFunctions.has(key))
        return new Set(classFunctions.get(key));

    var functions = new Set();
    if (superclasses.has(csu)) {
        for (var parent of superclasses.get(csu))
            functions.update(nearestAncestorMethods(parent, method));
    }

    return functions;
}

// Return [ instantations, suppressed ], where instantiations is a Set of all
// possible implementations of 'field' given static type 'initialCSU', plus
// null if arbitrary other implementations are possible, and suppressed is true
// if we the method is assumed to be non-GC'ing by annotation.
function findVirtualFunctions(initialCSU, field)
{
    var worklist = [initialCSU];
    var functions = new Set();

    // Loop through all methods of initialCSU (by looking at all methods of ancestor csus).
    //
    // If field is nsISupports::AddRef or ::Release, return an empty list and a
    // boolean that says we assert that it cannot GC.
    //
    // If this is a method that is annotated to be dangerous (eg, it could be
    // overridden with an implementation that could GC), then use null as a
    // signal value that it should be considered to GC, even though we'll also
    // collect all of the instantiations for other purposes.

    while (worklist.length) {
        var csu = worklist.pop();
        if (isSuppressedVirtualMethod(csu, field))
            return [ new Set(), true ];
        if (isOverridableField(initialCSU, csu, field)) {
            // We will still resolve the virtual function call, because it's
            // nice to have as complete a callgraph as possible for other uses.
            // But push a token saying that we can run arbitrary code.
            functions.add(null);
        }

        if (superclasses.has(csu))
            worklist.push(...superclasses.get(csu));
    }

    // Now return a list of all the instantiations of the method named 'field'
    // that could execute on an instance of initialCSU or a descendant class.

    // Start with the class itself, or if it doesn't define the method, all
    // nearest ancestor definitions.
    functions.update(nearestAncestorMethods(initialCSU, field));

    // Then recurse through all descendants to add in their definitions.
    var worklist = [initialCSU];
    while (worklist.length) {
        var csu = worklist.pop();
        var key = csu + ":" + field;

        if (classFunctions.has(key))
            functions.update(classFunctions.get(key));

        if (subclasses.has(csu))
            worklist.push(...subclasses.get(csu));
    }

    return [ functions, false ];
}

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

// Return a list of all callees that the given edge might be a call to. Each
// one is represented by an object with a 'kind' field that is one of
// ('direct', 'field', 'resolved-field', 'indirect', 'unknown'), though note
// that 'resolved-field' is really a global record of virtual method
// resolutions, indepedent of this particular edge.
function getCallees(edge)
{
    if (edge.Kind != "Call")
        return [];

    var callee = edge.Exp[0];
    var callees = [];
    if (callee.Kind == "Var") {
        assert(callee.Variable.Kind == "Func");
        callees.push({'kind': 'direct', 'name': callee.Variable.Name[0]});
    } else {
        assert(callee.Kind == "Drf");
        if (callee.Exp[0].Kind == "Fld") {
            var field = callee.Exp[0].Field;
            var fieldName = field.Name[0];
            var csuName = field.FieldCSU.Type.Name;
            var functions;
            if ("FieldInstanceFunction" in field) {
                let suppressed;
                [ functions, suppressed ] = findVirtualFunctions(csuName, fieldName, suppressed);
                if (suppressed) {
                    // Field call known to not GC; mark it as suppressed so
                    // direct invocations will be ignored
                    callees.push({'kind': "field", 'csu': csuName, 'field': fieldName,
                                  'suppressed': true});
                }
            } else {
                functions = new Set([null]); // field call
            }

            // Known set of virtual call targets. Treat them as direct calls to
            // all possible resolved types, but also record edges from this
            // field call to each final callee. When the analysis is checking
            // whether an edge can GC and it sees an unrooted pointer held live
            // across this field call, it will know whether any of the direct
            // callees can GC or not.
            var targets = [];
            var fullyResolved = true;
            for (var name of functions) {
                if (name === null) {
                    // Unknown set of call targets, meaning either a function
                    // pointer call ("field call") or a virtual method that can
                    // be overridden in extensions.
                    callees.push({'kind': "field", 'csu': csuName, 'field': fieldName});
                    fullyResolved = false;
                } else {
                    callees.push({'kind': "direct", 'name': name});
                    targets.push({'kind': "direct", 'name': name});
                }
            }
            if (fullyResolved)
                callees.push({'kind': "resolved-field", 'csu': csuName, 'field': fieldName, 'callees': targets});
        } else if (callee.Exp[0].Kind == "Var") {
            // indirect call through a variable.
            callees.push({'kind': "indirect", 'variable': callee.Exp[0].Variable.Name[0]});
        } else {
            // unknown call target.
            callees.push({'kind': "unknown"});
        }
    }

    return callees;
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
                var { csu, field } = callee;
                printOnce("F " + prologue + "CLASS " + csu + " FIELD " + field);
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

GCSuppressionTypes = loadTypeInfo(typeInfo_filename)["Suppress GC"] || [];

var xdb = xdbLibrary();
xdb.open("src_comp.xdb");

var minStream = xdb.min_data_stream();
var maxStream = xdb.max_data_stream();

for (var csuIndex = minStream; csuIndex <= maxStream; csuIndex++) {
    var csu = xdb.read_key(csuIndex);
    var data = xdb.read_entry(csu);
    var json = JSON.parse(data.readString());
    processCSU(csu.readString(), json[0]);

    xdb.free_string(csu);
    xdb.free_string(data);
}

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
        for (var [pbody, id] of allRAIIGuardedCallPoints(functionBodies, body, isSuppressConstructor))
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
        print("D " + memo(inChargeXTor) + " " + memo(functionName));

        // Bug 1056410: Oh joy. GCC does something even funkier internally,
        // where it generates calls to ~Foo() but a body for ~Foo(int32) even
        // though it uses the same mangled name for both. So we need to add a
        // synthetic edge from ~Foo() -> ~Foo(int32).
        //
        // inChargeXTor will have the (int32).
        if (functionName.indexOf("::~") > 0) {
            var calledDestructor = inChargeXTor.replace("(int32)", "()");
            print("D " + memo(calledDestructor) + " " + memo(inChargeXTor));
        }
    }

    // Further note: from http://mentorembedded.github.io/cxx-abi/abi.html the
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
    if (functionName.indexOf("C4E") != -1 || functionName.indexOf("D4Ev") != -1) {
        var [ mangled, unmangled ] = splitFunction(functionName);
        // E terminates the method name (and precedes the method parameters).
        // If eg "C4E" shows up in the mangled name for another reason, this
        // will create bogus edges in the callgraph. But will affect little and
        // is somewhat difficult to avoid, so we will live with it.
        for (let [synthetic, variant] of [['C4E', 'C1E'],
                                          ['C4E', 'C2E'],
                                          ['C4E', 'C3E'],
                                          ['D4Ev', 'D1Ev'],
                                          ['D4Ev', 'D2Ev'],
                                          ['D4Ev', 'D3Ev']])
        {
            if (mangled.indexOf(synthetic) == -1)
                continue;

            let variant_mangled = mangled.replace(synthetic, variant);
            let variant_full = variant_mangled + "$" + unmangled;
            print("D " + memo(variant_full) + " " + memo(functionName));
        }
    }
}

for (var nameIndex = minStream; nameIndex <= maxStream; nameIndex++) {
    var name = xdb.read_key(nameIndex);
    var data = xdb.read_entry(name);
    process(name.readString(), JSON.parse(data.readString()));
    xdb.free_string(name);
    xdb.free_string(data);
}
