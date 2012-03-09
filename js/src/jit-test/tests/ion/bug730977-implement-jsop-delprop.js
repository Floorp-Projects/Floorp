
function makeThing(i)
{
    var thing = {};
    thing.foo = i;
    thing.bar = "bar_" + i;
    Object.defineProperty(thing, 'baz', {'configurable':false, 'value':[i]});
    return thing;
}
function makeArray(count)
{
    var arr = new Array(count);
    for(var i = 0; i < count; i++) {
        arr[i] = makeThing(i);
    }
    return arr;
}
function delBar(obj)
{
    assertEq(Object.getOwnPropertyDescriptor(obj, 'bar') === undefined, false);
    assertEq(delete obj.bar, true);
    assertEq(Object.getOwnPropertyDescriptor(obj, 'bar') === undefined, true);
}
function delBaz(obj)
{
    var s = delete obj.baz;
    assertEq(Object.getOwnPropertyDescriptor(obj, 'baz') === undefined, false);
    assertEq(delete obj.baz, false);
    assertEq(Object.getOwnPropertyDescriptor(obj, 'baz') === undefined, false);
}
function delNonexistentThingy(obj)
{
    assertEq(Object.getOwnPropertyDescriptor(obj, 'thingy') === undefined, true);
    assertEq(delete obj.thingy, true);
    assertEq(Object.getOwnPropertyDescriptor(obj, 'thingy') === undefined, true);
}
function testDelProp()
{
    var arr = makeArray(10000);
    for(var i = 0; i < 10000; i++) {
        var obj = arr[i];
        assertEq(Object.getOwnPropertyDescriptor(obj, 'foo') === undefined, false);
        assertEq(delete obj.foo, true);
        assertEq(Object.getOwnPropertyDescriptor(obj, 'foo') === undefined, true);
        delBar(obj);
        delBaz(obj);
        delNonexistentThingy(obj);
    }
}

testDelProp();
