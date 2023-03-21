with (newGlobal(this)) {
  eval(`
        function bar() {}
        function foo(a) {
          try {
            foo();
          } catch {
            bar(...arguments);
          }
        }
        foo();
  `);
}
