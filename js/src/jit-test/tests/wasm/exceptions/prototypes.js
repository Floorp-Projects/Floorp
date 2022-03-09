class _Tag extends WebAssembly.Tag {}
class _Exception extends WebAssembly.Exception {}

let tag = new _Tag({parameters: []});
assertEq(tag instanceof _Tag, true);
assertEq(tag instanceof WebAssembly.Tag, true);

let exception = new _Exception(tag, []);
assertEq(exception instanceof _Exception, true);
assertEq(exception instanceof WebAssembly.Exception, true);
