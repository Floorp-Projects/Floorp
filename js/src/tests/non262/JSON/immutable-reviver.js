// |reftest| skip-if(!this.hasOwnProperty("Record"))

const values = [];

const result = JSON.parseImmutable('{"x":1,"a":[1,2,{},[]]}', function (k, v) {
	values.push(#[k, v]);
	return v;
});

assertEq(result, #{x: 1, a: #[1, 2, #{}, #[]]});

const next = () => values.shift();
assertEq(next(), #["x", 1]);
assertEq(next(), #["0", 1]);
assertEq(next(), #["1", 2]);
assertEq(next(), #["2", #{}]);
assertEq(next(), #["3", #[]]);
assertEq(next(), #["a", #[1, 2, #{}, #[]]]);
assertEq(next(), #["", result]);
assertEq(values.length, 0);

if (typeof reportCompare === "function") reportCompare(0, 0);
