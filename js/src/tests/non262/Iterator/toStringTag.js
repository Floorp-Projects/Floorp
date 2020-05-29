// |reftest| skip-if(!this.hasOwnProperty('Iterator')) -- Iterator is not enabled unconditionally
const propDesc = Reflect.getOwnPropertyDescriptor(Iterator.prototype, Symbol.toStringTag);
assertEq(propDesc.value, 'Iterator');
assertEq(propDesc.writable, false);
assertEq(propDesc.enumerable, false);
assertEq(propDesc.configurable, true);

class TestIterator extends Iterator {}
assertEq(new TestIterator().toString(), '[object Iterator]');

if (typeof reportCompare == 'function')
  reportCompare(0, 0);
