// |jit-test| error: ReferenceError

let m = parseModule(`
var i = 0;
addThis();
function addThis()
  statusmessages[i] = Number;
`);
m.declarationInstantiation();
m.evaluation();
