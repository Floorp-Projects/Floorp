// |reftest| skip-if(!this.hasOwnProperty("Record"))

assertEq(
	JSON.stringify(#{ x: 1, a: #[1, 2, #{}, #[]] }),
	'{"a":[1,2,{},[]],"x":1}'
);

assertEq(
	JSON.stringify({ rec: Object(#{ x: 1 }), tup: Object(#[1, 2]) }),
	'{"rec":{"x":1},"tup":[1,2]}'
);

if (typeof reportCompare === "function") reportCompare(0, 0);
