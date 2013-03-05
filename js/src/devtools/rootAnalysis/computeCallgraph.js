/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

"use strict";

load('utility.js');
load('annotations.js');
load('suppressedPoints.js');

var subclasses = {};
var classFunctions = {};

function processCSU(csuName, csu)
{
    if (!("FunctionField" in csu))
        return;
    for (var field of csu.FunctionField) {
        if (1 in field.Field) {
            var superclass = field.Field[1].Type.Name;
            var subclass = field.Field[1].FieldCSU.Type.Name;
            assert(subclass == csuName);
            if (!(superclass in subclasses))
                subclasses[superclass] = [];
            var found = false;
            for (var sub of subclasses[superclass]) {
                if (sub == subclass)
                    found = true;
            }
            if (!found)
                subclasses[superclass].push(subclass);
        }
        if ("Variable" in field) {
            // Note: not dealing with overloading correctly.
            var name = field.Variable.Name[0];
            var key = csuName + ":" + field.Field[0].Name[0];
            if (!(key in classFunctions))
                classFunctions[key] = [];
            classFunctions[key].push(name);
        }
    }
}

function findVirtualFunctions(csu, field)
{
    var functions = [];
    var worklist = [csu];

    while (worklist.length) {
        var csu = worklist.pop();
        var key = csu + ":" + field;

        if (key in classFunctions) {
            for (var name of classFunctions[key])
                functions.push(name);
        }

        if (csu in subclasses) {
            for (var subclass of subclasses[csu])
                worklist.push(subclass);
        }
    }

    return functions;
}

var memoized = {};
var memoizedCount = 0;

function memo(name)
{
    if (!(name in memoized)) {
        memoizedCount++;
        memoized[name] = "" + memoizedCount;
        print("#" + memoizedCount + " " + name);
    }
    return memoized[name];
}

function processBody(caller, body)
{
    if (!('PEdge' in body))
        return;
    for (var edge of body.PEdge) {
        if (edge.Kind != "Call")
            continue;
        var callee = edge.Exp[0];
        var suppressText = (edge.Index[0] in body.suppressed) ? "SUPPRESS_GC " : "";
        var prologue = suppressText + memo(caller) + " ";
        if (callee.Kind == "Var") {
            assert(callee.Variable.Kind == "Func");
            var name = callee.Variable.Name[0];
            print("D " + prologue + memo(name));
            var otherName = otherDestructorName(name);
            if (otherName)
                print("D " + prologue + memo(otherName));
        } else {
            assert(callee.Kind == "Drf");
            if (callee.Exp[0].Kind == "Fld") {
                var field = callee.Exp[0].Field;
                if ("FieldInstanceFunction" in field) {
                    // virtual function call.
                    var functions = findVirtualFunctions(field.FieldCSU.Type.Name, field.Name[0]);
                    for (var name of functions)
                        print("D " + prologue + memo(name));
                } else {
                    // indirect call through a field.
                    print("F " + prologue +
                          "CLASS " + field.FieldCSU.Type.Name +
                          " FIELD " + field.Name[0]);
                }
            } else if (callee.Exp[0].Kind == "Var") {
                // indirect call through a variable.
                print("I " + prologue +
                      "VARIABLE " + callee.Exp[0].Variable.Name[0]);
            } else {
                // unknown call target.
                print("I " + prologue + "VARIABLE UNKNOWN");
            }
        }
    }
}

var callgraph = {};

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

var minStream = xdb.min_data_stream();
var maxStream = xdb.max_data_stream();

for (var nameIndex = minStream; nameIndex <= maxStream; nameIndex++) {
    var name = xdb.read_key(nameIndex);
    var data = xdb.read_entry(name);
    functionBodies = JSON.parse(data.readString());
    for (var body of functionBodies)
        body.suppressed = [];
    for (var body of functionBodies)
        computeSuppressedPoints(body);
    for (var body of functionBodies)
        processBody(name.readString(), body);

    xdb.free_string(name);
    xdb.free_string(data);
}
