load(libdir + 'evalInFrame.js');

class B {
  #priv() {
    return 12;
  }

  #privF = () => { return 12; }

  callPriv() {
    return this.#priv();
  }

  callPrivF() {
    return this.#privF();
  }

  #val = 'constructed';
  set #x(x) {
    this.#val = x + ' haha';
  }
  get #x() {
    return this.#val;
  }

  ef(str) {
    return evalInFrame(0, str);
  }

  layerEf(str) {
    // Nested functions here result in computeEffectiveScope traversing
    // more than one environment, necessitating more hops.
    function tester(o) {
      let a = 10;
      function interior(o) {
        let b = a;
        return evalInFrame(0, str.replace("this", "o"));
      };
      return interior(o);
    }
    return tester(this);
  }

  moreLayerEf(str) {
    // Nested functions here result in computeEffectiveScope traversing
    // more than one environment, necessitating more hops.
    function tester(o) {
      let a = 0;
      function interior(o) {
        let b = a;
        return (() => {
          let k = o;
          let replace = str.replace("this", "k");
          print(replace);
          return evalInFrame(b, replace);
        })();
      };
      return interior(o);
    }
    return tester(this);
  }

  callFunc(f) {
    return f();
  }

  static #smethod() {
    return 14;
  }

  static #unusedmethod() {
    return 19;
  }

  static get #unusedgetter() {
    return 10;
  }

  static setter = undefined;
  static set #unusedsetter(x) { this.setter = x }


  static f() {
    return this.#smethod();
  }

  static seval(str) {
    return eval(str);
  }

  static sef(str) {
    return evalInFrame(0, str);
  }


  static sLayerEf(str) {
    // Nested functions here result in computeEffectiveScope traversing
    // more than one environment, necessitating more hops.
    function tester(o) {
      let a = 10;
      function interior(o) {
        if (a == 10) {
          let b = 10 - a;
          return evalInFrame(b, str.replace("this", "o"));
        }
      };
      return interior(o);
    }
    return tester(this);
  }


}

var b = new B();

assertEq(b.ef("this.callPriv()"), 12);
assertEq(b.ef("this.callPrivF()"), 12);

// Private fields don't need brand checking and should succeed.
assertEq(b.ef("this.#val"), 'constructed')

// PrivF is a field, not a method, and so isn't brand checked like a method.
assertEq(b.ef(`this.callFunc(() => { return this.#privF() })`), 12);
assertEq(b.ef(`this.#privF()`), 12);

// Accesors are stored like fields, and so successfully execute, as they don't
// need brand checking.
assertEq(b.ef(`this.#x = 'Bye'; this.#x`), 'Bye haha');
assertEq(b.ef(`var x = () => { this.#x = 'Hi'; return this.#x}; x()`), 'Hi haha');


// // Private methods require very special brand checking.
assertEq(b.ef(`this.#priv()`), 12);
assertEq(b.ef(`let hello;
let f = () => {
  hello = this.#priv();
  assertEq(hello, 12);
};
f();
hello`), 12);
assertEq(b.layerEf(`this.#priv()`), 12);
assertEq(b.moreLayerEf(`this.#priv()`), 12);

if ('dis' in this) {
  // Ensure disassembly of GetAliasedDebugVar wroks.
  assertEq(b.ef(`dis(); this.#priv()`), 12);
}

assertEq(b.ef(`var x = () => { return this.#priv(); }; x()`), 12);
assertEq(b.ef(`function x(o) { function y(o) { return o.#priv(); }; return y(o); } x(this)`), 12);

assertEq(b.ef("B.#smethod()"), 14)
assertEq(b.ef("B.#unusedmethod()"), 19);
assertEq(b.ef("B.#unusedgetter"), 10);

b.ef("B.#unusedsetter = 19");
assertEq(B.setter, 19);

assertEq(B.f(), 14);
assertEq(B.sef(`this.#smethod()`), 14);
assertEq(B.sLayerEf(`this.#smethod()`), 14);
assertEq(B.sef("this.#unusedmethod()"), 19);
assertEq(B.sef("this.#unusedgetter"), 10);


B.sef("this.#unusedsetter = 13");
assertEq(B.setter, 13);

// Test case variant from Arai.
class C {
  #priv() {
    return 12;
  }

  efInFunction(str) {
    return (() => {
      let self = this;
      return evalInFrame(0, str);
    })();
  }
}
c = new C;
assertEq(c.efInFunction(`self.#priv()`), 12);

// JIT testing
assertEq(b.ef(`
let result;
let f = () => {
  result = this.#priv();
  assertEq(result, 12);
};
for (let i = 0; i < 1000; i++) {
  f();
}
result
`), 12);

assertEq(b.ef("function f(o) { return o.callPriv() }; for (let i = 0; i < 1000; i++) { f(this); } f(this)"), 12);
assertEq(b.ef("function f(o) { return o.callPrivF() }; for (let i = 0; i < 1000; i++) { f(this); } f(this)"), 12);
assertEq(b.ef(`function x(o) { function y(o) { return o.#priv(); }; return y(o); } x(this)`), 12);

assertEq(B.sef(`function f(o) { return o.#smethod() }; for (let i = 0; i < 1000; i ++) { f(this); }; f(this)`), 14);

assertEq(b.ef(`
var x = () => {
  return (() => {
    return (() => {
      let a;
      return (() => {
        let b = a;
        return this.#priv();
      })();
    })();
  })();
};
x()
`), 12);