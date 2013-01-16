s = newGlobal('');
function f(code) {
    try {
        evalcx(code, s)
    } catch (e) {}
}
f("\
    options('strict');\
    var x;\
    y='';\
    Object.preventExtensions(this);\
    y=new String;\
    y.toString=(function(){x=new Iterator});\
");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("var z;");
f("\
    Iterator=String.prototype.toUpperCase;\
    v=(function(){});\
    Object.defineProperty(Function,0,({enumerable:x}));\
")
