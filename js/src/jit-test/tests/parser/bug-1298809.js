if (getBuildConfiguration().release_or_beta) {
    eval(`
    function f() {
      // The expression closure is deliberate here, testing the semicolon after
      // one when it appears as a FunctionDeclaration from B.3.4
      // FunctionDeclarations in IfStatement Statement Clauses.
      if (0)
        function g() x;
      else;
    }
    f();`)
}
