// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function viewToString(view)
{
  return String.fromCharCode.apply(null, view);
}

function assertThrows(f, wantException)
{
    try {
      f();
      assertEq(true, false, "expected " + wantException + " exception");
    } catch (e) {
      assertEq(e.name, wantException.name, e.toString());
    }
}

function test() {
    var filename = "file-mapped-arraybuffers.txt";
    var buffer = createMappedArrayBuffer(filename);
    var view = new Uint8Array(buffer);
    assertEq(viewToString(view), "01234567abcdefghijkl");

    var buffer2 = createMappedArrayBuffer(filename, 8);
    view = new Uint8Array(buffer2);
    assertEq(viewToString(view), "abcdefghijkl");

    var buffer3 = createMappedArrayBuffer(filename, 0, 8);
    view = new Uint8Array(buffer3);
    assertEq(viewToString(view), "01234567");

    // Check that invalid sizes and offsets are caught
    assertThrows(() => createMappedArrayBuffer("empty.txt", 8), RangeError);
    assertThrows(() => createMappedArrayBuffer("empty.txt", 0, 8), Error);
}

test();
reportCompare(0, 0, 'ok');
