// |reftest| skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// Set of properties on a cloned object that are legitimately non-enumerable,
// grouped by object type.
var non_enumerable = { 'Array': [ 'length' ],
                       'String': [ 'length' ] };

// Set of properties on a cloned object that are legitimately non-configurable,
// grouped by object type. The property name '0' stands in for any indexed
// property.
var non_configurable = { 'String': [ 0 ] };

// Set of properties on a cloned object that are legitimately non-writable,
// grouped by object type. The property name '0' stands in for any indexed
// property.
var non_writable = { 'String': [ 0 ] };

function classOf(obj) {
    var classString = Object.prototype.toString.call(obj);
    var [ all, classname ] = classString.match(/\[object (\w+)/);
    return classname;
}

function isIndex(p) {
    var u = p >>> 0;
    return ("" + u == p && u != 0xffffffff);
}

function notIndex(p) {
    return !isIndex(p);
}

function tableContains(table, cls, prop) {
    if (isIndex(prop))
        prop = 0;
    if (cls.match(/\wArray$/))
        cls = "(typed array)";
    var exceptionalProps = table[cls] || [];
    return exceptionalProps.indexOf(prop) != -1;
}

function shouldBeConfigurable(cls, prop) {
    return !tableContains(non_configurable, cls, prop);
}

function shouldBeWritable(cls, prop) {
    return !tableContains(non_writable, cls, prop);
}

function ownProperties(obj) {
    return Object.getOwnPropertyNames(obj).
        map(function (p) { return [p, Object.getOwnPropertyDescriptor(obj, p)]; });
}

function isCloneable(pair) {
    return typeof pair[0] === 'string' && pair[1].enumerable;
}

function compareProperties(a, b, stack, path) {
    var ca = classOf(a);

    // 'b', the original object, may have non-enumerable or XMLName properties;
    // ignore them. 'a', the clone, should not have any non-enumerable
    // properties (except .length, if it's an Array or String) or XMLName
    // properties.
    var pb = ownProperties(b).filter(isCloneable);
    var pa = ownProperties(a);
    for (var i = 0; i < pa.length; i++) {
        var propname = pa[i][0];
        assertEq(typeof propname, "string", "clone should not have E4X properties " + path);
        if (!pa[i][1].enumerable) {
            if (tableContains(non_enumerable, ca, propname)) {
                // remove it so that the comparisons below will work
                pa.splice(i, 1);
                i--;
            } else {
                throw new Error("non-enumerable clone property " + propname + " " + path);
            }
        }
    }

    // Check that, apart from properties whose names are array indexes,
    // the enumerable properties appear in the same order.
    var aNames = pa.map(function (pair) { return pair[1]; }).filter(notIndex);
    var bNames = pa.map(function (pair) { return pair[1]; }).filter(notIndex);
    assertEq(aNames.join(","), bNames.join(","), path);

    // Check that the lists are the same when including array indexes.
    function byName(a, b) { a = a[0]; b = b[0]; return a < b ? -1 : a === b ? 0 : 1; }
    pa.sort(byName);
    pb.sort(byName);
    assertEq(pa.length, pb.length, "should see the same number of properties " + path);
    for (var i = 0; i < pa.length; i++) {
        var aName = pa[i][0];
        var bName = pb[i][0];
        assertEq(aName, bName, path);

        var path2 = isIndex(aName) ? path + "[" + aName + "]" : path + "." + aName;
        var da = pa[i][1];
        var db = pb[i][1];
        assertEq(da.configurable, shouldBeConfigurable(ca, aName), path2);
        assertEq(da.writable, shouldBeWritable(ca, aName), path2);
        assertEq("value" in da, true, path2);
        var va = da.value;
        var vb = b[pb[i][0]];
        stack.push([va, vb, path2]);
    }
}

function isClone(a, b) {
    var stack = [[a, b, 'obj']];
    var memory = new WeakMap();
    var rmemory = new WeakMap();

    while (stack.length > 0) {
        var pair = stack.pop();
        var x = pair[0], y = pair[1], path = pair[2];
        if (typeof x !== "object" || x === null) {
            // x is primitive.
            assertEq(x, y, "equal primitives");
        } else if (x instanceof Date) {
            assertEq(x.getTime(), y.getTime(), "equal times for cloned Dates");
        } else if (memory.has(x)) {
            // x is an object we have seen before in a.
            assertEq(y, memory.get(x), "repeated object the same");
            assertEq(rmemory.get(y), x, "repeated object's clone already seen");
        } else {
            // x is an object we have not seen before.
	    // Check that we have not seen y before either.
            assertEq(rmemory.has(y), false);

            var xcls = classOf(x);
            var ycls = classOf(y);
            assertEq(xcls, ycls, "same [[Class]]");

            // clone objects should have the default prototype of the class
            assertEq(Object.getPrototypeOf(x), this[xcls].prototype);

            compareProperties(x, y, stack, path);

            // Record that we have seen this pair of objects.
            memory.set(x, y);
            rmemory.set(y, x);
        }
    }
    return true;
}

function check(val) {
    var clone = deserialize(serialize(val));
    assertEq(isClone(val, clone), true);
    return clone;
}

// Various recursive objects

// Recursive array.
var a = [];
a[0] = a;
check(a);

// Recursive Object.
var b = {};
b.next = b;
check(b);

// Mutually recursive objects.
var a = [];
var b = {};
var c = {};
a[0] = b;
a[1] = b;
a[2] = b;
b.next = a;
check(a);
check(b);

// A date
check(new Date);

// A recursive object that is very large.
a = [];
b = a;
for (var i = 0; i < 10000; i++) {
    b[0] = {};
    b[1] = [];
    b = b[1];
}
b[0] = {owner: a};
b[1] = [];
check(a);

// Date objects should not be identical even if representing the same date
var ar = [ new Date(1000), new Date(1000) ];
var clone = check(ar);
assertEq(clone[0] === clone[1], false);

// Identity preservation for various types of objects

function checkSimpleIdentity(v)
{
    a = check([ v, v ]);
    assertEq(a[0] === a[1], true);
    return a;
}

var v = new Boolean(true);
checkSimpleIdentity(v);

v = new Number(17);
checkSimpleIdentity(v);

v = new String("yo");
checkSimpleIdentity(v);

v = "fish";
checkSimpleIdentity(v);

v = new Int8Array([ 10, 20 ]);
checkSimpleIdentity(v);

v = new ArrayBuffer(7);
checkSimpleIdentity(v);

v = new Date(1000);
b = [ v, v, { 'date': v } ];
clone = check(b);
assertEq(clone[0] === clone[1], true);
assertEq(clone[0], clone[2]['date']);
assertEq(clone[0] === v, false);

// Reduced and modified from postMessage_structured_clone test
let foo = { };
let baz = { };
let obj = { 'foo': foo,
            'bar': { 'foo': foo },
            'expando': { 'expando': baz },
            'baz': baz };
check(obj);

for (obj of getTestContent())
    check(obj);

// Stolen wholesale from postMessage_structured_clone_helper.js
function* getTestContent()
{
  yield "hello";
  yield 2+3;
  yield 12;
  yield null;
  yield "complex" + "string";
  yield new Object();
  yield new Date(1306113544);
  yield [1, 2, 3, 4, 5];
  let obj = new Object();
  obj.foo = 3;
  obj.bar = "hi";
  obj.baz = new Date(1306113544);
  obj.boo = obj;
  yield obj;

  let recursiveobj = new Object();
  recursiveobj.a = recursiveobj;
  recursiveobj.foo = new Object();
  recursiveobj.foo.bar = "bar";
  recursiveobj.foo.backref = recursiveobj;
  recursiveobj.foo.baz = 84;
  recursiveobj.foo.backref2 = recursiveobj;
  recursiveobj.bar = new Object();
  recursiveobj.bar.foo = "foo";
  recursiveobj.bar.backref = recursiveobj;
  recursiveobj.bar.baz = new Date(1306113544);
  recursiveobj.bar.backref2 = recursiveobj;
  recursiveobj.expando = recursiveobj;
  yield recursiveobj;

  obj = new Object();
  obj.expando1 = 1;
  obj.foo = new Object();
  obj.foo.bar = 2;
  obj.bar = new Object();
  obj.bar.foo = obj.foo;
  obj.expando = new Object();
  obj.expando.expando = new Object();
  obj.expando.expando.obj = obj;
  obj.expando2 = 4;
  obj.baz = obj.expando.expando;
  obj.blah = obj.bar;
  obj.foo.baz = obj.blah;
  obj.foo.blah = obj.blah;
  yield obj;

  let diamond = new Object();
  obj = new Object();
  obj.foo = "foo";
  obj.bar = 92;
  obj.backref = diamond;
  diamond.ref1 = obj;
  diamond.ref2 = obj;
  yield diamond;

  let doubleref = new Object();
  obj = new Object();
  doubleref.ref1 = obj;
  doubleref.ref2 = obj;
  yield doubleref;
}

reportCompare(0, 0, 'ok');
