var evalInFrame = (function (global) {
  var dbgGlobal = newGlobal();
  var dbg = new dbgGlobal.Debugger();

  return function evalInFrame(upCount, code) {
    dbg.addDebuggee(global);

    // Skip ourself.
    var frame = dbg.getNewestFrame().older;
    for (var i = 0; i < upCount; i++) {
      if (!frame.older)
        break;
      frame = frame.older;
    }

    var completion = frame.eval(code);
    if (completion.return) {
      var v = completion.return;
      if (typeof v === "object")
        v = v.unsafeDereference();
      return v;
    }
    if (completion.throw) {
      var v = completion.throw;
      if (typeof v === "object")
        v = v.unsafeDereference();
      throw v;
    }
    if (completion === null)
      terminate();
  };
})(this);
