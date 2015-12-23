class base {}
class derived extends base {
  constructor() {
    try {
      (function() { p1(eval()) }())
    } catch (e) {
        return
    }
  }
}
assertThrowsInstanceOf(()=>new derived(), ReferenceError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
