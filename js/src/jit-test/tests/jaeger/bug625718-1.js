function f3() { return 2; };
function f4(o) { o.g4 = function() {}; };

var f = function() {};
f.x = undefined;
f4(new String("x"));
f3();
f4(f);

for(var i=0; i<20; i++) {
    f4(Math);
}
