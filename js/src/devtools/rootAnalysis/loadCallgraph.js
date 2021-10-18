/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');

// Functions come out of sixgill in the form "mangled$readable". The mangled
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
// create a table mapping mangled names to all readable names, and use the
// mangled names in all computation -- except for limited circumstances when
// the readable name is used in annotations.
//
// Note that callgraph.txt uses a compressed representation -- each name is
// mapped to an integer, and those integers are what is recorded in the edges.
// But the integers depend on the full name, whereas the true edge should only
// consider the mangled name. And some of the names encoded in callgraph.txt
// are FieldCalls, not just function names.

var readableNames = {}; // map from mangled name => list of readable names
var calleesOf = {}; // map from mangled => list of tuples of {'callee':mangled, 'any':intset, 'all':intset}
var callersOf; // map from mangled => list of tuples of {'caller':mangled, 'any':intset, 'all':intset}
var gcFunctions = {}; // map from mangled callee => reason
var limitedFunctions = {}; // set of mangled names (map from mangled name => [any,all intsets])
var gcEdges = {};

// "Map" from identifier to mangled name, or sometimes to a Class.Field name.
var functionNames = [""];

var mangledToId = {};

// Returns whether the function was added. (It will be refused if it was
// already there, or if attrs or annotations say it shouldn't be added.)
function addGCFunction(caller, reason, functionAttrs)
{
    if (functionAttrs[caller] && functionAttrs[caller][1] & ATTR_GC_SUPPRESSED)
        return false;

    if (ignoreGCFunction(functionNames[caller]))
        return false;

    if (!(caller in gcFunctions)) {
        gcFunctions[caller] = reason;
        return true;
    }

    return false;
}

// Every caller->callee callsite is associated with attrs saying what is
// allowed at that callsite (eg if it's in a GC suppression zone, it would have
// ATTR_GC_SUPPRESSED set.) A given caller might call the same callee multiple
// times, with different attributes. Associate the <caller,callee> edge with
// the intersection (AND) and disjunction (OR) of all of the callsites' attrs.
// The AND ('all') says what attributes are present for all callers; the OR
// ('any') says what attributes are present on any caller. Preserve the
// original order.
//
// During the same scan, build callersOf from calleesOf.
function merge_repeated_calls(calleesOf) {
    const callersOf = Object.create(null);

    for (const [caller_prop, callee_attrs] of Object.entries(calleesOf)) {
        const caller = caller_prop|0;
        const ordered_callees = [];

        // callee_attrs is a list of {callee,any,all} objects.
        const callee2any = new Map();
        const callee2all = new Map();
        for (const {callee, any, all} of callee_attrs) {
            const prev_any = callee2any.get(callee);
            if (prev_any === undefined) {
                assert(!callee2all.has(callee));
                callee2any.set(callee, any);
                callee2all.set(callee, all);
                ordered_callees.push(callee);
            } else {
                const prev_all = callee2all.get(callee);
                callee2any.set(callee, prev_any | any);
                callee2all.set(callee, prev_all & all);
            }
        }

        // Update the contents of callee_attrs to contain a single entry for
        // each callee, with its attrs set to the AND of the attrs observed at
        // all callsites within this caller function.
        callee_attrs.length = 0;
        for (const callee of ordered_callees) {
            const any = callee2any.get(callee);
            const all = callee2all.get(callee);
            callee_attrs.push({callee, any, all});
            if (!(callee in callersOf))
                callersOf[callee] = [];
            callersOf[callee].push({caller, any, all});
        }
    }

    return callersOf;
}

