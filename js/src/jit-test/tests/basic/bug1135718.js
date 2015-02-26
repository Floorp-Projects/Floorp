
setJitCompilerOption("ion.warmup.trigger", 30);
function ArrayCallback(state)
  this.state = state;
ArrayCallback.prototype.isUpperCase = function(v, index, array) {
    return this.state ? true : (v == v.toUpperCase());
};
strings = ['hello', 'Array', 'WORLD'];
obj = new ArrayCallback(false);
strings.filter(obj.isUpperCase, obj)
obj = new ArrayCallback(true);
strings.filter(obj.isUpperCase, obj)
obj.__proto__ = {};
