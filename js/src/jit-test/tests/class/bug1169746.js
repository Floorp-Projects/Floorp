class C { }
class D extends C { }

function f()
{
    for (var i = 0; i < 2000; ++i)
        new D();
}

f();
