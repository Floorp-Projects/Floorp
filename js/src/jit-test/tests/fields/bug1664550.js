class OverrideBase {
  constructor(o30) {
    return o30;
  }
};

class A3 extends OverrideBase {
  get #m() {}
}

var obj = {};
Object.seal(obj);
new A3(obj);
