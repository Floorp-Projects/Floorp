const m1 = parseModule(`
  export let mod = {};
  function setter(elem) {
    delete Array.prototype[0]; // Delete the setter
    this.push(elem); // Push the element for real
    mod = elem; // Export the gathered module
  }
  Array.prototype.__defineSetter__(0, setter);
  export const _foo = await Promise.resolve(5);
`);

const m2 = parseModule(`
  import {mod} from 'm1';
  assertEq(mod.status, undefined);
`);

registerModule('m1', m1);
moduleLink(m2);
moduleEvaluate(m2);
drainJobQueue();
