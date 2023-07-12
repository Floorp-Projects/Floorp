// |jit-test| --baseline-eager

function foo(src) {
  try {
    evaluate(src);
  } catch {}
  Math = undefined;
}

foo(``);
foo(``);
foo(`var a = 0;`);
foo(`var b = 0;`);
foo(`var c = 0;`);
foo(`var d = 0;`);
foo(`undef();`);
foo(`{`);
foo(`
   gc();
   e = 0;
   gc();
`);
foo(`
   var f = 0;
   gc();
   delete Math;
`);
foo(`
   g = undefined;
`);
