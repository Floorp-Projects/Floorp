function detachArrayBufferEventually(arr, i)
{
  with (arr)
  {
    // prevent inlining
  }

  if (i === 2000)
    detachArrayBuffer(arr.buffer);
}

function test()
{
  var buf = new ArrayBuffer(1000);
  var ta = new Int8Array(buf);

  for (var i = 0; i < 2500; i++)
  {
    detachArrayBufferEventually(ta, i);
    assertEq(ta.length, i >= 2000 ? 0 : 1000);
  }
}

test();
