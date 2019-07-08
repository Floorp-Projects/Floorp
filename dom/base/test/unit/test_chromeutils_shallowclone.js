"use strict";

add_task(function test_shallowclone() {
  // Check that shallow cloning an object with regular properties,
  // results into a new object with all properties from the source object.
  const fullyCloneableObject = {
    numProp: 123,
    strProp: "str",
    boolProp: true,
    arrayProp: [{ item1: "1", item2: "2" }],
    fnProp() {
      return "fn result";
    },
    promise: Promise.resolve("promised-value"),
    weakmap: new WeakMap(),
    proxy: new Proxy({}, {}),
  };

  let clonedObject = ChromeUtils.shallowClone(fullyCloneableObject);

  Assert.deepEqual(
    clonedObject,
    fullyCloneableObject,
    "Got the expected cloned object for an object with regular properties"
  );

  // Check that shallow cloning an object with getters and setters properties,
  // results into a new object without all the properties from the source object excluded
  // its getters and setters.
  const objectWithGetterAndSetter = {
    get myGetter() {
      return "getter result";
    },
    set mySetter(v) {},
    myFunction() {
      return "myFunction result";
    },
  };

  clonedObject = ChromeUtils.shallowClone(objectWithGetterAndSetter);

  Assert.deepEqual(
    clonedObject,
    {
      myFunction: objectWithGetterAndSetter.myFunction,
    },
    "Got the expected cloned object for an object with getters and setters"
  );

  // Check that shallow cloning a proxy object raises the expected exception..
  const proxyObject = new Proxy(fullyCloneableObject, {});

  Assert.throws(
    () => {
      ChromeUtils.shallowClone(proxyObject);
    },
    /Shallow cloning a proxy object is not allowed/,
    "Got the expected error on ChromeUtils.shallowClone called on a proxy object"
  );
});
