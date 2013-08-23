/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

var functionBodies;

function findAllPoints(blockId)
{
    var points = [];
    var body;

    for (var xbody of functionBodies) {
        if (sameBlockId(xbody.BlockId, blockId)) {
            assert(!body);
            body = xbody;
        }
    }
    assert(body);

    if (!("PEdge" in body))
        return;
    for (var edge of body.PEdge) {
        points.push([body, edge.Index[0]]);
        if (edge.Kind == "Loop")
            Array.prototype.push.apply(points, findAllPoints(edge.BlockId));
    }

    return points;
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

function allRAIIGuardedCallPoints(body, isConstructor)
{
    if (!("PEdge" in body))
        return [];

    var points = [];
    var successors = getSuccessors(body);

    for (var edge of body.PEdge) {
        if (edge.Kind != "Call")
            continue;
        var callee = edge.Exp[0];
        if (callee.Kind != "Var")
            continue;
        var variable = callee.Variable;
        assert(variable.Kind == "Func");
        if (!isConstructor(variable.Name[0]))
            continue;
        if (edge.PEdgeCallInstance.Exp.Kind != "Var")
            continue;

        Array.prototype.push.apply(points, pointsInRAIIScope(body, edge, successors));
    }

    return points;
}

// Compute the points within a function body where GC is suppressed.
function computeSuppressedPoints(body)
{
    for (var [pbody, id] of allRAIIGuardedCallPoints(body, isSuppressConstructor))
        pbody.suppressed[id] = true;
}
