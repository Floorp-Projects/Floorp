// |reftest| skip-if(!this.hasOwnProperty("Record"))

assertEq(
	JSON.stringify(#{ x: 1, a: #[1, 2, #{}, #[]] }),
	'{"a":[1,2,{},[]],"x":1}'
);

if (typeof reportCompare === "function") reportCompare(0, 0);
