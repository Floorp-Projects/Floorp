// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var t = #[void 0];

assertEq(#[1, null, void 0].flat(), #[1, null, undefined]);
assertEq(#[1, #[null, void 0]].flat(), #[1, null, undefined]);
assertEq(#[#[null, void 0], #[null, void 0]].flat(), #[null, undefined, null, undefined]);
assertEq(#[1, #[null, t]].flat(1), #[1, null, t]);
assertEq(#[1, #[null, t]].flat(2), #[1, null, undefined]);

reportCompare(0, 0);
