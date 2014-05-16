function neuterEventually(arr, i, variant)
{
  with (arr)
  {
    // prevent inlining
  }

  if (i === 2000)
    neuter(arr.buffer, variant);
}

function test(variant)
{
  var buf = new ArrayBuffer(1000);
  var ta = new Int8Array(buf);

  for (var i = 0; i < 2500; i++)
  {
    neuterEventually(ta, i, variant);
    assertEq(ta.length, i >= 2000 ? 0 : 1000);
  }
}

test("change-data");
test("same-data");
