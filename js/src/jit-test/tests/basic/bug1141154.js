function foo() {
  (function() {
    Object.preventExtensions(this);
    setJitCompilerOption("ion.warmup.trigger", 4);
    var g = newGlobal({newCompartment: true});
    g.debuggeeGlobal = this;
    g.eval("(" + function () {
        dbg = new Debugger(debuggeeGlobal);
        dbg.onExceptionUnwind = function (frame, exc) {
            var s = '!';
            for (var f = frame; f; f = f.older)
            debuggeeGlobal.log += s;
        };
    } + ")();");
    try {
      j('Number.prototype.toString.call([])');
    } catch (exc) {}
  })();
} foo();
