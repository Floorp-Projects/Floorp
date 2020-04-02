// |jit-test| --no-warp
// Disable WarpBuilder because the expression decompiler is not used for Ion
// frames currently. See bug 831120.

function check(f, message) {
  let caught = false;
  try {
    f();
  } catch (e) {
    assertEq(e.message, message);
    caught = true;
  }
  assertEq(caught, true);
}

check(() => {
  let obj = {
    prop: undefined
  };
  obj.prop.prop2();
}, "can't access property \"prop2\", obj.prop is undefined");

check(() => {
  let obj = {
    prop: null
  };
  obj.prop.prop2();
}, "can't access property \"prop2\", obj.prop is null");

check(() => {
  let prop = "prop";
  undefined[prop]();
}, "can't access property \"prop\" of undefined");

check(() => {
  let prop = "prop";
  null[prop]();
}, "can't access property \"prop\" of null");
