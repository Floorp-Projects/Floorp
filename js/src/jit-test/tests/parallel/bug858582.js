// |jit-test| error: TypeError
// Don't crash.
if (getBuildConfiguration().parallelJS) {
gczeal(2);
evaluate("\
function assertAlmostEq(v1, v2) {\
  print(\"v2 = \" + v2);\
  print(\"% diff = \" + percent);\
function assertStructuralEq(e1, e2) {}\
function assertEqParallelArrayArray(a, b) {\
    try {} catch (e) {\
      print(\"...in index \", i, \" of \", l);\
  }\
}\
    function assertEqArray(a, b) {\
      try {} catch (e) {}\
}\
function assertEqParallelArray(a, b) {\
  var shape = a.shape;\
  function bump(indices) {\
  var iv = shape.map(function () { return 0; });\
      print(\"...in indices \", iv, \" of \", shape);\
    }\
  } while (bump(iv));\
}\
function assertParallelArrayModesEq(modes, acc, opFunction, cmpFunction) {\
  modes.forEach(function (mode) {\
    var result = opFunction({ mode: mode, expect: \"success\" });\
    cmpFunction(acc, result);\
  });\
function assertParallelArrayModesCommute(modes, opFunction) {\
    var acc = opFunction({ mode: modes[0], expect: \"success\" });\
}\
function comparePerformance(opts) {\
        print(\"Option \" + opts[i].name + \" took \" + diff + \"ms\");\
        print(\"Option \" + opts[i].name + \" relative to option \" +\
              opts[0].name + \": \" + (rel|0) + \"%\");\
    }\
}\
function compareAgainstArray(jsarray, opname, func, cmpFunction) {\
  var expected = jsarray[opname].apply(jsarray, [func]);\
  var parray = new ParallelArray(jsarray);\
  assertParallelArrayModesEq([\"seq\", \"par\", \"par\"], expected, function(m) {\
    var result = parray[opname].apply(parray, [func, m]);\
  }, cmpFunction);\
}\
function testFilter(jsarray, func, cmpFunction) {}\
", { noScriptRval : true });
  compareAgainstArray([
	"a", 
	"b", 
	('captures: 1,1; RegExp.leftContext: ""; RegExp.rightContext: "123456"'), 
	"d", "e", 
	"f", "g", "h",
        "i", "j", "k", "l", 
	"m", "n", "o", "p",
        "q", "r", "s", "t", 
	(.6  ), "v", "w", "x", "y", "z"
	], "map", function(e) { 
		return e != "u" 
			&& 
			(function b   (   )  { 
			} )      
			!= "x"; 
	});
} else {
  throw new TypeError();
}
