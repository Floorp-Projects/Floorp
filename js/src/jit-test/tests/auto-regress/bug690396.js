// Binary: cache/js-dbg-64-ff51ddfdf5d1-linux
// Flags:
//

function makeGenerator() {
  yield function generatorClosure() {};
}
var generator = makeGenerator();
if (typeof findReferences == 'function') {
  findReferences(generator);
}
