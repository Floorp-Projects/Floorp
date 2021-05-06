function checkValues() {
  const a = "";
  const b = false;
  const c = undefined;
  const d = null;
  const e = [];
  const f = {};

  const obj = {
    foo: 1,
  };

  let bs = [];
  for (let i = 0; i <= 100; i++) {
    bs.push({ a: 2, b: { c: 3 } });
  }
  debugger;
}

function columnWise() {
  let a = "a";
  let b = "b";
  let c = "c";
  console.log(c, a, b);
  debugger;
}

function objectProperties() {
  const obj = { hello: "world", a: { b: "c" } };
  console.log(obj.hello);
  console.log(obj.a.b);
  debugger;
}

function classProperties() {
  class Foo {
    x = 1;
    #privateVar = 2;
    #privateMethod() {
      return this.#privateVar;
    }
    breakFn() {
      let i = this.x * this.#privateVar;
      debugger;
    }
  }
  const foo = new Foo();
  foo.breakFn();
}

function btnClick() {
  const btn = document.querySelector("button");
  debugger;
  btn.click();
}

function onBtnClick(event) {
  debugger;
}
