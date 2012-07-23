function ctor()
{
    this.a = 1;
    this.b = 2;
}
function f2(o)
{
    o.c = 12;
}
function f()
{
    var x = new ctor();
    verifyprebarriers();
    f2(x);
    verifyprebarriers();
}
f();
f();
var o = {};
for (var i=0; i<1000; i++)
    o['a'+i] = 3;
f();
f();
