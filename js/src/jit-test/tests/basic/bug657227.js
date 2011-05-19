var obj;
var counter = 0;
var p = Proxy.create({
    has : function(id) {
	if (id == 'xyz') {
	    ++counter;
	    if (counter == 7) {
		obj.__proto__ = null;
	    }
	    return true;
	}
	return false;
    },
    get : function(id) {
	if (id == 'xyz')
	    return 10;
    }
});

function test()
{
    Object.prototype.__proto__ = null;
    obj = { xyz: 1};
    var n = 0;
    for (var i = 0; i != 100; ++i) {
	var s = obj.xyz;
	if (s)
	    ++n;
	if (i == 10) {
	    delete obj.xyz;
	    Object.prototype.__proto__ = p;
	}

    }
}

try {
    test();
} catch (e) {}
