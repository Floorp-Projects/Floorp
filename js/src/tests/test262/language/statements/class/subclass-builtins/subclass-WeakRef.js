// |reftest| skip-if(!this.hasOwnProperty('WeakRef')) -- WeakRef is not enabled unconditionally
// This file was procedurally generated from the following sources:
// - src/subclass-builtins/WeakRef.case
// - src/subclass-builtins/default/statement.template
/*---
description: new SubWeakRef() instanceof WeakRef (Subclass instanceof Heritage)
features: [WeakRef]
flags: [generated]
---*/


class Subclass extends WeakRef {}

const sub = new Subclass({});
assert(sub instanceof Subclass);
assert(sub instanceof WeakRef);

reportCompare(0, 0);
