function foo() {
    {
      let x=arguments;
      return function() { return x; };
    }
}
foo()();
