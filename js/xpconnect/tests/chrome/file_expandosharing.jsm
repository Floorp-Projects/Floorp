this.EXPORTED_SYMBOLS = ['checkFromJSM'];

this.checkFromJSM = function checkFromJSM(target, is_op) {
  is_op(target.numProp, 42, "Number expando works");
  is_op(target.strProp, "foo", "String expando works");
  // If is_op is todo_is, target.objProp will be undefined.
  try {
    is_op(target.objProp.bar, "baz", "Object expando works");
  } catch(e) { is_op(0, 1, "No object expando"); }
}
