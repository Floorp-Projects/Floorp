/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

// Utility code for traversing the JSON data structures produced by sixgill.

"use strict";

var TRACING = false;

// Find all points (positions within the code) of the body given by the list of
// bodies and the blockId to match (which will specify an outer function or a
// loop within it), recursing into loops if needed.
function findAllPoints(bodies, blockId, bits)
{
    var points = [];
    var body;

    for (var xbody of bodies) {
        if (sameBlockId(xbody.BlockId, blockId)) {
            assert(!body);
            body = xbody;
        }
    }
    assert(body);

    if (!("PEdge" in body))
        return;
    for (var edge of body.PEdge) {
        points.push([body, edge.Index[0], bits]);
        if (edge.Kind == "Loop")
            points.push(...findAllPoints(bodies, edge.BlockId, bits));
    }

    return points;
}

// Visitor of a graph of <body, ppoint> vertexes and sixgill-generated edges,
// where the edges represent the actual computation happening.
//
// Uses the syntax `var Visitor = class { ... }` rather than `class Visitor`
// to allow reloading this file with the JS debugger.
var Visitor = class {
    constructor(bodies) {
        this.visited_bodies = new Map();
        for (const body of bodies) {
            this.visited_bodies.set(body, new Map());
        }
    }

    // Returns whether we should keep going after seeing this <body, ppoint>
    // pair. Also records it as visited.
    visit(body, ppoint, info) {
        const visited = this.visited_bodies.get(body);
        const existing = visited.get(ppoint);
        const action = this.next_action(existing, info);
        const merged = this.merge_info(existing, info);
        visited.set(ppoint, merged);
        return [action, merged];
    }

    // Default implementation does a basic "only visit nodes once" search.
    // (Whether this is BFS/DFS/other is determined by the caller.)

    // Override if you need to revisit nodes. Valid actions are "continue",
    // "prune", and "done". "continue" means continue with the search. "prune"
    // means stop at this node, only continue on other edges. "done" means the
    // whole search is complete even if unvisited nodes remain.
    next_action(prev, current) { return prev ? "prune" : "continue"; }

    // Update the info at a node. If this is the first time the node has been
    // seen, `prev` will be undefined. `current` will be the info computed by
    // `extend_path`. The node will be updated with the return value.
    merge_info(prev, current) { return true; }

    // Prepend `edge` to the info stored at the successor node, returning
    // the updated info value. This should be overridden by pretty much any
    // subclass, as a traversal's semantics are largely determined by this method.
    extend_path(edge, body, ppoint, successor_path) { return true; }
};

function findMatchingBlock(bodies, blockId) {
    for (const body of bodies) {
        if (sameBlockId(body.BlockId, blockId)) {
            return body;
        }
    }
    assert(false);
}

// Perform a mostly breadth-first search through the graph of <body, ppoints>.
// This is only mostly breadth-first because the visitor decides whether to
// stop searching when it sees an already-visited node. It can choose to
// re-visit a node in order to find "better" paths that include a node more
// than once.
//
// The return value depends on how the search finishes. If a 'done' action
// is returned by visitor.visit(), use the information returned by
// that call. If the search completes without reaching the entry point of
// the function (the "root"), return null. If the search manages to reach
// the root, return the value of the `result_if_reached_root` parameter.
//
// This allows this function to be used in different ways. If the visitor
// associates a value with each node that chains onto its successors
// (or predecessors in the "upwards" search order), then this will return
// a complete path through the graph. But this can also be used to test
// whether a condition holds (eg "the exit point is reachable after
// calling SomethingImportant()"), in which case no path is needed and the
// visitor will cause the return value to be a simple boolean (or null
// if it terminates the search before reaching the root.)
//
// The information returned by the visitor for a node is often called
// `path` in the code below, even though it may not represent a path.
//
function BFS_upwards(start_body, start_ppoint, bodies, visitor,
                     initial_successor_info={},
                     result_if_reached_root=null)
{
    const work = [[start_body, start_ppoint, null, initial_successor_info]];
    if (TRACING) {
        printErr(`BFS start at ${blockIdentifier(start_body)}:${start_ppoint}`);
    }

    let reached_root = false;
    while (work.length > 0) {
        const [body, ppoint, edgeToAdd, successor_path] = work.shift();
        if (TRACING) {
            printErr(`prepending edge from ${ppoint} to state '${successor_path}'`);
        }
        let path = visitor.extend_path(edgeToAdd, body, ppoint, successor_path);

        const [action,  merged_path] = visitor.visit(body, ppoint, path);
        if (action === "done") {
            return merged_path;
        }
        if (action === "prune") {
            // Do not push anything else to the work queue, but continue processing
            // other branches.
            continue;
        }
        assert(action == "continue");
        path = merged_path;

        const predecessors = getPredecessors(body);
        for (const edge of (predecessors[ppoint] || [])) {
            if (edge.Kind == "Loop") {
                // Propagate the search into the exit point of the loop body.
                const loopBody = findMatchingBlock(bodies, edge.BlockId);
                const loopEnd = loopBody.Index[1];
                work.push([loopBody, loopEnd, null, path]);
                // Don't continue to predecessors here without going through
                // the loop. (The points in this body that enter the loop will
                // be traversed when we reach the entry point of the loop.)
            } else {
                work.push([body, edge.Index[0], edge, path]);
            }
        }

        // Check for hitting the entry point of a loop body.
        if (ppoint == body.Index[0] && body.BlockId.Kind == "Loop") {
            // Propagate to outer body parents that enter the loop body.
            for (const parent of (body.BlockPPoint || [])) {
                const parentBody = findMatchingBlock(bodies, parent.BlockId);
                work.push([parentBody, parent.Index, null, path]);
            }

            // This point is also preceded by the *end* of this loop, for the
            // previous iteration.
            work.push([body, body.Index[1], null, path]);
        }

        // Check for reaching the root of the function.
        if (body === start_body && ppoint == body.Index[0]) {
            reached_root = true;
        }
    }

    // The search space was exhausted without finding a 'done' state. That
    // might be because all search paths were pruned before reaching the entry
    // point of the function, in which case reached_root will be false. (If
    // reached_root is true, then we may still not have visited the entire
    // graph, if some paths were pruned but at least one made it to the root.)
    return reached_root ? result_if_reached_root : null;
}

