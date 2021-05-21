function Base() {
  // Creates MNewObject, which is recoverable. An instruction which has the
  // |RecoveredOnBailout| flag set mustn't have any live uses.
  return {};
}

class C extends Base {
  constructor() {
    // |super()| assigns to |this|. The |this| slot mustn't be optimised away
    // in case the debugger tries to read that slot.
    super();
  }
}

for (var i = 0; i < 100; i++) {
  // The returned object is not used, so it can be optimised away.
  new C();
}
