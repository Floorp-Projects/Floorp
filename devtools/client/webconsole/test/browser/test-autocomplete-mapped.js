"use strict";
const i = { x: { y: { importResult: true } } };
const j = { get x() { return blackbox({ get y() { return blackbox({ getterResult: 1 }); } }); } };

const blackbox = x=>[x].pop();

function firstCall() {
  const t = 42;
  const u = i.x.y;
  const v = j.x.y.getterResult;
  const o = {
    get value() {
      return blackbox(Promise.resolve());
    }
  };
  debugger;
}
//# sourceMappingURL=test-autocomplete-mapped.js.map
