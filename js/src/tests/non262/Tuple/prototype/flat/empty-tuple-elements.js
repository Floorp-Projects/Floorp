// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var t = #[];
assertEq(#[].flat(), #[]);
assertEq(#[#[],#[]].flat(), #[]);
assertEq(#[#[], #[1]].flat(), #[1]);
assertEq(#[#[], #[1, t]].flat(), #[1, t]);

reportCompare(0, 0);
