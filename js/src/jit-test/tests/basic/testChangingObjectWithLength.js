function testChangingObjectWithLength()
{
  var obj = { length: 10 };
  var dense = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
  var slow = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]; slow.slow = 5;

  /*
   * The elements of objs constitute a De Bruijn sequence repeated 4x to trace
   * and run native code for every object and transition.
   */
  var objs = [obj, obj, obj, obj,
              obj, obj, obj, obj,
              dense, dense, dense, dense,
              obj, obj, obj, obj,
              slow, slow, slow, slow,
              dense, dense, dense, dense,
              dense, dense, dense, dense,
              slow, slow, slow, slow,
              slow, slow, slow, slow,
              obj, obj, obj, obj];

  var counter = 0;

  for (var i = 0, sz = objs.length; i < sz; i++)
  {
    var o = objs[i];
    for (var j = 0; j < o.length; j++)
      counter++;
  }

  return counter;
}
assertEq(testChangingObjectWithLength(), 400);
checkStats({
  recorderAborted: 0,
  sideExitIntoInterpreter: 15 // empirically determined
});
