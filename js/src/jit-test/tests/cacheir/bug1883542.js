Object.defineProperty(this, "getter", {get: foo});

var output;
let i = 20;
function foo() {
  eval("");
  output = this;
  if (i--) {
    getter++;
  }
}
foo();
output.newString();
