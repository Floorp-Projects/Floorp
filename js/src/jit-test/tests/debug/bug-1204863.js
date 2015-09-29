var dbg = newGlobal().Debugger(this);
dbg.onExceptionUnwind = function (frame, exc) {
  return { return:"sproon" };
};
Intl.Collator.supportedLocalesOf([2]);
