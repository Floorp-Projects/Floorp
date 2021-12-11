if (typeof(dojo) != 'undefined') { dojo.require('MochiKit.Base'); }
if (typeof(JSAN) != 'undefined') { JSAN.use('MochiKit.Base'); }
if (typeof(tests) == 'undefined') { tests = {}; }

tests.test_Base = function (t) {
    // test bind
    var not_self = {"toString": function () { return "not self"; } };
    var self = {"toString": function () { return "self"; } };
    var func = function (arg) { return this.toString() + " " + arg; };
    var boundFunc = bind(func, self);
    not_self.boundFunc = boundFunc;

    t.is( isEmpty([], [], ""), true, "isEmpty true" )
    t.is( isEmpty([], [1], ""), true, "isEmpty true" )
    t.is( isNotEmpty([], [], ""), false, "isNotEmpty false" )
    t.is( isNotEmpty([], [1], ""), false, "isNotEmpty false" )

    t.is( isEmpty([1], [1], "1"), false, "isEmpty false" )
    t.is( isEmpty([1], [1], "1"), false, "isEmpty false" )
    t.is( isNotEmpty([1], [1], "1"), true, "isNotEmpty true" )
    t.is( isNotEmpty([1], [1], "1"), true, "isNotEmpty true" )

    t.is( boundFunc("foo"), "self foo", "boundFunc bound to self properly" );
    t.is( not_self.boundFunc("foo"), "self foo", "boundFunc bound to self on another obj" );
    t.is( bind(boundFunc, not_self)("foo"), "not self foo", "boundFunc successfully rebound!" );
    t.is( bind(boundFunc, undefined, "foo")(), "self foo", "boundFunc partial no self change" );
    t.is( bind(boundFunc, not_self, "foo")(), "not self foo", "boundFunc partial self change" );

    // test method
    not_self = {"toString": function () { return "not self"; } };
    self = {"toString": function () { return "self"; } };
    func = function (arg) { return this.toString() + " " + arg; };
    var boundMethod = method(self, func);
    not_self.boundMethod = boundMethod;

    t.is( boundMethod("foo"), "self foo", "boundMethod bound to self properly" );
    t.is( not_self.boundMethod("foo"), "self foo", "boundMethod bound to self on another obj" );
    t.is( method(not_self, boundMethod)("foo"), "not self foo", "boundMethod successfully rebound!" );
    t.is( method(undefined, boundMethod, "foo")(), "self foo", "boundMethod partial no self change" );
    t.is( method(not_self, boundMethod, "foo")(), "not self foo", "boundMethod partial self change" );




    // test bindMethods

    var O = function (value) {
        bindMethods(this);
        this.value = value;
    };
    O.prototype.func = function () {
        return this.value;
    };

    var o = new O("boring");
    var p = {};
    p.func = o.func;
    var func = o.func;
    t.is( o.func(), "boring", "bindMethods doesn't break shit" );
    t.is( p.func(), "boring", "bindMethods works on other objects" );
    t.is( func(), "boring", "bindMethods works on functions" );

    var p = clone(o);
    t.ok( p instanceof O, "cloned correct inheritance" );
    var q = clone(p);
    t.ok( q instanceof O, "clone-cloned correct inheritance" );
    q.foo = "bar";
    t.is( p.foo, undefined, "clone-clone is copy-on-write" );
    p.bar = "foo";
    t.is( o.bar, undefined, "clone is copy-on-write" );
    t.is( q.bar, "foo", "clone-clone has proper delegation" );
    // unbind
    p.func = bind(p.func, null);
    t.is( p.func(), "boring", "clone function calls correct" );
    q.value = "awesome";
    t.is( q.func(), "awesome", "clone really does work" );
    
    // test boring boolean funcs

    t.is( isCallable(isCallable), true, "isCallable returns true on itself" );
    t.is( isCallable(1), false, "isCallable returns false on numbers" );

    t.is( isUndefined(null), false, "null is not undefined" );
    t.is( isUndefined(""), false, "empty string is not undefined" );
    t.is( isUndefined(undefined), true, "undefined is undefined" );
    t.is( isUndefined({}.foo), true, "missing property is undefined" );

    t.is( isUndefinedOrNull(null), true, "null is undefined or null" );
    t.is( isUndefinedOrNull(""), false, "empty string is not undefined or null" );
    t.is( isUndefinedOrNull(undefined), true, "undefined is undefined or null" );
    t.is( isUndefinedOrNull({}.foo), true, "missing property is undefined or null" );

    // test extension of arrays
    var a = [];
    var b = [];
    var three = [1, 2, 3];

    extend(a, three, 1);
    t.ok( objEqual(a, [2, 3]), "extend to an empty array" );
    extend(a, three, 1)
    t.ok( objEqual(a, [2, 3, 2, 3]), "extend to a non-empty array" );

    extend(b, three);
    t.ok( objEqual(b, three), "extend of an empty array" );

    t.is( compare(1, 2), -1, "numbers compare lt" );
    t.is( compare(2, 1), 1, "numbers compare gt" );
    t.is( compare(1, 1), 0, "numbers compare eq" );
    t.is( compare([1], [1]), 0, "arrays compare eq" );
    t.is( compare([1], [1, 2]), -1, "arrays compare lt (length)" );
    t.is( compare([1, 2], [2, 1]), -1, "arrays compare lt (contents)" );
    t.is( compare([1, 2], [1]), 1, "arrays compare gt (length)" );
    t.is( compare([2, 1], [1, 1]), 1, "arrays compare gt (contents)" );
    
    // test partial application
    var a = [];
    var func = function (a, b) {
        if (arguments.length != 2) {
            return "bad args";
        } else {
            return this.value + a + b;
        }
    };
    var self = {"value": 1, "func": func};
    var self2 = {"value": 2};
    t.is( self.func(2, 3), 6, "setup for test is correct" );
    self.funcTwo = partial(self.func, 2);
    t.is( self.funcTwo(3), 6, "partial application works" );
    t.is( self.funcTwo(3), 6, "partial application works still" );
    t.is( bind(self.funcTwo, self2)(3), 7, "rebinding partial works" );
    self.funcTwo = bind(bind(self.funcTwo, self2), null);
    t.is( self.funcTwo(3), 6, "re-unbinding partial application works" );

    
    // nodeWalk test
    // ... looks a lot like a DOM tree on purpose
    var tree = {
        "id": "nodeWalkTestTree",
        "test:int": "1",
        "childNodes": [
            {
                "test:int": "2",
                "childNodes": [
                    {"test:int": "5"},
                    "ignored string",
                    {"ignored": "object"},
                    ["ignored", "list"],
                    {
                        "test:skipchildren": "1",
                        "childNodes": [{"test:int": 6}]
                    }
                ]
            },
            {"test:int": "3"},
            {"test:int": "4"}
        ]
    }

    var visitedNodes = [];
    nodeWalk(tree, function (node) {
        var attr = node["test:int"];
        if (attr) {
            visitedNodes.push(attr);
        }
        if (node["test:skipchildren"]) {
                return;
        }
        return node.childNodes;
    });

    t.ok( objEqual(visitedNodes, ["1", "2", "3", "4", "5"]), "nodeWalk looks like it works");
    
    // test map
    var minusOne = function (x) { return x - 1; };
    var res = map(minusOne, [1, 2, 3]);
    t.ok( objEqual(res, [0, 1, 2]), "map works" );

    var res2 = xmap(minusOne, 1, 2, 3);
    t.ok( objEqual(res2, res), "xmap works" );

    res = map(operator.add, [1, 2, 3], [2, 4, 6]);
    t.ok( objEqual(res, [3, 6, 9]), "map(fn, p, q) works" );

    res = map(operator.add, [1, 2, 3], [2, 4, 6, 8]);
    t.ok( objEqual(res, [3, 6, 9]), "map(fn, p, q) works (q long)" );

    res = map(operator.add, [1, 2, 3, 4], [2, 4, 6]);
    t.ok( objEqual(res, [3, 6, 9]), "map(fn, p, q) works (p long)" );

    res = map(null, [1, 2, 3], [2, 4, 6]);
    t.ok( objEqual(res, [[1, 2], [2, 4], [3, 6]]), "map(null, p, q) works" );

    res = zip([1, 2, 3], [2, 4, 6]);
    t.ok( objEqual(res, [[1, 2], [2, 4], [3, 6]]), "zip(p, q) works" );

    res = map(null, [1, 2, 3]);
    t.ok( objEqual(res, [1, 2, 3]), "map(null, lst) works" );


    
    
    t.is( isNotEmpty("foo"), true, "3 char string is not empty" );
    t.is( isNotEmpty(""), false, "0 char string is empty" );
    t.is( isNotEmpty([1, 2, 3]), true, "3 element list is not empty" );
    t.is( isNotEmpty([]), false, "0 element list is empty" );

    // test filter
    var greaterThanThis = function (x) { return x > this; };
    var greaterThanOne = function (x) { return x > 1; };
    var res = filter(greaterThanOne, [-1, 0, 1, 2, 3]);
    t.ok( objEqual(res, [2, 3]), "filter works" );
    var res = filter(greaterThanThis, [-1, 0, 1, 2, 3], 1);
    t.ok( objEqual(res, [2, 3]), "filter self works" );
    var res2 = xfilter(greaterThanOne, -1, 0, 1, 2, 3);
    t.ok( objEqual(res2, res), "xfilter works" );
 
    t.is(objMax(1, 2, 9, 12, 42, -16, 16), 42, "objMax works (with numbers)");
    t.is(objMin(1, 2, 9, 12, 42, -16, 16), -16, "objMin works (with numbers)");
    
    // test adapter registry

    var R = new AdapterRegistry();
    R.register("callable", isCallable, function () { return "callable"; });
    R.register("arrayLike", isArrayLike, function () { return "arrayLike"; });
    t.is( R.match(function () {}), "callable", "registry found callable" );
    t.is( R.match([]), "arrayLike", "registry found ArrayLike" );
    try {
        R.match(null);
        t.ok( false, "non-matching didn't raise!" );
    } catch (e) {
        t.is( e, NotFound, "non-matching raised correctly" );
    }
    R.register("undefinedOrNull", isUndefinedOrNull, function () { return "undefinedOrNull" });
    R.register("undefined", isUndefined, function () { return "undefined" });
    t.is( R.match(undefined), "undefinedOrNull", "priorities are as documented" );
    t.ok( R.unregister("undefinedOrNull"), "removed adapter" );
    t.is( R.match(undefined), "undefined", "adapter was removed" );
    R.register("undefinedOrNull", isUndefinedOrNull, function () { return "undefinedOrNull" }, true);
    t.is( R.match(undefined), "undefinedOrNull", "override works" );
    
    var a1 = {"a": 1, "b": 2, "c": 2};
    var a2 = {"a": 2, "b": 1, "c": 2};
    t.is( keyComparator("a")(a1, a2), -1, "keyComparator 1 lt" );
    t.is( keyComparator("c")(a1, a2), 0, "keyComparator 1 eq" );
    t.is( keyComparator("c", "b")(a1, a2), 1, "keyComparator 2 eq gt" );
    t.is( keyComparator("c", "a")(a1, a2), -1, "keyComparator 2 eq lt" );
    t.is( reverseKeyComparator("a")(a1, a2), 1, "reverseKeyComparator" );
    t.is( compare(concat([1], [2], [3]), [1, 2, 3]), 0, "concat" );
    t.is( repr("foo"), '"foo"', "string repr" );
    t.is( repr(1), '1', "number repr" );
    t.is( listMin([1, 3, 5, 3, -1]), -1, "listMin" );
    t.is( objMin(1, 3, 5, 3, -1), -1, "objMin" );
    t.is( listMax([1, 3, 5, 3, -1]), 5, "listMax" );
    t.is( objMax(1, 3, 5, 3, -1), 5, "objMax" );

    var v = keys(a1);
    v.sort();
    t.is( compare(v, ["a", "b", "c"]), 0, "keys" );
    v = items(a1);
    v.sort();
    t.is( compare(v, [["a", 1], ["b", 2], ["c", 2]]), 0, "items" );

    var StringMap = function() {};
    a = new StringMap();
    a.foo = "bar";
    b = new StringMap();
    b.foo = "bar";
    try {
        compare(a, b);
        t.ok( false, "bad comparison registered!?" );
    } catch (e) {
        t.ok( e instanceof TypeError, "bad comparison raised TypeError" );
    }

    t.is( repr(a), "[object Object]", "default repr for StringMap" );
    var isStringMap = function () {
        for (var i = 0; i < arguments.length; i++) {
            if (!(arguments[i] instanceof StringMap)) {
                return false;
            }
        }
        return true;
    };

    registerRepr("stringMap",
        isStringMap,
        function (obj) {
            return "StringMap(" + repr(items(obj)) + ")";
        }
    );

    t.is( repr(a), 'StringMap([["foo", "bar"]])', "repr worked" );

    // not public API
    MochiKit.Base.reprRegistry.unregister("stringMap");
    
    t.is( repr(a), "[object Object]", "default repr for StringMap" );

    registerComparator("stringMap",
        isStringMap,
        function (a, b) {
            // no sorted(...) in base
            a = items(a);
            b = items(b);
            a.sort(compare);
            b.sort(compare);
            return compare(a, b);
        }
    );

    t.is( compare(a, b), 0, "registerComparator" );

    update(a, {"foo": "bar"}, {"wibble": "baz"}, undefined, null, {"grr": 1});
    t.is( a.foo, "bar", "update worked (first obj)" );
    t.is( a.wibble, "baz", "update worked (second obj)" );
    t.is( a.grr, 1, "update worked (skipped undefined and null)" );
    t.is( compare(a, b), 1, "update worked (comparison)" );


    setdefault(a, {"foo": "unf"}, {"bar": "web taco"} );
    t.is( a.foo, "bar", "setdefault worked (skipped existing)" );
    t.is( a.bar, "web taco", "setdefault worked (set non-existing)" );

    var c = items(merge({"foo": "bar"}, {"wibble": "baz"}));
    c.sort(compare);
    t.is( compare(c, [["foo", "bar"], ["wibble", "baz"]]), 0, "merge worked" );
    
    // not public API
    MochiKit.Base.comparatorRegistry.unregister("stringMap");
    
    try {
        compare(a, b);
        t.ok( false, "bad comparison registered!?" );
    } catch (e) {
        t.ok( e instanceof TypeError, "bad comparison raised TypeError" );
    }
    
    var o = {"__repr__": function () { return "__repr__"; }};
    t.is( repr(o), "__repr__", "__repr__ protocol" );
    t.is( repr(MochiKit.Base), MochiKit.Base.__repr__(), "__repr__ protocol when repr is defined" );
    var o = {"NAME": "NAME"};
    t.is( repr(o), "NAME", "NAME protocol (obj)" );
    o = function () { return "TACO" };
    o.NAME = "NAME";
    t.is( repr(o), "NAME", "NAME protocol (func)" );
    
    t.is( repr(MochiKit.Base.nameFunctions), "MochiKit.Base.nameFunctions", "test nameFunctions" );
    // Done!

    t.is( urlEncode("1+2=2").toUpperCase(), "1%2B2%3D2", "urlEncode" );
    t.is( queryString(["a", "b"], [1, "two"]), "a=1&b=two", "queryString");
    t.is( queryString({"a": 1}), "a=1", "one item alternate form queryString" );
    var o = {"a": 1, "b": 2, "c": function() {}};
    var res = queryString(o).split("&");
    res.sort();
    t.is( res.join("&"), "a=1&b=2", "two item alternate form queryString, function skip" );
    var res = parseQueryString("1+1=2&b=3%3D2");
    t.is( res["1 1"], "2", "parseQueryString pathological name" );
    t.is( res.b, "3=2", "parseQueryString second name:value pair" );
    var res = parseQueryString("foo=one&foo=two", true);
    t.is( res["foo"].join(" "), "one two", "parseQueryString useArrays" );
    var res = parseQueryString("?foo=2&bar=1");
    t.is( res["foo"], "2", "parseQueryString strip leading question mark");

    t.is( serializeJSON("foo\n\r\b\f\t"), "\"foo\\n\\r\\b\\f\\t\"", "string JSON" );
    t.is( serializeJSON(null), "null", "null JSON");
    try {
        serializeJSON(undefined);
        t.ok(false, "undefined should not be serializable");
    } catch (e) {
        t.ok(e instanceof TypeError, "undefined not serializable");
    }
    t.is( serializeJSON(1), "1", "1 JSON");
    t.is( serializeJSON(1.23), "1.23", "1.23 JSON");
    t.is( serializeJSON(serializeJSON), null, "function JSON (null, not string)" );
    t.is( serializeJSON([1, "2", 3.3]), "[1, \"2\", 3.3]", "array JSON" );
    var res = evalJSON(serializeJSON({"a":1, "b":2}));
    t.is( res.a, 1, "evalJSON on an object (1)" );
    t.is( res.b, 2, "evalJSON on an object (2)" );
    var res = {"a": 1, "b": 2, "json": function () { return this; }};
    var res = evalJSON(serializeJSON(res));
    t.is( res.a, 1, "evalJSON on an object that jsons self (1)" );
    t.is( res.b, 2, "evalJSON on an object that jsons self (2)" );
    var strJSON = {"a": 1, "b": 2, "json": function () { return "json"; }};
    t.is( serializeJSON(strJSON), "\"json\"", "json serialization calling" );
    t.is( serializeJSON([strJSON]), "[\"json\"]", "json serialization calling in a structure" );
    t.is( evalJSON('/* {"result": 1} */').result, 1, "json comment stripping" );
    t.is( evalJSON('/* {"*/ /*": 1} */')['*/ /*'], 1, "json comment stripping" );
    registerJSON("isDateLike",
        isDateLike,
        function (d) {
            return "this was a date";
        }
    );
    t.is( serializeJSON(new Date()), "\"this was a date\"", "json registry" );
    MochiKit.Base.jsonRegistry.unregister("isDateLike");

    var a = {"foo": {"bar": 12, "wibble": 13}};
    var b = {"foo": {"baz": 4, "bar": 16}, "bar": 4};
    updatetree(a, b);
    var expect = [["bar", 16], ["baz", 4], ["wibble", 13]];
    var got = items(a.foo);
    got.sort(compare);
    t.is( repr(got), repr(expect), "updatetree merge" );
    t.is( a.bar, 4, "updatetree insert" );
    
    var c = counter();
    t.is( c(), 1, "counter starts at 1" );
    t.is( c(), 2, "counter increases" );
    c = counter(2);
    t.is( c(), 2, "counter starts at 2" );
    t.is( c(), 3, "counter increases" );

    t.is( findValue([1, 2, 3], 4), -1, "findValue returns -1 on not found");
    t.is( findValue([1, 2, 3], 1), 0, "findValue returns correct index");
    t.is( findValue([1, 2, 3], 1, 1), -1, "findValue honors start");
    t.is( findValue([1, 2, 3], 2, 0, 1), -1, "findValue honors end");
    t.is( findIdentical([1, 2, 3], 4), -1, "findIdentical returns -1");
    t.is( findIdentical([1, 2, 3], 1), 0, "findIdentical returns correct index");
    t.is( findIdentical([1, 2, 3], 1, 1), -1, "findIdentical honors start");
    t.is( findIdentical([1, 2, 3], 2, 0, 1), -1, "findIdentical honors end");
    t.is( isNull(undefined), false, "isNull doesn't match undefined" );

    var flat = flattenArguments(1, "2", 3, [4, [5, [6, 7], 8, [], 9]]);
    var expect = [1, "2", 3, 4, 5, 6, 7, 8, 9];
    t.is( repr(flat), repr(expect), "flattenArguments" );

    var fn = function () {
        return [this, concat(arguments)];
    }
    t.is( methodcaller("toLowerCase")("FOO"), "foo", "methodcaller with a method name" );
    t.is( repr(methodcaller(fn, 2, 3)(1)), "[1, [2, 3]]", "methodcaller with a function" );

    var f1 = function (x) { return [1, x]; };
    var f2 = function (x) { return [2, x]; };
    var f3 = function (x) { return [3, x]; };
    t.is( repr(f1(f2(f3(4)))), "[1, [2, [3, 4]]]", "test the compose test" );
    t.is( repr(compose(f1,f2,f3)(4)), "[1, [2, [3, 4]]]", "three fn composition works" );
    t.is( repr(compose(compose(f1,f2),f3)(4)), "[1, [2, [3, 4]]]", "associative left" );
    t.is( repr(compose(f1,compose(f2,f3))(4)), "[1, [2, [3, 4]]]", "associative right" );
    
    try {
        compose(f1, "foo");
        t.ok( false, "wrong compose argument not raised!" );
    } catch (e) {
        t.is( e.name, 'TypeError', "wrong compose argument raised correctly" );
    }
    
    t.is(camelize('one'), 'one', 'one word');
    t.is(camelize('one-two'), 'oneTwo', 'two words');
    t.is(camelize('one-two-three'), 'oneTwoThree', 'three words');
    t.is(camelize('1-one'), '1One', 'letter and word');
    t.is(camelize('one-'), 'one', 'trailing hyphen');
    t.is(camelize('-one'), 'One', 'starting hyphen');
    t.is(camelize('o-two'), 'oTwo', 'one character and word');

    var flat = flattenArray([1, "2", 3, [4, [5, [6, 7], 8, [], 9]]]);
    var expect = [1, "2", 3, 4, 5, 6, 7, 8, 9];
    t.is( repr(flat), repr(expect), "flattenArray" );
    
    /* mean */
    try {
        mean();
        t.ok( false, "no arguments didn't raise!" );
    } catch (e) {
        t.is( e.name, 'TypeError', "no arguments raised correctly" );
    }    
    t.is( mean(1), 1, 'single argument (arg list)');
    t.is( mean([1]), 1, 'single argument (array)');
    t.is( mean(1,2,3), 2, 'three arguments (arg list)');
    t.is( mean([1,2,3]), 2, 'three arguments (array)');
    t.is( average(1), 1, 'test the average alias');

    /* median */
    try {
        median();
        t.ok( false, "no arguments didn't raise!" );
    } catch (e) {
        t.is( e.name, 'TypeError', "no arguments raised correctly" );
    }
    t.is( median(1), 1, 'single argument (arg list)');
    t.is( median([1]), 1, 'single argument (array)');
    t.is( median(3,1,2), 2, 'three arguments (arg list)');
    t.is( median([3,1,2]), 2, 'three arguments (array)');
    t.is( median(3,1,2,4), 2.5, 'four arguments (arg list)');
    t.is( median([3,1,2,4]), 2.5, 'four arguments (array)');

    /* #185 */
    t.is( serializeJSON(parseQueryString("")), "{}", "parseQueryString('')" );
    t.is( serializeJSON(parseQueryString("", true)), "{}", "parseQueryString('', true)" );

    /* #109 */
    t.is( queryString({ids: [1,2,3]}), "ids=1&ids=2&ids=3", "queryString array value" );
    t.is( queryString({ids: "123"}), "ids=123", "queryString string value" );

    /* test values */
    var o = {a: 1, b: 2, c: 4, d: -1};
    var got = values(o);
    got.sort();
    t.is( repr(got), repr([-1, 1, 2, 4]), "values()" );

    t.is( queryString([["foo", "bar"], ["baz", "wibble"]]), "foo=baz&bar=wibble" );
    o = parseQueryString("foo=1=1=1&bar=2&baz&wibble=");
    t.is( o.foo, "1=1=1", "parseQueryString multiple = first" );
    t.is( o.bar, "2", "parseQueryString multiple = second" );
    t.is( o.baz, "", "parseQueryString multiple = third" );
    t.is( o.wibble, "", "parseQueryString multiple = fourth" );

};
