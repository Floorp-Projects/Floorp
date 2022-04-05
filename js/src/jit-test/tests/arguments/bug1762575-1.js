// Tests on mapped arguments objects.

function ArraySlice() {
  Object.defineProperty(arguments, 0, {value: 1});
  var result = Array.prototype.slice.call(arguments);
  assertEq(result[0], 1);
}
ArraySlice(0);

function ArrayShift() {
  Object.defineProperty(arguments, 0, {value: 1});
  var result = Array.prototype.shift.call(arguments);
  assertEq(result, 1);
}
ArrayShift(0);

function ArrayPop() {
  Object.defineProperty(arguments, 0, {value: 1});
  var result = Array.prototype.pop.call(arguments);
  assertEq(result, 1);
}
ArrayPop(0);

function ArrayJoin() {
  Object.defineProperty(arguments, 0, {value: 1});
  var result = Array.prototype.join.call(arguments);
  assertEq(result, "1");
}
ArrayJoin(0);

function ArrayIncludes() {
  Object.defineProperty(arguments, 0, {value: 1});
  var result = Array.prototype.includes.call(arguments, 1);
  assertEq(result, true);
}
ArrayIncludes(0);

function FunctionApply() {
  Object.defineProperty(arguments, 0, {value: 1});
  var id = x => x;
  var result = id.apply(null, arguments);
  assertEq(result, 1);
}
FunctionApply(0);
