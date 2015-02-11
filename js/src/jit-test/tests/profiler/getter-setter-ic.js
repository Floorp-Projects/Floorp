// Ensure readSPSProfilingStack() doesn't crash with Ion
// getter/setter ICs on the stack.
function getObjects() {
    var objs = [];
    objs.push({x: 0, get prop() {
	readSPSProfilingStack();
	return ++this.x;
    }, set prop(v) {
	readSPSProfilingStack();
	this.x = v + 2;
    }});
    objs.push({x: 0, y: 0, get prop() {
	readSPSProfilingStack();
	return this.y;
    }, set prop(v) {
	readSPSProfilingStack();
	this.y = v;
    }});
    return objs;
}
function f() {
    var objs = getObjects();
    var res = 0;
    for (var i=0; i<100; i++) {
	var o = objs[i % objs.length];
	res += o.prop;
	o.prop = i;
    }
    assertEq(res, 4901);
}
enableSPSProfiling();
f();
