// |reftest| 
var C = class {
  static #field = () => 'Test262';
  static field = () => 'Test262';
  #instance = () => 'Test262';
  instance = () => 'Test262';

  static accessPrivateField() {
    return this.#field;
  }

  accessPrivateInstanceField() {
    return this.#instance;
  }

  static accessField() {
    return this.field;
  }

  accessInstanceField() {
    return this.instance;
  }
}
assertEq(C.accessPrivateField().name, '#field')
assertEq(C.accessField().name, 'field');
var c = new C;
assertEq(c.accessPrivateInstanceField().name, '#instance');
assertEq(c.accessInstanceField().name, 'instance');

if (typeof reportCompare === 'function') reportCompare(0, 0);
