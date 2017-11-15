if (getBuildConfiguration().release_or_beta) {
    eval(`
    function f() {
      if (0)
        function g() { x };
      else;
    }
    f();`)
}
