// |reftest| skip-if(!this.hasOwnProperty("Tuple"))
var sample;

sample = #[4,3,2,1].toSorted();
assertEq(sample, #[1,2,3,4]);

sample = #[3, 4, 1, 2].toSorted();
assertEq(sample, #[1, 2, 3, 4]);

sample = #[3,4,3,1,0,1,2].toSorted();
assertEq(sample, #[0,1,1,2,3,3,4]);

sample = #[1,0,-0,2].toSorted();
// This matches the behavior of Array.sort()
assertEq(sample, #[0,-0,1,2]);

sample = #[-4,3,4,-3,2,-1,1,0].toSorted();
assertEq(sample, #[-1,-3,-4,0,1,2,3,4]);

sample = #[0.5,0,1.5,1].toSorted();
assertEq(sample, #[0,0.5,1,1.5]);

sample = #[0.5,0,1.5,-0.5,-1,-1.5,1].toSorted();
assertEq(sample, #[-0.5,-1,-1.5,0,0.5,1,1.5]);

sample = #[3,4,Infinity,-Infinity,1,2].toSorted();
assertEq(sample, #[-Infinity,1,2,3,4,Infinity]);

reportCompare(0, 0);
