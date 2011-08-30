var i = 1;
var j = 2;
function f() {
    if (false) 
        function g() {};
    return i / j;
}
-null;
-null;
-null;
-null;
-null;
f();

