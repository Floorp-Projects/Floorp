if (!('oomTest' in this))
    quit();

function parseAsmJS() {
    eval(`function m(stdlib)
          {
            "use asm";
            var abs = stdlib.Math.abs;
            function f(d)
            {
              d = +d;
              return (~~(5.0 - +abs(d)))|0;
            }
            return f;
          }`);
}
oomTest(parseAsmJS);
