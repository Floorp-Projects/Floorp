// |reftest| skip-if(!this.hasOwnProperty("Record"))

function test(input, query, same) {
	assertEq(set(input).has(query), same);
	assertEq(new Map([[input, 1]]).has(query), same);
	if (same) assertEq(new Map([[input, 1]]).get(query), 1);
}
function first(it) {
	for (const v of it) return v;
}
function set(v) {
	return new Set([v]);
}


test(#[0, 1], #[0, 1], true);
test(#[0, 1], #[1, 0], false);

test(#{x: 1}, #{x: 1}, true);
test(#{x: 1}, #{y: 1}, false);
test(#{x: 1}, #{x: 0}, false);
test(#{x: 0, y: 1}, #{x: 0, y: 1}, true);
test(#{x: 0, y: 1}, #{y: 1, x: 0}, true);
test(#{x: 0, y: 1}, #{x: 1, y: 0}, false);
test(#{x: 0, y: 1}, #{x: 0, y: 0}, false);

test(#[NaN], #[NaN], true);
test(#{x: NaN}, #{x: NaN}, true);
test(#[+0], #[-0], true);
test(#{x: +0}, #{x: -0}, true);
assertEq(Object.is(first(set(#[+0]))[0], +0), true);
assertEq(Object.is(first(set(#[-0]))[0], -0), true);
assertEq(Object.is(first(set(#{x: +0})).x, +0), true);
assertEq(Object.is(first(set(#{x: -0})).x, -0), true);

// Test ropes.
test(#["ab" + String.fromCodePoint(67)], #["ab" + String.fromCodePoint(67)], true);
test(#{ x: "ab" + String.fromCodePoint(67) }, #{ x: "ab" + String.fromCodePoint(67) }, true);
test(#["ab" + String.fromCodePoint(67)], #["ab" + String.fromCodePoint(68)], false);
test(#{ x: "ab" + String.fromCodePoint(67) }, #{ x: "ab" + String.fromCodePoint(68) }, false);

if (typeof reportCompare === "function") reportCompare(0, 0);
