function f() {
    for (var i=0; i<20; i++) {
	var o = {x: 1,
		 get g1() { return this.x; },
		 set g2(v) { this.x = v; },
		 get 44() { return this.x },
		 set 44(v) { this.x = v; }
		};

	assertEq(o.x, 1);
	assertEq(o.g1, 1);
	assertEq(o[44], 1);

	o.g2 = i;
	assertEq(o.x, i);
	assertEq(o.g1, i);
	assertEq(o[44], i);

	o[44] = 33;
	assertEq(o.x, 33);
	assertEq(o.g1, 33);
	assertEq(o[44], 33);
    }
}
f();
