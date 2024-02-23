function oomTestEval(lfVarx) {
  oomTest(() => eval(lfVarx));
}

oomTestEval(``);
oomTestEval(`
  {
    function bug1661454() {  }
  }
`);
