/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
"use strict";

loadRelativeToScript('utility.js');
loadRelativeToScript('annotations.js');
loadRelativeToScript('loadCallgraph.js');

function usage() {
  throw "Usage: computeGCFunctions.js <rawcalls1.txt> <rawcalls2.txt>... --outputs <out:callgraph.txt> <out:gcFunctions.txt> <out:gcFunctions.lst> <out:gcEdges.txt> <out:limitedFunctions.lst>";
}

if (typeof scriptArgs[0] != 'string')
  usage();

var start = "Time: " + new Date;

var rawcalls_filenames = [];
while (scriptArgs.length) {
  const arg = scriptArgs.shift();
  if (arg == '--outputs')
    break;
  rawcalls_filenames.push(arg);
}
if (scriptArgs.length == 0)
  usage();

var callgraph_filename            = scriptArgs[0] || "callgraph.txt";
var gcFunctions_filename          = scriptArgs[1] || "gcFunctions.txt";
var gcFunctionsList_filename      = scriptArgs[2] || "gcFunctions.lst";
var gcEdges_filename              = scriptArgs[3] || "gcEdges.txt";
var limitedFunctionsList_filename = scriptArgs[4] || "limitedFunctions.lst";

var {
  gcFunctions,
  functions,
  calleesOf,
  limitedFunctions
} = loadCallgraph(rawcalls_filenames);

printErr("Writing " + gcFunctions_filename);
redirect(gcFunctions_filename);

for (var name in gcFunctions) {
    for (let readable of (functions.readableName[name] || [name])) {
        print("");
        const fullname = (name == readable) ? name : name + "$" + readable;
        print("GC Function: " + fullname);
        let current = name;
        do {
            current = gcFunctions[current];
            if (current === 'internal')
                ; // Hit the end
            else if (current in functions.readableName)
                print("    " + functions.readableName[current][0]);
            else
                print("    " + current);
        } while (current in gcFunctions);
    }
}

printErr("Writing " + gcFunctionsList_filename);
redirect(gcFunctionsList_filename);
for (var name in gcFunctions) {
    if (name in functions.readableName) {
        for (var readable of functions.readableName[name])
            print(name + "$" + readable);
    } else {
        print(name);
    }
}

// gcEdges is a list of edges that can GC for more specific reasons than just
// calling a function that is in gcFunctions.txt.
//
// Right now, it is unused. It was meant for ~AutoRealm when it might
// wrap an exception, but anything held live across ~AC will have to be held
// live across the corresponding constructor (and hence the whole scope of the
// AC), and in that case it'll be held live across whatever could create an
// exception within the AC scope. So ~AC edges are redundant. I will leave the
// stub machinery here for now.
printErr("Writing " + gcEdges_filename);
redirect(gcEdges_filename);
for (var block in gcEdges) {
  for (var edge in gcEdges[block]) {
      var func = gcEdges[block][edge];
    print([ block, edge, func ].join(" || "));
  }
}

printErr("Writing " + limitedFunctionsList_filename);
redirect(limitedFunctionsList_filename);
print(JSON.stringify(limitedFunctions, null, 4));

printErr("Writing " + callgraph_filename);
redirect(callgraph_filename);
saveCallgraph(functions, calleesOf);
