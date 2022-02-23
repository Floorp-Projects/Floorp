// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
assertEq(#[1, 2].flatMap(function(e) {
  return #[e, e * 2];
}), #[1, 2, 2, 4]);

var result = #[1, 2, 3].flatMap(function(ele) {
  return #[
    #[ele * 2]
  ];
});
assertEq(result.length, 3);
assertEq(result[0], #[2]);
assertEq(result[1] ,#[4]);
assertEq(result[2], #[6]);

reportCompare(0, 0);