// Given the CFG for the constructor call of some RAII, return whether the
// given edge is the matching destructor call.
function isMatchingDestructor(constructor, edge)
{
    if (edge.Kind != "Call")
        return false;
    var callee = edge.Exp[0];
    if (callee.Kind != "Var")
        return false;
    var variable = callee.Variable;
    assert(variable.Kind == "Func");
    if (variable.Name[1].charAt(0) != '~')
        return false;

    // Note that in some situations, a regular function can begin with '~', so
    // we don't necessarily have a destructor in hand. This is probably a
    // sixgill artifact, but in js::wasm::ModuleGenerator::~ModuleGenerator, a
    // templatized static inline EraseIf is invoked, and it gets named ~EraseIf
    // for some reason.
    if (!("PEdgeCallInstance" in edge))
        return false;

    var constructExp = constructor.PEdgeCallInstance.Exp;
    assert(constructExp.Kind == "Var");

    var destructExp = edge.PEdgeCallInstance.Exp;
    if (destructExp.Kind != "Var")
        return false;

    return sameVariable(constructExp.Variable, destructExp.Variable);
}

// Return all calls within the RAII scope of any constructor matched by
// isConstructor(). (Note that this would be insufficient if you needed to
// treat each instance separately, such as when different regions of a function
// body were guarded by these constructors and you needed to do something
// different with each.)
function allRAIIGuardedCallPoints(typeInfo, bodies, body, isConstructor)
{
    if (!("PEdge" in body))
        return [];

    var points = [];

    for (var edge of body.PEdge) {
        if (edge.Kind != "Call")
            continue;
        var callee = edge.Exp[0];
        if (callee.Kind != "Var")
            continue;
        var variable = callee.Variable;
        assert(variable.Kind == "Func");
        const bits = isConstructor(typeInfo, edge.Type, variable.Name);
        if (!bits)
            continue;
        if (!("PEdgeCallInstance" in edge))
            continue;
        if (edge.PEdgeCallInstance.Exp.Kind != "Var")
            continue;

        points.push(...pointsInRAIIScope(bodies, body, edge, bits));
    }

    return points;
}

