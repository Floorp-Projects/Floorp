/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');

// Functions come out of sixgill in the form "mangled|readable". The mangled
// name is Truth. One mangled name might correspond to multiple readable names,
// for multiple reasons, including (1) sixgill/gcc doesn't always qualify types
// the same way or de-typedef the same amount; (2) sixgill's output treats
// references and pointers the same, and so doesn't distinguish them, but C++
// treats them as separate for overloading and linking; (3) (identical)
// destructors sometimes have an int32 parameter, sometimes not.
//
// The readable names are useful because they're far more meaningful to the
// user, and are what should show up in reports and questions to mrgiggles. At
// least in most cases, it's fine to have the extra mangled name tacked onto
// the beginning for these.
//
// The strategy used is to separate out the pieces whenever they are read in,
// create a table mapping mangled names to (one of the) readable names, and
// use the mangled names in all computation.
//
// Note that callgraph.txt uses a compressed representation -- each name is
// mapped to an integer, and those integers are what is recorded in the edges.
// But the integers depend on the full name, whereas the true edge should only
// consider the mangled name. And some of the names encoded in callgraph.txt
// are FieldCalls, not just function names.

var readableNames = {}; // map from mangled name => list of readable names
var mangledName = {}; // map from demangled names => mangled names. Could be eliminated.
var calleesOf = {}; // map from mangled => list of tuples of {'callee':mangled, 'limits':intset}
var callersOf; // map from mangled => list of tuples of {'caller':mangled, 'limits':intset}
var gcFunctions = {}; // map from mangled callee => reason
var limitedFunctions = {}; // set of mangled names (map from mangled name => limit intset)
var gcEdges = {};

// Map from identifier to mangled name (or to a Class.Field)
var idToMangled = [""];

// Map from identifier to full "mangled$readable" name. Or sometimes to a
// Class.Field name.
var functionNames = [""];

var mangledToId = {};

// Returns whether the function was added. (It will be refused if it was
// already there, or if limits or annotations say it shouldn't be added.)
function addGCFunction(caller, reason, functionLimits)
{
    if (functionLimits[caller] & LIMIT_CANNOT_GC)
        return false;

    if (ignoreGCFunction(idToMangled[caller]))
        return false;

    if (!(caller in gcFunctions)) {
        gcFunctions[caller] = reason;
        return true;
    }

    return false;
}

// Every caller->callee callsite is associated with a limit saying what is
// allowed at that callsite (eg if it's in a GC suppression zone, it would have
// LIMIT_CANNOT_GC set.) A given caller might call the same callee multiple
// times, with different limits, so we want to associate the <caller,callee>
// edge with the intersection ('AND') of all of the callsites' limits.
//
// Scan through all call edges and intersect the limits for all matching
// <caller,callee> edges (so that the result is the least limiting of all
// matching edges.) Preserve the original order.
//
// During the same scan, build callersOf from calleesOf.
function merge_repeated_calls(calleesOf) {
    const callersOf = Object.create(null);

    for (const [caller, callee_limits] of Object.entries(calleesOf)) {
        const ordered_callees = [];

        // callee_limits is a list of {callee,limit} objects.
        const callee2limit = new Map();
        for (const {callee, limits} of callee_limits) {
            const prev_limits = callee2limit.get(callee);
            if (prev_limits === undefined) {
                callee2limit.set(callee, limits);
                ordered_callees.push(callee);
            } else {
                callee2limit.set(callee, prev_limits & limits);
            }
        }

        // Update the contents of callee_limits to contain a single entry for
        // each callee, with its limits set to the AND of the limits observed
        // at all callsites within this caller function.
        callee_limits.length = 0;
        for (const callee of ordered_callees) {
            const limits = callee2limit.get(callee);
            callee_limits.push({callee, limits});
            if (!(callee in callersOf))
                callersOf[callee] = [];
            callersOf[callee].push({caller, limits});
        }
    }

    return callersOf;
}

