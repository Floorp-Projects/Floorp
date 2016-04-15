// AutoEntryMonitor should catch all entry points into JavaScript.

load(libdir + 'array-compare.js');

function cold_and_warm(f, params, expected) {
  print(uneval(params));
  print(uneval(entryPoints(params)));
  assertEq(arraysEqual(entryPoints(params), expected), true);

  // Warm up the function a bit, so the JITs will compile it, and try again.
  if (f)
    for (i = 0; i < 10; i++)
      f();

  assertEq(arraysEqual(entryPoints(params), expected), true);
}

function entry1() { }
cold_and_warm(entry1, { function: entry1 }, [ "entry1" ]);

var getx = { get x() { } };
cold_and_warm(Object.getOwnPropertyDescriptor(getx, 'x').get,
              { object: getx, property: 'x' }, [ "getx.x" ]);

var sety = { set y(v) { } };
cold_and_warm(Object.getOwnPropertyDescriptor(sety, 'y').set,
              { object: sety, property: 'y', value: 'glerk' }, [ "sety.y" ]);

cold_and_warm(Object.prototype.toString, { ToString: {} }, []);

var toS = { toString: function myToString() { return "string"; } };
cold_and_warm(toS.toString, { ToString: toS }, [ "myToString" ]);

cold_and_warm(undefined, { ToNumber: {} }, []);

var vOf = { valueOf: function myValueOf() { return 42; } };
cold_and_warm(vOf.valueOf, { ToNumber: vOf }, [ "myValueOf" ]);

var toSvOf = { toString: function relations() { return Object; },
               valueOf: function wallpaper() { return 17; } };
cold_and_warm(() => {  toSvOf.toString(); toSvOf.valueOf(); },
              { ToString: toSvOf }, [ "relations", "wallpaper" ]);

var vOftoS = { toString: function ettes() { return "6-inch lengths"; },
               valueOf: function deathBy() { return Math; } };
cold_and_warm(() => {  vOftoS.valueOf(); vOftoS.toString(); },
              { ToNumber: vOftoS }, [ "deathBy", "ettes" ]);


cold_and_warm(() => 0, { eval: "Math.atan2(1,1);" }, [ "eval:entryPoint eval" ]);
