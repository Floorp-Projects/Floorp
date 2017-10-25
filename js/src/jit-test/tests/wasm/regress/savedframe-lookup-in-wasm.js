g = newGlobal();
g.parent = this;
g.eval(`(function() {
  Debugger(parent).onExceptionUnwind = function(frame) frame.eval("")
})()`);

var module = new WebAssembly.Module(wasmTextToBinary(`
  (module (import $imp "" "inc") (func) (func $start (call $imp)) (start $start) (export "" $start))
`));

var imports = {
    "": {
        inc: function() { undefined_function(); }
    }
};

new WebAssembly.Instance(module, imports).exports['']();
