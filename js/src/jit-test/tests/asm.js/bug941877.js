load(libdir + "asm.js");

function FunctionBody(f) {
    var str = f.toString();
    var firstBrace = str.indexOf('{');
    var lastBrace = str.lastIndexOf('}');
    return str.substr(firstBrace + 1, lastBrace - firstBrace - 1);
}

var F = (function (stdlib, n, heap) {
    var Int16ArrayView = new stdlib.Int16Array(heap);
    function f(i0, d1) {
        i0 = i0 | 0
        d1 = +d1
        var d2 = 1.
        var i3 = 0
        Int16ArrayView[0] = i0
        d2 = (.0 / .0)
        switch ((i0 + i0) | 0) {
            case 0:
                d2 = .0
                break
        };
        (((i3 - (0 < 0)) >>> ((.0 < -0) + (.0 > .0))) / 0) >> (0 + (((0 + 0) ^
(9 > 0)) | 0))
    }
    return f;
});

var compiled = asmCompile('stdlib', 'n', 'heap', USE_ASM + FunctionBody(F));
asmLink(compiled, this, null, new ArrayBuffer(BUF_MIN))();

var F = (function(stdlib, n, heap) {
    var Float64ArrayView = new stdlib.Float64Array(heap)
    function f() {
        Float64ArrayView[0] = +(0xffffffff / 0xffffffff >> 0)
    }
    return f;
});

var compiled = asmCompile('stdlib', 'n', 'heap', USE_ASM + FunctionBody(F));
asmLink(compiled, this, null, new ArrayBuffer(BUF_MIN))();

function test0(x)
{
   return (x >>> 0) % 10;
}
assertEq(test0(25), 5);
assertEq(test0(24), 4);
assertEq(test0(23), 3);
assertEq(test0(0), 0);
assertEq(test0(4294967295), 5);

function test1(x)
{
   return (x >>> 2) % 10;
}
assertEq(test1(25), 6);
assertEq(test1(24), 6);
assertEq(test1(23), 5);
assertEq(test1(4294967295), 3);

function test2(x, y)
{
   return (x >>> 0) % (y >>> 0);
}
assertEq(test2(25, 3), 1);
assertEq(test2(24, 4), 0);
assertEq(test2(4294967295, 10), 5);
assertEq(test2(23, 0), NaN);

function test3()
{
    function f(x, y)
    {
        x = x|0;
        y = y|0;
        var t = 0;
        t = (((x|0) % (y|0))|0) > -2;
        return t|0;
    }
    return f;
}
var compiled = asmCompile(USE_ASM + FunctionBody(test3));
var linked = asmLink(compiled);
assertEq(linked(4294967290, 4294967295), 1);
assertEq(linked(4294967290, 4294967295), 1);

function test4()
{
    function f(x, y)
    {
        x = x|0;
        y = y|0;
        var t = 0;
        t = (((x>>>0) % (y>>>0)) >>> 0) > 0;
        return t|0;
    }
    return f;
}
var compiled = asmCompile(USE_ASM + FunctionBody(test4));
var linked = asmLink(compiled);
assertEq(linked(4294967290, 4294967295), 1);
assertEq(linked(4294967290, 4294967295), 1);

function test5()
{
    function f(x, y)
    {
        x = x|0;
        y = y|0;
        var t = 0.;
        t = +(((x>>>0) % (y>>>0)) >>> 0) * 1.01;
        return +t;
    }
    return f;
}
var compiled = asmCompile(USE_ASM + FunctionBody(test5));
var linked = asmLink(compiled);
assertEq(linked(4294967290, 4294967295), 4337916962.9);
assertEq(linked(4294967290, 4294967295), 4337916962.9);

function test6()
{
    function f(x, y)
    {
        x = x|0;
        y = y|0;
        return ((x>>>1) % (y>>>1))|0;
    }
    return f;
}
var compiled = asmCompile(USE_ASM + FunctionBody(test6));
var linked = asmLink(compiled);
assertEq(linked(23847, 7), 1);
assertEq(linked(23848, 7), 2);

function test7()
{
    function f(x)
    {
        x = x|0;
        return ((x>>>0) / 8)|0;
    }
    return f;
}
var compiled = asmCompile(USE_ASM + FunctionBody(test7));
var linked = asmLink(compiled);
assertEq(linked(23847, 7), 2980);
assertEq(linked(23848, 7), 2981);

function test8()
{
    function f(i,j)
    {
        i=i|0;j=j|0;
        return ((i>>>0)/(j>>>0))|0;
    }
    return f;
}
var compiled = asmCompile(USE_ASM + FunctionBody(test8));
var linked = asmLink(compiled);
assertEq(linked(2147483647, 0), 0);
assertEq(linked(2147483646, 0), 0);