function loadCallgraph(file)
{
    const fieldCallAttrs = {};
    const fieldCallCSU = new Map(); // map from full field name id => csu name

    // set of mangled names (map from mangled name => [any,all])
    var functionAttrs = {};

    let numGCCalls = 0;

    for (let line of readFileLines_gen(file)) {
        line = line.replace(/\n/, "");

        let match;
        if (match = line.charAt(0) == "#" && /^\#(\d+) (.*)/.exec(line)) {
            const [ _, id, mangled ] = match;
            assert(functionNames.length == id);
            functionNames.push(mangled);
            mangledToId[mangled] = id;
            continue;
        }
        if (match = line.charAt(0) == "=" && /^= (\d+) (.*)/.exec(line)) {
            const [ _, id, readable ] = match;
            const mangled = functionNames[id];
            if (mangled in readableNames)
                readableNames[mangled].push(readable);
            else
                readableNames[mangled] = [ readable ];
            continue;
        }

        let attrs = 0;
        // Example line: D /17 6 7
        //
        // This means a direct call from 6 -> 7, but within a scope that
        // applies attrs 0x1 and 0x10 to the callee.
        //
        // Look for a bit specifier and remove it from the line if found.
        if (line.indexOf("/") != -1) {
            match = /^(..)\/(\d+) (.*)/.exec(line);
            line = match[1] + match[3];
            attrs = match[2]|0;
        }
        const tag = line.charAt(0);
        if (match = tag == 'I' && /^I (\d+) VARIABLE ([^\,]*)/.exec(line)) {
            const caller = match[1]|0;
            const name = match[2];
            if (!indirectCallCannotGC(functionNames[caller], name) &&
                !(attrs & ATTR_GC_SUPPRESSED))
            {
                // FIXME: Should emit this regardless, with attrs saying it
                // cannot GC.
                addGCFunction(caller, "IndirectCall: " + name, functionAttrs);
            }
        } else if (match = tag == 'F' && /^F (\d+) (\d+) CLASS (.*?) FIELD (.*)/.exec(line)) {
            const caller = match[1]|0;
            const fullfield = match[2]|0;
            const csu = match[3];
            const fullfield_str = csu + "." + match[4];
            assert(functionNames[fullfield] == fullfield_str);
            if (attrs)
                fieldCallAttrs[fullfield] = attrs;
            addToKeyedList(calleesOf, caller, {callee:fullfield, any:attrs, all:attrs});
            fieldCallCSU.set(fullfield, csu);
        } else if (match = tag == 'V' && /^V (\d+) (\d+) CLASS (.*?) FIELD (.*)/.exec(line)) {
            // V tag is no longer used, but we are still emitting it becasue it
            // can be helpful to understand what's going on.
        } else if (match = tag == 'D' && /^D (\d+) (\d+)/.exec(line)) {
            const caller = match[1]|0;
            const callee = match[2]|0;
            addToKeyedList(calleesOf, caller, {callee, any:attrs, all:attrs});
        } else if (match = tag == 'R' && /^R (\d+) (\d+)/.exec(line)) {
            assert(false, "R tag is no longer used");
        } else if (match = tag == 'T' && /^T (\d+) (.*)/.exec(line)) {
            const id = match[1]|0;
            let tag = match[2];
            if (tag == 'GC Call') {
                addGCFunction(id, "GC", functionAttrs);
                numGCCalls++;
            }
        } else {
            assert(false, "Invalid format in callgraph line: " + line);
        }
    }

    // Callers have a list of callees, with duplicates (if the same function is
    // called more than once.) Merge the repeated calls, maintaining 'any' and
    // 'all'. Also, generate the callersOf table at the same time.
    callersOf = merge_repeated_calls(calleesOf);

    // Add in any extra functions at the end. (If we did this early, it would
    // mess up the id <-> name correspondence. Also, we need to know if the
    // functions even exist in the first place.)
    for (var func of extraGCFunctions()) {
        addGCFunction(mangledToId[func], "annotation", functionAttrs);
    }
    const unknown = mangledToId['(unknown-definition)'];
    if (unknown) {
        addGCFunction(unknown, "internal", functionAttrs);
    }

    // Compute functionAttrs: it should contain the set of functions that are
    // *always* called within some sort of context (eg GC suppression).

    // Initialize to field calls with attrs set.
    for (var [name, attrs] of Object.entries(fieldCallAttrs))
        functionAttrs[name] = [attrs, attrs];

    // Initialize functionAttrs to the set of all functions, where each one is
    // maximally attributed, and return a worklist containing all simple roots
    // (nodes with no callers).
    const simple_roots = gather_simple_roots(functionAttrs, callersOf);

    // Traverse the graph, spreading the attrs down from the roots.
    propagate_attrs(simple_roots, functionAttrs, calleesOf);

    // There are a surprising number of "recursive roots", where there is a
    // cycle of functions calling each other but not called by anything else,
    // and these roots may also have descendants. Now that the above traversal
    // has eliminated everything reachable from simple roots, traverse the
    // remaining graph to gather up a representative function from each root
    // cycle.
    //
    // Simple example: in the JS shell build, moz_xstrdup calls itself, but
    // there are no calls to it from within js/src.
    const recursive_roots = gather_recursive_roots(functionAttrs, calleesOf, callersOf);

    // And do a final traversal starting with the recursive roots.
    propagate_attrs(recursive_roots, functionAttrs, calleesOf);

    for (const [f, [any, all]] of Object.entries(functionAttrs)) {
        // Throw out all functions with no attrs set, to reduce the size of the
        // output. From now on, "not in functionAttrs" means [any=0, all=0].
        if (any == 0 && all == 0)
            delete functionAttrs[f];

        // Remove GC-suppressed functions from the set of functions known to GC.
        if (all & ATTR_GC_SUPPRESSED)
            delete gcFunctions[name];
    }

    // functionAttrs now contains all functions that are ever called in an
    // attributed context, based on the known callgraph (i.e., calls through
    // function pointers are not taken into consideration.)

    // Sanity check to make sure the callgraph has some functions annotated as
    // GC Calls. This is mostly a check to be sure the earlier processing
    // succeeded (as opposed to, say, running on empty xdb files because you
    // didn't actually compile anything interesting.)
    assert(numGCCalls > 0, "No GC functions found!");

    // Initialize the worklist to all known gcFunctions.
    var worklist = [];
    for (const name in gcFunctions)
        worklist.push(name);

    // Add all field calls (but not virtual method calls) to gcFunctions.
    for (const [name, csuName] of fieldCallCSU) {
        const fullFieldName = functionNames[name];
        if (!fieldCallCannotGC(csuName, fullFieldName)) {
            gcFunctions[name] = 'arbitrary function pointer ' + fullFieldName;
            worklist.push(name);
        }
    }

    // Recursively find all callers not always called in a GC suppression
    // context, and add them to the set of gcFunctions.
    while (worklist.length) {
        name = worklist.shift();
        assert(name in gcFunctions, "gcFunctions does not contain " + name);
        if (!(name in callersOf))
            continue;
        for (const {caller, any, all} of callersOf[name]) {
            if (!(all & ATTR_GC_SUPPRESSED)) {
                if (addGCFunction(caller, name, functionAttrs))
                    worklist.push(caller);
            }
        }
    }

    // Convert functionAttrs to limitedFunctions (using mangled names instead
    // of ids.)

    for (const [id, [any, all]] of Object.entries(functionAttrs))
        limitedFunctions[functionNames[id]] = { attributes: all };

    for (const [id, limits, label] of recursive_roots) {
        const name = functionNames[id];
        const s = limitedFunctions[name] || (limitedFunctions[name] = {});
        s.recursive_root = true;
    }

    // The above code uses integer ids for efficiency. External code uses
    // mangled names. Rewrite the various data structures to convert ids to
    // mangled names.
    remap_ids_to_mangled_names();
}

