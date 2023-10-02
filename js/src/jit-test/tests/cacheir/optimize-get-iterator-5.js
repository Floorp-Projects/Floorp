(() => {
    var returnCalled = false;
    Object.defineProperty(globalThis, 'x', {
      get() {
        return 42;
      },
      set(value) {
        ({}).__proto__.return = () => {
          returnCalled = true;
          return { value: 3, done: true };
        };
      }
    });

    [x] = [1, 2];

    assertEq(x, 42);
    assertEq(returnCalled, true);
})();
