// Test for invalidating shape teleporting when transplanting objects on proto
// chains.

function checkGetProp(obj, expected) {
    for (var i = 0; i < 50; i++) {
        assertEq(obj.prop, expected);
    }
}

Object.prototype.prop = 1234;

// Construct the following proto chain:
//
//   receiver => protoA (FakeDOMObject) => protoB {prop: 567} => null
const protoB = Object.create(null);
protoB.prop = 567;
const protoA = new FakeDOMObject();
Object.setPrototypeOf(protoA, protoB);
const receiver = Object.create(protoA);

// Ensure all objects we allocated are tenured. This way we don't need to trigger
// a GC later in TransplantableObject, which makes the test more reliable.
gc();

// Attach an IC for `receiver.prop`.
checkGetProp(receiver, 567);

// Swap protoA with another object. This must invalidate shape teleporting,
// because the proto chain of `receiver` now looks like this:
//
//   receiver => protoA (new FakeDOMObject) => FakeDOMObject.prototype => Object.prototype => null
const {transplant} = transplantableObject({object: protoA});
transplant(this);

// `receiver.prop` now gets `prop` from Object.prototype.
checkGetProp(receiver, 1234);
