// Binary: cache/js-dbg-64-ca97bbcd6b90-linux
// Flags: --ion-eager
//

gczeal(4);
function TestCase(n, d, e, a) {}
TestCase.prototype.dump = function () {};
TestCase.prototype.testFailed = (function TestCase_testFailed() {
	});
  try  {
    try    {    }    catch(ex1)    {    }
  }  catch(ex)  {  }
  options.initvalues  = {};
  var optionNames = options().split(',');
  var optionsframe = {};
  try  {
    optionsClear();
  }  catch(ex)  {  }
var lfcode = new Array();
lfcode.push("\
  try {  } catch (exception) {  }\
    try {    } catch (exception) {    }\
    try {    } catch (exception) {    }\
    try {    } catch (actual) {    }\
        var props = {};\
  function test(which) {\
    var g = newGlobal();\
    function addDebugger(g, i) {\
        var dbg = Debugger(g);\
        dbg.onDebuggerStatement = function (frame) { };\
    }\
    for (var i = 0; i < 3; i++) {\
        addDebugger(g, i);\
    }\
    g.eval(\"debugger;\");\
}\
for (var j = 0; j < 3; j++) test(j);\
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
	try { evaluate(file); } catch (lfVare) { }
}
