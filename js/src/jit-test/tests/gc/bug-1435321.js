// Check that corresponding parameters are updated to ensure that invariants are
// preserved when updating various GC parameters.

gcparam('highFrequencyHeapGrowthMin', 200);
gcparam('highFrequencyHeapGrowthMax', 400);
assertEq(gcparam('highFrequencyHeapGrowthMin'), 200);
assertEq(gcparam('highFrequencyHeapGrowthMax'), 400);

gcparam('highFrequencyHeapGrowthMax', 150);
assertEq(gcparam('highFrequencyHeapGrowthMin'), 150);
assertEq(gcparam('highFrequencyHeapGrowthMax'), 150);

gcparam('highFrequencyHeapGrowthMin', 300);
assertEq(gcparam('highFrequencyHeapGrowthMin'), 300);
assertEq(gcparam('highFrequencyHeapGrowthMax'), 300);

// The following parameters are stored in bytes but specified/retrieved in MiB.

gcparam('highFrequencyLowLimit', 200);
gcparam('highFrequencyHighLimit', 500);
assertEq(gcparam('highFrequencyLowLimit'), 200);
assertEq(gcparam('highFrequencyHighLimit'), 500);

gcparam('highFrequencyHighLimit', 100);
assertEq(gcparam('highFrequencyLowLimit'), 99);
assertEq(gcparam('highFrequencyHighLimit'), 100);

gcparam('highFrequencyLowLimit', 300);
assertEq(gcparam('highFrequencyLowLimit'), 300);
assertEq(gcparam('highFrequencyHighLimit'), 300);