// Return a worklist of functions with no callers, and also initialize
// functionAttrs to the set of all functions, each mapped to
// [ATTRS_NONE, ATTRS_UNVISITED].
function gather_simple_roots(functionAttrs, callersOf) {
    const roots = [];
    for (let callee in callersOf)
        functionAttrs[callee] = [ATTRS_NONE, ATTRS_UNVISITED];
    for (let caller in calleesOf) {
        functionAttrs[caller] = [ATTRS_NONE, ATTRS_UNVISITED];
        if (!(caller in callersOf))
            roots.push([caller, ATTRS_NONE, 'root']);
    }

    return roots;
}

// Recursively traverse the callgraph from the roots. Recurse through every
// edge that weakens the attrs. (Attrs that entirely disappear, ie go to a zero
// intset, will be removed from functionAttrs.)
function propagate_attrs(roots, functionAttrs, calleesOf) {
    const worklist = Array.from(roots);
    let top = worklist.length;
    while (top > 0) {
        // Consider caller where (graph) -> caller -> (0 or more callees)
        // 'callercaller' is for debugging.
        const [caller, edge_attrs, callercaller] = worklist[--top];
        const [prev_any, prev_all] = functionAttrs[caller];
        assert(prev_any !== undefined);
        assert(prev_all !== undefined);
        const [new_any, new_all] = [prev_any | edge_attrs, prev_all & edge_attrs];
        if (prev_any != new_any || prev_all != new_all) {
            // Update function attrs, then recurse to the children if anything
            // was updated.
            functionAttrs[caller] = [new_any, new_all];
            for (const {callee, any, all} of (calleesOf[caller] || []))
                worklist[top++] = [callee, all | edge_attrs, caller];
        }
    }
}

// Mutually-recursive roots and their descendants will not have been visited,
// and will still be set to [0, ATTRS_UNVISITED]. Scan through and gather them.
function gather_recursive_roots(functionAttrs, calleesOf, callersOf) {
    const roots = [];

    // Pick any node. Mark everything reachable by adding to a 'seen' set. At
    // the end, if there are any incoming edges to that node from an unmarked
    // node, then it is not a root. Otherwise, mark the node as a root. (There
    // will be at least one back edge coming into the node from a marked node
    // in this case, since otherwise it would have already been considered to
    // be a root.)
    //
    // Repeat with remaining unmarked nodes until all nodes are marked.
    const seen = new Set();
    for (let [func, [any, all]] of Object.entries(functionAttrs)) {
        func = func|0;
        if (all != ATTRS_UNVISITED)
            continue;

        // We should only be looking at nodes with callers, since otherwise
        // they would have been handled in the previous pass!
        assert(callersOf[func].length > 0);

        if (seen.has(func))
            continue;

        const work = [func];
        while (work.length > 0) {
            const f = work.pop();
            for (const callee of (calleesOf[f] || []).map(o => o.callee)) {
                if (!seen.has(callee) &&
                    callee != func &&
                    functionAttrs[callee][1] == ATTRS_UNVISITED)
                {
                    work.push(callee);
                    seen.add(callee);
                }
            }
        }

        assert(!seen.has(func));
        seen.add(func);
        if (callersOf[func].findIndex(o => !seen.has(o.caller)) == -1) {
            // No unmarked incoming edges, including self-edges, so this is a
            // (recursive) root.
            roots.push([func, ATTRS_NONE, 'recursive-root']);
        }
    }

    return roots;
}

function remap_ids_to_mangled_names() {
    var tmp = gcFunctions;
    gcFunctions = {};
    for (const [caller, reason] of Object.entries(tmp))
        gcFunctions[functionNames[caller]] = functionNames[reason] || reason;
}
