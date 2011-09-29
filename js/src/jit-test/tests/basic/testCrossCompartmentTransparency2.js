var g = newGlobal('new-compartment');

var array = g.eval("new Array(1,2,3)");
assertEq([array,array].concat().toString(), "1,2,3,1,2,3");
assertEq(Array.isArray(array), true);

var number = g.eval("new Number(42)");
var bool = g.eval("new Boolean(false)");
var string = g.eval("new String('ponies')");
assertEq(JSON.stringify({n:number, b:bool, s:string}), "{\"n\":42,\"b\":false,\"s\":\"ponies\"}");
assertEq(JSON.stringify({arr:array}), "{\"arr\":[1,2,3]}");
assertEq(JSON.stringify({2:'ponies', unicorns:'not real'}, array), "{\"2\":\"ponies\"}");
assertEq(JSON.stringify({42:true, ponies:true, unicorns:'sad'}, [number, string]), "{\"42\":true,\"ponies\":true}");
assertEq(JSON.stringify({a:true,b:false}, undefined, number), "{\n          \"a\": true,\n          \"b\": false\n}");
assertEq(JSON.stringify({a:true,b:false}, undefined, string), "{\nponies\"a\": true,\nponies\"b\": false\n}");

var o = Proxy.create({getPropertyDescriptor:function(name) {}}, Object.prototype);
var threw = false;
try {
    print([].concat(o).toString());
} catch(e) {
    assertEq(e instanceof TypeError, true);
    threw = true;
}
assertEq(threw, true);
