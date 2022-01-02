function O() {
    this.x0 = 1;
    this.x1 = 1;
    this.x2 = 1;
    this.x3 = 1;
    this.x4 = 1;
    this.x5 = 1;
}
function f() {
    var arr = [];
    for (var i=0; i<1500; i++)
	arr.push(new O);
    for (var i=0; i<1500; i++) {
	var o = arr[i];
	o.x10 = 1;
	o.x11 = 1;
	o.x12 = 1;
	o.x13 = 1;
	o.x14 = 1;
	o.x15 = 1;
	o.x16 = 1;
	o.x17 = 1;
	o.x18 = 1;
	o.x19 = 1;
	o.x20 = 1;
    }
}
f();
