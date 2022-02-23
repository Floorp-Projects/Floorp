/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

// Utility code for traversing the JSON data structures produced by sixgill.

"use strict";

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
