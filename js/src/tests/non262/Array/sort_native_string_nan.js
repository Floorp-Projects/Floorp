
var array = ["not-a-number", "also-not-a-number"];
var copy = [...array];

// The sort comparator must be exactly equal to the bytecode pattern:
//
// JSOp::GetArg 0/1
// JSOp::GetArg 1/0
// JSOp::Sub
// JSOp::Return
array.sort(function(a, b) { return a - b; });

assertEqArray(array, copy);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