function loadCallgraph(file)
{
    const fieldCallLimits = {};
    const fieldCallCSU = new Map(); // map from full field name id => csu name
    const resolvedFieldCalls = new Set();

    // set of mangled names (map from mangled name => limit intset)
    var functionLimits = {};

    let numGCCalls = 0;

    for (let line of readFileLines_gen(file)) {
        line = line.replace(/\n/, "");

        let match;
        if (match = line.charAt(0) == "#" && /^\#(\d+) (.*)/.exec(line)) {
            assert(functionNames.length == match[1]);
            functionNames.push(match[2]);
            const [ mangled, readable ] = splitFunction(match[2]);
            if (mangled in readableNames)
                readableNames[mangled].push(readable);
            else
                readableNames[mangled] = [ readable ];
            mangledName[readable] = mangled;
            mangledToId[mangled] = idToMangled.length;
            idToMangled.push(mangled);
            continue;
        }
        let limits = 0;
        // Example line: D /17 6 7
        //
        // This means a direct call from 6 -> 7, but within a scope that
        // applies limits 0x1 and 0x10 to the callee.
        //
        // Look for a limit and remove it from the line if found.
        if (line.indexOf("/") != -1) {
            match = /^(..)\/(\d+) (.*)/.exec(line);
            line = match[1] + match[3];
            limits = match[2]|0;
        }
        const tag = line.charAt(0);
        if (match = tag == 'I' && /^I (\d+) VARIABLE ([^\,]*)/.exec(line)) {
            const caller = match[1]|0;
            const name = match[2];
            if (!indirectCallCannotGC(functionNames[caller], name) &&
                !(limits & LIMIT_CANNOT_GC))
            {
                addGCFunction(caller, "IndirectCall: " + name, functionLimits);
            }
        } else if (match = (tag == 'F' || tag == 'V') && /^[FV] (\d+) (\d+) CLASS (.*?) FIELD (.*)/.exec(line)) {
            const caller = match[1]|0;
            const fullfield = match[2]|0;
            const csu = match[3];
            const fullfield_str = csu + "." + match[4];
            assert(functionNames[fullfield] == fullfield_str);
            if (limits)
                fieldCallLimits[fullfield] = limits;
            addToKeyedList(calleesOf, caller, {callee:fullfield, limits});
            fieldCallCSU.set(fullfield, csu);
        } else if (match = tag == 'D' && /^D (\d+) (\d+)/.exec(line)) {
            const caller = match[1]|0;
            const callee = match[2]|0;
            addToKeyedList(calleesOf, caller, {callee:callee, limits:limits});
        } else if (match = tag == 'R' && /^R (\d+) (\d+)/.exec(line)) {
            const callerField = match[1]|0;
            const callee = match[2]|0;
            // Resolved virtual functions create a dummy node for the field
            // call, and callers call it. It will then call all possible
            // instantiations. No additional limits are placed on the callees;
            // it's as if there were a function named BaseClass.foo:
            //
            //     void BaseClass.foo() {
            //         Subclass1::foo();
            //         Subclass2::foo();
            //     }
            //
            addToKeyedList(calleesOf, callerField, {callee:callee, limits:0});
            // Mark that we resolved this virtual method, so that it isn't
            // assumed to call some random function that might do anything.
            resolvedFieldCalls.add(callerField);
        } else if (match = tag == 'T' && /^T (\d+) (.*)/.exec(line)) {
            const mangled = match[1]|0;
            let tag = match[2];
            if (tag == 'GC Call') {
                addGCFunction(mangled, "GC", functionLimits);
                numGCCalls++;
            }
        } else {
            assert(false, "Invalid format in callgraph line: " + line);
        }
    }

    // Callers have a list of callees, with duplicates (if the same function is
    // called more than once.) Merge the repeated calls, only keeping limits
    // that are in force for *every* callsite of that callee. Also, generate
    // the callersOf table at the same time.
    callersOf = merge_repeated_calls(calleesOf);

    // Add in any extra functions at the end. (If we did this early, it would
    // mess up the id <-> name correspondence. Also, we need to know if the
    // functions even exist in the first place.)
    for (var func of extraGCFunctions()) {
        addGCFunction(mangledToId[func], "annotation", functionLimits);
    }

    // Compute functionLimits: it should contain the set of functions that
    // are *always* called within some sort of limited context (eg GC
    // suppression).

    // Initialize functionLimits to the set of all functions, where each one
    // is maximally limited, and initialize the worklist to all toplevel
    // callers with dummy unlimited edges.
    var worklist = [];
    for (let callee in callersOf)
        functionLimits[callee] = LIMIT_UNVISITED;
    for (var caller in calleesOf) {
        if (!(caller in callersOf)) {
            functionLimits[caller] = LIMIT_UNVISITED;
            worklist.push([caller, LIMIT_NONE, 'root']);
        }
    }

    // Add in limited field calls.
    for (var [name, limits] of Object.entries(fieldCallLimits)) {
        if (limits)
            functionLimits[name] = limits;
    }

    // Recursively traverse the callgraph from the roots. Recurse through every
    // edge that weakens the limits. (Limits that entirely disappear, aka go to
    // a zero intset, will be removed from functionLimits.)
    while (worklist.length > 0) {
        // Consider caller where (graph) -> caller -> (0 or more callees)
        // 'callercaller' is for debugging.
        const [caller, edge_limits, callercaller] = worklist.pop();
        const prev_limits = functionLimits[caller];
        if (prev_limits & ~edge_limits) {
            // Turning off a limit (or unvisited marker). Must recurse to the
            // children. But first, update this caller's limits: we just found
            // out it is reachable by an unlimited path, so it must be treated
            // as unlimited (with respect to that bit).
            const new_limits = prev_limits & edge_limits;
            if (new_limits)
                functionLimits[caller] = new_limits;
            else
                delete functionLimits[caller];
            for (const {callee, limits} of (calleesOf[caller] || []))
                worklist.push([callee, limits | edge_limits, caller]);
        }
    }

    // Not all functions are reachable by searching backwards from GC
    // functions, so some functions will be LIMIT_UNVISITED. Remove them.
    for (var func in functionLimits) {
        if (functionLimits[func] == LIMIT_UNVISITED)
            delete functionLimits[func];
    }

    // Eliminate GC-limited functions from the set of functions known to GC.
    for (var name in gcFunctions) {
        if (functionLimits[name] & LIMIT_CANNOT_GC)
            delete gcFunctions[name];
    }

    // functionLimits should now contain all functions that are always called
    // in a limited context.

    // Sanity check to make sure the callgraph has some functions annotated as
    // GC Calls. This is mostly a check to be sure the earlier processing
    // succeeded (as opposed to, say, running on empty xdb files because you
    // didn't actually compile anything interesting.)
    assert(numGCCalls > 0, "No GC functions found!");

    // Initialize the worklist to all known gcFunctions.
    var worklist = [];
    for (const name in gcFunctions)
        worklist.push(name);

    // Include all field calls and unresolved virtual method calls.
    for (const [name, csuName] of fieldCallCSU) {
        if (resolvedFieldCalls.has(name))
            continue; // Skip resolved virtual functions.
        const fullFieldName = idToMangled[name];
        if (!fieldCallCannotGC(csuName, fullFieldName)) {
            gcFunctions[name] = 'unresolved ' + idToMangled[name];
            worklist.push(name);
        }
    }

    // Recursively find all callers not always called in a GC suppression
    // context, and add them to the set of gcFunctions.
    while (worklist.length) {
        name = worklist.shift();
        assert(name in gcFunctions);
        if (!(name in callersOf))
            continue;
        for (const {caller, limits} of callersOf[name]) {
            if (!(limits & LIMIT_CANNOT_GC)) {
                if (addGCFunction(caller, name, functionLimits))
                    worklist.push(caller);
            }
        }
    }

    // Convert functionLimits to limitedFunctions (using mangled names instead
    // of ids.)

    for (const [id, limits] of Object.entries(functionLimits))
        limitedFunctions[idToMangled[id]] = limits;

    // The above code uses integer ids for efficiency. External code uses
    // mangled names. Rewrite the various data structures to convert ids to
    // mangled names.
    remap_ids_to_mangled_names();
}

function remap_ids_to_mangled_names() {
    var tmp = gcFunctions;
    gcFunctions = {};
    for (const [caller, reason] of Object.entries(tmp))
        gcFunctions[idToMangled[caller]] = idToMangled[reason] || reason;

    tmp = calleesOf;
    calleesOf = {};
    for (const [callerId, callees] of Object.entries(calleesOf)) {
        const caller = idToMangled[callerId];
        for (const {calleeId, limits} of callees)
            calleesOf[caller][idToMangled[calleeId]] = limits;
    }

    tmp = callersOf;
    callersOf = {};
    for (const [calleeId, callers] of Object.entries(callersOf)) {
        const callee = idToMangled[calleeId];
        callersOf[callee] = {};
        for (const {callerId, limits} of callers)
            callersOf[callee][idToMangled[caller]] = limits;
    }
}
