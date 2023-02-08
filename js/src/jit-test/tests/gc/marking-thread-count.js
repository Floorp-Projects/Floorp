// |jit-test| skip-if: helperThreadCount() === 0

let initialHelperThreadCount = helperThreadCount();
let initialGCHelperThreadCount = gcparam('helperThreadCount');

let prevHelperThreadCount = initialHelperThreadCount;
for (let i of [0, 1, 4, 8, 4, 0]) {
  gcparam('markingThreadCount', i);
  assertEq(gcparam('markingThreadCount'), i);
  assertEq(gcparam('helperThreadCount'), initialGCHelperThreadCount);
  assertEq(helperThreadCount(), Math.max(prevHelperThreadCount, i));
  prevHelperThreadCount = helperThreadCount();
}
