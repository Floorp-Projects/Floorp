a = newGlobal({newCompartment: true});
a.parent = this;
a.eval("Debugger(parent).onExceptionUnwind=function(){}");
(function() {
  try {} finally {
    try {
      throw 9;
    } catch {}
  }
})()
