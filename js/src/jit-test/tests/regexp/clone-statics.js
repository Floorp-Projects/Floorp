// |jit-test| skip-if: helperThreadCount() === 0

offThreadCompileToStencil(`
  function foo(x, {}) {
    do {
      re = /erwe/;
      if (x === 1)
        re.x = 1;
      else
        re.x = "a";
      assertEq(re.x.length, (x === 1) ? undefined : 1);
    } while (!inIon());
  }

  foo(0, 0);
  RegExp.multiline = 1;
  foo(1, 0);
`);

var stencil = finishOffThreadStencil();
evalStencil(stencil);
