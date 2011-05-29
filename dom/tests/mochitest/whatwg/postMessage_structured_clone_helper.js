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
  yield obj;
}
