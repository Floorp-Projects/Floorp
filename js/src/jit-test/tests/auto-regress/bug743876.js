// Binary: cache/js-dbg-64-434f50e70815-linux
// Flags: -m -n -a
//

var lfcode = new Array();
lfcode.push("3");
lfcode.push("\
evaluate(\"\");\
function slice(a, b) {\
    return slice(index, ++(ArrayBuffer));\
}\
");
lfcode.push("0");
lfcode.push("var arr = [0, 1, 2, 3, 4];\
function replacer() {\
  assertEq(arguments.length, 2);\
  var key = arguments[0], value = arguments[1];\
  return value;\
}\
assertEq(JSON.stringify(arr, replacer), '[0,1,2,3,4]');\
");
while (true) {
        var file = lfcode.shift(); if (file == undefined) { break; }
                loadFile(file);
}
function loadFile(lfVarx) {
                if (!isNaN(lfVarx)) {
                        lfRunTypeId = parseInt(lfVarx);
                } else {
                        switch (lfRunTypeId) {
                                case 0: evaluate(lfVarx); break;
                                case 3: function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); break;
                }
        }
}
