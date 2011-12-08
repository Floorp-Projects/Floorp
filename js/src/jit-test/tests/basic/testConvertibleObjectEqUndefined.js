function x4(v) { return "" + v + v + v + v; }
function testConvertibleObjectEqUndefined()
{
  var compares =
    [
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
     false, false, false, false,
     undefined, undefined, undefined, undefined,
    ];
  var count = 0;
  var obj = { valueOf: function() { count++; return 1; } };
  var results = compares.map(function(v) { return "unwritten"; });

  for (var i = 0, sz = compares.length; i < sz; i++)
    results[i] = compares[i] == obj;

  return results.join("") + count;
}

assertEq(testConvertibleObjectEqUndefined(),  
	 x4(false) + x4(false) + x4(false) + x4(false) + x4(false) + x4(false) +
	 x4(false) + x4(false) + x4(false) + x4(false) + "20");

