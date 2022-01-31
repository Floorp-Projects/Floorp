// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
assertDeepEq([1, 2].concat(#[3, 4]), [1, 2, 3, 4]);
assertDeepEq([].concat(#[3, 4]), [3, 4]);
assertDeepEq([].concat(#[]), []);
assertDeepEq([1, 2, 3].concat(#[]), [1, 2, 3]);

reportCompare(0, 0);
