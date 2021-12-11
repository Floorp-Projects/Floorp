// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator')) 

async function* gen() {
  yield 1;
  yield 2;
  yield 3;
}

gen().reduce((x, y) => `(${x}+${y})`, 0)
  .then(result => assertEq(result, '(((0+1)+2)+3)'));
gen().reduce((x, y) => `(${x}+${y})`)
  .then(result => assertEq(result, '((1+2)+3)'));

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
