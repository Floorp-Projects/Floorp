// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

async function* gen(x) { yield x; }

const methods = ['map', 'filter', 'flatMap'];

for (const method of methods) {
  const iter = gen('value');
  const iterHelper = iter[method](x => iterHelper.next());
  iterHelper.next().then(
    _ => assertEq(true, false, 'Expected reject.'),
    err => assertEq(err instanceof TypeError, true),
  );
}

if (typeof reportCompare == 'function')
  reportCompare(0, 0);

