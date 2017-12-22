class base {}
class derived extends base {
  constructor() {
    try {
      return;
    } catch (e) {
      try {
        return;
      } catch (e) {}
    }
  }
}
assertThrowsInstanceOf(() => new derived, ReferenceError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
