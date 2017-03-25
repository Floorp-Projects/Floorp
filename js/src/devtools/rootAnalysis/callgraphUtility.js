
var subclasses = new Map(); // Map from csu => set of immediate subclasses
var superclasses = new Map(); // Map from csu => set of immediate superclasses
var classFunctions = new Map(); // Map from "csu:name" => set of full method name

function addEntry(map, name, entry)
{
    if (!map.has(name))
        map.set(name, new Set());
    map.get(name).add(entry);
}

function fieldKey(csuName, field)
{
    // Note: not dealing with overloading correctly.
    var nargs = 0;
    if (field.Type.Kind == "Function" && "TypeFunctionArguments" in field.Type)
	nargs = field.Type.TypeFunctionArguments.length;
    return csuName + ":" + field.Name[0] + ":" + nargs;
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
	    var name = field.Variable.Name[0];
            addEntry(classFunctions, fieldKey(csuName, field.Field[0]), name);
	}
    }
}

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

// Return the nearest ancestor method definition, or all nearest definitions in
// the case of multiple inheritance.
function nearestAncestorMethods(csu, field)
{
    var key = fieldKey(csu, field);

  print("Scanning for nearest ancestor of " + key);
  
    if (classFunctions.has(key))
        return new Set(classFunctions.get(key));

    var functions = new Set();
    if (superclasses.has(csu)) {
        for (var parent of superclasses.get(csu))
            functions.update(nearestAncestorMethods(parent, field));
    }

    return functions;
}

// Return [ instantations, suppressed ], where instantiations is a Set of all
// possible implementations of 'field' given static type 'initialCSU', plus
// null if arbitrary other implementations are possible, and suppressed is true
// if we the method is assumed to be non-GC'ing by annotation.
function findVirtualFunctions(initialCSU, field)
{
    var fieldName = field.Name[0];
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
        if (isSuppressedVirtualMethod(csu, fieldName))
            return [ new Set(), true ];
        if (isOverridableField(initialCSU, csu, fieldName)) {
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
        var key = fieldKey(csu, field);

        if (classFunctions.has(key))
            functions.update(classFunctions.get(key));

        if (subclasses.has(csu))
            worklist.push(...subclasses.get(csu));
    }

    assert(functions.size > 0, "failed to find virtual function for " + fieldName);

    return [ functions, false ];
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
                [ functions, suppressed ] = findVirtualFunctions(csuName, field, suppressed);
                if (suppressed) {
                    // Field call known to not GC; mark it as suppressed so
                    // direct invocations will be ignored
                    callees.push({'kind': "field", 'csu': csuName, 'field': fieldName,
                                  'suppressed': true, 'isVirtual': true});
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
                    // be overridden in extensions. Use the isVirtual property
		    // so that callers can tell which case holds.
                    callees.push({'kind': "field", 'csu': csuName, 'field': fieldName,
				  'isVirtual': "FieldInstanceFunction" in field});
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
