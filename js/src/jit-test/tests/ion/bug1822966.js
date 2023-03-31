// |jit-test| --no-threads

function bar() { return Math; }

function foo() {
    for (var i = 0; i < 50; i++) {
      const arr = [1];
      const arr2 = [1];
      arr.h = 2;
      for (var j = 0; j < 10; j++) {
          for (var n = 0; n < 3000; n++) {}

          for (var k = 0; k < 101; k++) {
        bar();
          }
      }

      arr.a = i;
      arr2.x = 1;
    }
}
foo()
