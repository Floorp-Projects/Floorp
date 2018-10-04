// |jit-test| error: ReferenceError

let m = parseModule(`
var i = 0;
addThis();
function addThis() {
  return statusmessages[i] = Number;
}
`);
m.declarationInstantiation();
m.evaluation();
