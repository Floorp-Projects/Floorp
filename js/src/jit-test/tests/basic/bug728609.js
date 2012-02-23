var lfcode = new Array();
lfcode.push("\
test();\
function test() {\
  function removeAllProperties(o) {\
    bar() = thaw, patterns;\
  }\
  var o = {};\
  o.first = { toSource: function() { removeAllProperties(o); } };\
  return o.toSource();\
}\
");
lfcode.push("test();");
gczeal(4);
while (true) {
        var file = lfcode.shift(); if (file == undefined) { break; }
                loadFile(file);
}
function loadFile(lfVarx) {
        try {
                        evaluate(lfVarx);
        } catch (lfVare) {      }
}
