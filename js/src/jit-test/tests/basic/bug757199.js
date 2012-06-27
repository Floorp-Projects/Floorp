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
    verifybarriers();
    f2(x);
    verifybarriers();
}
f();
f();
var o = {};
for (var i=0; i<1000; i++)
    o['a'+i] = 3;
f();
f();
