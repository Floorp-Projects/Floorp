// |jit-test| slow

/* Make a lot of functions of the form:
function x1(){x1();}
function x2(){x2();}
function x3(){x3();}
...
*/

var g = newGlobal();
var dbg = new g.Debugger(this);

var s = '';
for (var i = 0; i < 70000; i++) {
    s += 'function x' + i + '() { x' + i + '(); }\n';
}
s += 'pc2line(1);\n'
evaluate(s);
