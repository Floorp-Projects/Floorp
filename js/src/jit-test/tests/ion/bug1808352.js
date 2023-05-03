let domObject = new FakeDOMObject();
let expectedValue = domObject.x;

let {object, transplant} = transplantableObject({object: domObject});
assertEq(object, domObject);

let global1 = newGlobal({newCompartment: true});
let global2 = newGlobal({newCompartment: true});

transplant(global1);
transplant(global2);
transplant(global1);

assertEq(object, domObject);
assertEq(domObject.x, expectedValue);

global1.domObj = domObject;
global1.expectedValue = expectedValue;

global1.evaluate(`
function f() {
    for (var i = 0; i < 2000; i++) {
        assertEq(domObj.x, expectedValue);
    }
}
f();
`);
