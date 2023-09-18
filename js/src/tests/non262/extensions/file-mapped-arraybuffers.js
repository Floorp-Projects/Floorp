// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function viewToString(view)
{
  return String.fromCharCode.apply(null, view);
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

    // Test detaching during subarray creation.
    var nasty = {
      valueOf: function () {
        print("detaching...");
        serialize(buffer3, [buffer3]);
        print("detached");
        return 3000;
      }
    };

    var a = new Uint8Array(buffer3);
    assertThrowsInstanceOf(() => {
        var aa = a.subarray(0, nasty);
        for (i = 0; i < 3000; i++)
            aa[i] = 17;
    }, TypeError);

    // Check that invalid sizes and offsets are caught
    assertThrowsInstanceOf(() => createMappedArrayBuffer("empty.txt", 8), RangeError);
    assertThrowsInstanceOf(() => createMappedArrayBuffer("empty.txt", 0, 8), Error);
}

if (getBuildConfiguration("mapped-array-buffer"))
    test();
reportCompare(0, 0, 'ok');
