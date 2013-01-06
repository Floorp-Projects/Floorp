// Test 1: When constructing x, we shouldn't take the prototype for this.
// it will crash if that happens
evalcx("\
    var x = newGlobal().Object;\
    function f() { return new x; }\
    f();\
    f();\
", newGlobal());

// Test 2: Don't take the prototype of proxy's to create |this|,
// as this will throw... Not expected behaviour.
var O = new Proxy(function() {}, {
    get: function() {
	    throw "get trap";
    }
});

function f() {
  new O();
}

f();
f();
