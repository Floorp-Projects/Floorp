/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

var functionBodies;

function suppressAllPoints(id)
{
    var body = null;
    for (var xbody of functionBodies) {
        if (sameBlockId(xbody.BlockId, id)) {
            assert(!body);
            body = xbody;
        }
    }
    assert(body);

    if (!("PEdge" in body))
        return;
    for (var edge of body.PEdge) {
        body.suppressed[edge.Index[0]] = true;
        if (edge.Kind == "Loop")
            suppressAllPoints(edge.BlockId);
    }
}

function isMatchingDestructor(constructor, edge)
{
    if (edge.Kind != "Call")
        return false;
    var callee = edge.Exp[0];
    if (callee.Kind != "Var")
        return false;
    var variable = callee.Variable;
    assert(variable.Kind == "Func");
    if (!/::~/.test(variable.Name[0]))
        return false;

    var constructExp = constructor.PEdgeCallInstance.Exp;
    assert(constructExp.Kind == "Var");

    var destructExp = edge.PEdgeCallInstance.Exp;
    if (destructExp.Kind != "Var")
        return false;

    return sameVariable(constructExp.Variable, destructExp.Variable);
}

// Compute the points within a function body where GC is suppressed.
function computeSuppressedPoints(body)
{
    var successors = [];

    if (!("PEdge" in body))
        return;
    for (var edge of body.PEdge) {
        var source = edge.Index[0];
        if (!(source in successors))
            successors[source] = [];
        successors[source].push(edge);
    }

    for (var edge of body.PEdge) {
        if (edge.Kind != "Call")
            continue;
        var callee = edge.Exp[0];
        if (callee.Kind != "Var")
            continue;
        var variable = callee.Variable;
        assert(variable.Kind == "Func");
        if (!isSuppressConstructor(variable.Name[0]))
            continue;
        if (edge.PEdgeCallInstance.Exp.Kind != "Var")
            continue;

        var seen = [];
        var worklist = [edge.Index[1]];
        while (worklist.length) {
            var point = worklist.pop();
            if (point in seen)
                continue;
            seen[point] = true;
            body.suppressed[point] = true;
            if (!(point in successors))
                continue;
            for (var nedge of successors[point]) {
                if (isMatchingDestructor(edge, nedge))
                    continue;
                if (nedge.Kind == "Loop")
                    suppressAllPoints(nedge.BlockId);
                worklist.push(nedge.Index[1]);
            }
        }
    }

    return [];
}
