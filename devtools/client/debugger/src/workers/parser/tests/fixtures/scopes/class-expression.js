export {};

var Outer = class Outer {
  method() {
    var Inner = class {
      m() {
        console.log(this);
      }
    };
  }
};
