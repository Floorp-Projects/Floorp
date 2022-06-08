function f() {
  var i = 0;
  while (i < (this.foo = this.foo ^ 123)) {
    this.prop = 1;
  }
}
new f();
f();
