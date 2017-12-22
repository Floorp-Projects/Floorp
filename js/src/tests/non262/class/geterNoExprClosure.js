// getter/setter with expression closure is allowed only in object literal.

assertThrowsInstanceOf(() => eval(`
  class foo {
    constructor() {}

    get a() 1
  }
`), SyntaxError);

assertThrowsInstanceOf(() => eval(`
  class foo {
    constructor() {}

    set a(v) 1
  }
`), SyntaxError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
