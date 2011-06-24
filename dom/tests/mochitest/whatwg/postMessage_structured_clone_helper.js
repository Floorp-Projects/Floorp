function getTestContent()
{
  yield "hello";
  yield 2+3;
  yield 12;
  yield null;
  yield "complex" + "string";
  yield new Object();
  yield new Date(1306113544);
  yield [1, 2, 3, 4, 5];
  let obj = new Object();
  obj.foo = 3;
  obj.bar = "hi";
  obj.baz = new Date(1306113544);
  obj.boo = obj;
  yield obj;

  let recursiveobj = new Object();
  recursiveobj.a = recursiveobj;
  recursiveobj.foo = new Object();
  recursiveobj.foo.bar = "bar";
  recursiveobj.foo.backref = recursiveobj;
  recursiveobj.foo.baz = 84;
  recursiveobj.foo.backref2 = recursiveobj;
  recursiveobj.bar = new Object();
  recursiveobj.bar.foo = "foo";
  recursiveobj.bar.backref = recursiveobj;
  recursiveobj.bar.baz = new Date(1306113544);
  recursiveobj.bar.backref2 = recursiveobj;
  recursiveobj.expando = recursiveobj;
  yield recursiveobj;

  let obj = new Object();
  obj.expando1 = 1;
  obj.foo = new Object();
  obj.foo.bar = 2;
  obj.bar = new Object();
  obj.bar.foo = obj.foo;
  obj.expando = new Object();
  obj.expando.expando = new Object();
  obj.expando.expando.obj = obj;
  obj.expando2 = 4;
  obj.baz = obj.expando.expando;
  obj.blah = obj.bar;
  obj.foo.baz = obj.blah;
  obj.foo.blah = obj.blah;
  yield obj;

  let diamond = new Object();
  let obj = new Object();
  obj.foo = "foo";
  obj.bar = 92;
  obj.backref = diamond;
  diamond.ref1 = obj;
  diamond.ref2 = obj;
  yield diamond;

  let doubleref = new Object();
  let obj = new Object();
  doubleref.ref1 = obj;
  doubleref.ref2 = obj;
  yield doubleref;
}