// Test whether the given edge is the constructor corresponding to the given
// destructor edge.
function isMatchingConstructor(destructor, edge)
{
    if (edge.Kind != "Call")
        return false;
    var callee = edge.Exp[0];
    if (callee.Kind != "Var")
        return false;
    var variable = callee.Variable;
    if (variable.Kind != "Func")
        return false;
    var name = readable(variable.Name[0]);
    var destructorName = readable(destructor.Exp[0].Variable.Name[0]);
    var match = destructorName.match(/^(.*?::)~(\w+)\(/);
    if (!match) {
        printErr("Unhandled destructor syntax: " + destructorName);
        return false;
    }
    var constructorSubstring = match[1] + match[2];
    if (name.indexOf(constructorSubstring) == -1)
        return false;

    var destructExp = destructor.PEdgeCallInstance.Exp;
    if (destructExp.Kind != "Var")
        return false;

    var constructExp = edge.PEdgeCallInstance.Exp;
    if (constructExp.Kind != "Var")
        return false;

    return sameVariable(constructExp.Variable, destructExp.Variable);
}

function findMatchingConstructor(destructorEdge, body, warnIfNotFound=true)
{
    var worklist = [destructorEdge];
    var predecessors = getPredecessors(body);
    while(worklist.length > 0) {
        var edge = worklist.pop();
        if (isMatchingConstructor(destructorEdge, edge))
            return edge;
        if (edge.Index[0] in predecessors) {
            for (var e of predecessors[edge.Index[0]])
                worklist.push(e);
        }
    }
    if (warnIfNotFound)
        printErr("Could not find matching constructor!");
    return undefined;
}

function pointsInRAIIScope(bodies, body, constructorEdge, bits) {
    var seen = {};
    var worklist = [constructorEdge.Index[1]];
    var points = [];
    while (worklist.length) {
        var point = worklist.pop();
        if (point in seen)
            continue;
        seen[point] = true;
        points.push([body, point, bits]);
        var successors = getSuccessors(body);
        if (!(point in successors))
            continue;
        for (var nedge of successors[point]) {
            if (isMatchingDestructor(constructorEdge, nedge))
                continue;
            if (nedge.Kind == "Loop")
                points.push(...findAllPoints(bodies, nedge.BlockId, bits));
            worklist.push(nedge.Index[1]);
        }
    }

    return points;
}

// Look at an invocation of a virtual method or function pointer contained in a
// field, and return the static type of the invocant (or the containing struct,
// for a function pointer field.)
function getFieldCallInstanceCSU(edge, field)
{
    if ("FieldInstanceFunction" in field) {
        // We have a 'this'.
        const instanceExp = edge.PEdgeCallInstance.Exp;
        if (instanceExp.Kind == 'Drf') {
            // somevar->foo()
            return edge.Type.TypeFunctionCSU.Type.Name;
        } else if (instanceExp.Kind == 'Fld') {
            // somevar.foo()
            return instanceExp.Field.FieldCSU.Type.Name;
        } else if (instanceExp.Kind == 'Index') {
            // A strange construct.
            // C++ code: static_cast<JS::CustomAutoRooter*>(this)->trace(trc);
            // CFG: Call(21,30, this*[-1]{JS::CustomAutoRooter}.trace*(trc*))
            return instanceExp.Type.Name;
        } else if (instanceExp.Kind == 'Var') {
            // C++: reinterpret_cast<SimpleTimeZone*>(gRawGMT)->~SimpleTimeZone();
            // CFG:
            //   # icu_64::SimpleTimeZone::icu_64::SimpleTimeZone.__comp_dtor
            //   [6,7] Call gRawGMT.icu_64::SimpleTimeZone.__comp_dtor ()
            return field.FieldCSU.Type.Name;
        } else {
            printErr("------------------ edge -------------------");
            printErr(JSON.stringify(edge, null, 4));
            printErr("------------------ field -------------------");
            printErr(JSON.stringify(field, null, 4));
            assert(false, `unrecognized FieldInstanceFunction Kind ${instanceExp.Kind}`);
        }
    } else {
        // somefar.foo() where somevar is a field of some CSU.
        return field.FieldCSU.Type.Name;
    }
}

// gcc uses something like "__dt_del " for virtual destructors that it
// generates.
function isSyntheticVirtualDestructor(funcName) {
    return funcName.endsWith(" ");
}

function typedField(field)
{
    if ("FieldInstanceFunction" in field) {
        // Virtual call
        //
        // This makes a minimal attempt at dealing with overloading, by
        // incorporating the number of parameters. So far, that is all that has
        // been needed. If more is needed, sixgill will need to produce a full
        // mangled type.
        const {Type, Name: [name]} = field;

        // Virtual destructors don't need a type or argument count,
        // and synthetic ones don't have them filled in.
        if (isSyntheticVirtualDestructor(name)) {
            return name;
        }

        var nargs = 0;
        if (Type.Kind == "Function" && "TypeFunctionArguments" in Type)
            nargs = Type.TypeFunctionArguments.Type.length;
        return name + ":" + nargs;
    } else {
        // Function pointer field
        return field.Name[0];
    }
}

function fieldKey(csuName, field)
{
    return csuName + "." + typedField(field);
}
