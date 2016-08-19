if (!this.hasOwnProperty("TypedObject"))
  quit();

var {StructType, uint32, storage} = TypedObject;
var S = new StructType({f: uint32, g: uint32});

function readFromS(s) {
  return s.f + s.g;
}

function main() {
  var s = new S({f: 22, g: 44});

  for (var i = 0; i < 10; i++)
    assertEq(readFromS(s), 66);

  detachArrayBuffer(storage(s).buffer);

  for (var i = 0; i < 10; i++) {
    var ok = false;

    try {
      readFromS(s);
    } catch (e) {
      ok = e instanceof TypeError;
    }

    assertEq(ok, true);
  }
}

main();
