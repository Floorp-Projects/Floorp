evaluate(`
  var f = (function module() {
    "use asm";
    function f(i) {
        i=i|0;
        if (!i)
            return;
    }
    return f;
  })();
  evaluate(\`new f({}, {});\`);
`);
