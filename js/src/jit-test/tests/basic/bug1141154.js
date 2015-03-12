function foo() {
  (function() {
    Object.preventExtensions(this);
    setJitCompilerOption("ion.warmup.trigger", 4);
    var g = newGlobal();
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
      j('Number.prototype.toSource.call([])');
    } catch (exc) {}
  })();
} foo();
