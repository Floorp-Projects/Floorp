/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

"use strict";

loadRelativeToScript('utility.js');
loadRelativeToScript('annotations.js');

function processCSU(csu, body)
{
    if (!("DataField" in body))
        return;
    for (var field of body.DataField) {
        var type = field.Field.Type;
        var fieldName = field.Field.Name[0];
        if (type.Kind == "Pointer") {
            var target = type.Type;
            if (target.Kind == "CSU")
                addNestedPointer(csu, target.Name, fieldName);
        }
        if (type.Kind == "CSU") {
            // Ignore nesting in classes which are AutoGCRooters. We only consider
            // types with fields that may not be properly rooted.
            if (type.Name == "JS::AutoGCRooter" || type.Name == "JS::CustomAutoRooter")
                return;
            addNestedStructure(csu, type.Name, fieldName);
        }
    }
}

var structureParents = {}; // Map from field => list of <parent, fieldName>
var pointerParents = {}; // Map from field => list of <parent, fieldName>

function addNestedStructure(csu, inner, field)
{
    if (!(inner in structureParents))
        structureParents[inner] = [];
    structureParents[inner].push([ csu, field ]);
}

function addNestedPointer(csu, inner, field)
{
    if (!(inner in pointerParents))
        pointerParents[inner] = [];
    pointerParents[inner].push([ csu, field ]);
}

var xdb = xdbLibrary();
xdb.open("src_comp.xdb");

var minStream = xdb.min_data_stream();
var maxStream = xdb.max_data_stream();

for (var csuIndex = minStream; csuIndex <= maxStream; csuIndex++) {
    var csu = xdb.read_key(csuIndex);
    var data = xdb.read_entry(csu);
    var json = JSON.parse(data.readString());
    assert(json.length == 1);
    processCSU(csu.readString(), json[0]);

    xdb.free_string(csu);
    xdb.free_string(data);
}

var gcTypes = {}; // map from parent struct => Set of GC typed children
var gcPointers = {}; // map from parent struct => Set of GC typed children
var gcFields = {};

function addGCType(typeName, child, why)
{
    if (!why)
        why = '<annotation>';
    if (!child)
        child = 'annotation';

    if (isRootedTypeName(typeName))
        return;

    if (!(typeName in gcTypes))
        gcTypes[typeName] = Set();
    gcTypes[typeName].add(why);

    if (!(typeName in gcFields))
        gcFields[typeName] = Map();
    gcFields[typeName].set(why, child);

    if (typeName in structureParents) {
        for (var field of structureParents[typeName]) {
            var [ holderType, fieldName ] = field;
            addGCType(holderType, typeName, fieldName);
        }
    }
    if (typeName in pointerParents) {
        for (var field of pointerParents[typeName]) {
            var [ holderType, fieldName ] = field;
            addGCPointer(holderType, typeName, fieldName);
        }
    }
}

function addGCPointer(typeName, child, why)
{
    if (!why)
        why = '<annotation>';
    if (!child)
        child = 'annotation';

    // Ignore types that are properly rooted.
    if (isRootedPointerTypeName(typeName))
        return;

    if (!(typeName in gcPointers))
        gcPointers[typeName] = Set();
    gcPointers[typeName].add(why);

    if (!(typeName in gcFields))
        gcFields[typeName] = Map();
    gcFields[typeName].set(why, child);

    if (typeName in structureParents) {
        for (var field of structureParents[typeName]) {
            var [ holder, fieldName ] = field;
            addGCPointer(holder, typeName, fieldName);
        }
    }
}

addGCType('JSObject');
addGCType('JSString');
addGCType('js::Shape');
addGCType('js::BaseShape');
addGCType('JSScript');
addGCType('js::LazyScript');
addGCType('js::ion::IonCode');
addGCPointer('JS::Value');
addGCPointer('jsid');
addGCPointer('JS::AutoCheckCannotGC');

function explain(csu, indent, seen) {
    if (!seen)
        seen = Set();
    seen.add(csu);
    if (!(csu in gcFields))
        return;
    if (gcFields[csu].has('<annotation>')) {
        print(indent + "because I said so");
        return;
    }
    for (var [ field, child ] of gcFields[csu]) {
        var inherit = "";
        if (field == "field:0")
            inherit = " (probably via inheritance)";
        print(indent + "contains field '" + field + "' of type " + child + inherit);
        if (!seen.has(child))
            explain(child, indent + "  ", seen);
    }
}

for (var csu in gcTypes) {
    print("GCThing: " + csu);
    explain(csu, "  ");
}
for (var csu in gcPointers) {
    print("GCPointer: " + csu);
    explain(csu, "  ");
}
