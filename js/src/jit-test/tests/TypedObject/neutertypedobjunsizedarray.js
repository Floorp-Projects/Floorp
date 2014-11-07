// Test the case where we neuter an instance of a variable-length array.
// Here we can fold the neuter check into the bounds check.

if (!this.hasOwnProperty("TypedObject"))
  quit();

var {StructType, uint32, storage} = TypedObject;
var S = new StructType({f: uint32, g: uint32});
var A = S.array(10);

function readFrom(a) {
  return a[2].f + a[2].g;
}

function main(variant) {
  var a = new A();
  a[2].f = 22;
  a[2].g = 44;

  for (var i = 0; i < 10; i++)
    assertEq(readFrom(a), 66);

  neuter(storage(a).buffer, variant);

  for (var i = 0; i < 10; i++) {
    var ok = false;

    try {
      readFrom(a);
    } catch (e) {
      ok = e instanceof TypeError;
    }

    assertEq(ok, true);
  }
}

main("same-data");
main("change-data");
