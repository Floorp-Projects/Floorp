// |reftest| skip-if(!this.hasOwnProperty("Record"))

assertEq(
	JSON.parseImmutable('{"x":1,"a":[1,2,{},[]]}'),
	#{ x: 1, a: #[1, 2, #{}, #[]] }
);

assertEq(
	JSON.parseImmutable('{"a":1,"a":2}'),
	#{ a: 2 }
);

if (typeof reportCompare === "function") reportCompare(0, 0);
