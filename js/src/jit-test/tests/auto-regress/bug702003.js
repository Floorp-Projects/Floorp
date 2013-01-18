// Binary: cache/js-dbg-64-50c1bcb49c76-linux
// Flags: -m -n -a
//

var lfcode = new Array();
lfcode.push("try { \
  gczeal(2);\
  exitFunc ('test');\
  } catch(exc1) {}\
");
lfcode.push("var summary = 'Foo'; \
  var actual = 'No Crash';\
  var expect = 'No Crash';\
  test();\
  function test() {\
    try {\
      eval('(function(){ <x/>.(yield 4) })().next();');\
    }\ catch(ex) { 'Bar'; }\
  }\
");
while (true) {
        var code = lfcode.shift();
        if (code == undefined) { break; }
        evaluate(code);
}
