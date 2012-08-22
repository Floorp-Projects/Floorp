function C() {
  this.x = this[this.y = "foo"]--;
}

// Don't crash.
new C;
