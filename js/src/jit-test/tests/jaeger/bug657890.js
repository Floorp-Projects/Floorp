function f() {};
var x;
for(var i=0; i<200; i++) {
    x = f.bind(x, x, 2);
    gc();
}
