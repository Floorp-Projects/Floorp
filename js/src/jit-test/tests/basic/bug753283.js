var summary = '';
function printStatus (msg) {
  var lines = msg.split ("\n");
}
evaluate("\
function f() {\
    var ss = [\
        new f(Int8Array, propertyIsEnumerable, '[let (x = 3, y = 4) x].map(0)')\
    ];\
}\
try {\
    f();\
} catch (e) {}\
  gczeal(4);\
  printStatus (summary);\
");
evaluate("\
function g(n, h) {\
    var a = f;\
    if (n <= 0) \
    return f; \
    var t = g(n - 1, h);\
    var r = function(x) {    };\
}\
g(80, f);\
");
