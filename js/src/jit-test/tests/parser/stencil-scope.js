const optionsFull = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "ParseEverythingEagerly",
};

const optionsLazy = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "OnDemandOnly",
};

const optionsLazyCache = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "ConcurrentDepthFirst",
};

const optionsLazyCache2 = {
    fileName: "compileToStencil-DATA.js",
    lineNumber: 1,
    eagerDelazificationStrategy: "ConcurrentLargeFirst",
};

let result = 0;

function testMainThread(script_str) {
    const stencil = compileToStencil(script_str, optionsFull);
    result = evalStencil(stencil, optionsFull);
    assertEq(result, 1);
}

function testMainThreadDelazifyAll(script_str) {
    if (isLcovEnabled()) {
      // Code-coverage implies forceFullParse = true, and as such it cannot be
      // used while testing to incrementally delazify.
      return;
    }
    const stencil = compileToStencil(script_str, optionsLazy);
    result = evalStencil(stencil, optionsLazy);
    assertEq(result, 1);
}

function testMainThreadCacheAll(script_str) {
  if (isLcovEnabled() || helperThreadCount() === 0) {
    // Code-coverage implies forceFullParse = true, and as such it cannot be
    // used while testing to incrementally delazify.
    // Similarly, concurrent delazification requires off-threads processing.
    return;
  }
  const stencil = compileToStencil(script_str, optionsLazyCache);
  result = evalStencil(stencil, optionsLazyCache);
  assertEq(result, 1);
}

function testMainThreadCacheAll2(script_str) {
  if (isLcovEnabled() || helperThreadCount() === 0) {
    // Code-coverage implies forceFullParse = true, and as such it cannot be
    // used while testing to incrementally delazify.
    // Similarly, concurrent delazification requires off-threads processing.
    return;
  }
  const stencil = compileToStencil(script_str, optionsLazyCache2);
  result = evalStencil(stencil, optionsLazyCache2);
  assertEq(result, 1);
}

function testOffThread(script_str) {
    const job = offThreadCompileToStencil(script_str, optionsFull);
    const stencil = finishOffThreadStencil(job);
    result = evalStencil(stencil, optionsFull);
    assertEq(result, 1);
}

// These patches are meant to wrap the inner code given as argument into one
// kind of scope. The freeVars specify one way to retrieve the variable name
// added in this process if any.
const scopeCases = [
    { code: inner => `{ ${inner} }`, freeVars: [] },
    { code: inner => `{ var v = 1; ${inner} }`, freeVars: ["v"] },
    { code: inner => `{ let l = 1; ${inner} }`, freeVars: ["l"] },
    { code: inner => `{ const c = 1; ${inner} }`, freeVars: ["c"] },
    { code: inner => `with ({ p: 1 }) { ${inner} }`, freeVars: ["p"],
      inClass: false },
    { code: inner => `(a => { ${inner} })(1)`, freeVars: ["a"] },
    { code: inner => `function fun(a) { ${inner} }; fun(1)`, freeVars: ["a"],
      inClass: false},
    { code: inner => `try { ${inner} } catch(unused) { }`, freeVars: [] },
    { code: inner => `try { throw 1; } catch(t) { ${inner} }`, freeVars: ["t"] },
    { code: inner => `{ class C { #m = 1; constructor() { ${inner} }}; new C() }`,
      freeVars: ["this.#m"], isClass: true },
];

// This function is used to generate code which mostly exercise the various kind
// of scopes to cover ScopeContext class in CompilationStencil.h
function generateCode(seed) {
    let start = inner => `
      ${inner};
      result
    `;

    let prog = [start];
    let freeVars = ["1"];
    let inClass = false;

    while (seed >= freeVars.length) {
        let index = seed % scopeCases.length;
        seed = (seed / scopeCases.length) | 0;
        let scope = scopeCases[index];
        if (inClass && !(scope.inClass ?? false)) {
            // Skip illegal code (non-strict) or code which might not accept
            // this to work.
            continue;
        }
        inClass ||= scope.isClass ?? false;
        prog.push(scope.code);
        freeVars = freeVars.concat(scope.freeVars);
    }

    let name = freeVars[seed];
    return prog.reduceRight((inner, f) => f(inner), `result = ${name}`);
}

for (let s = 0; s < 3000; s++) {
    let code = generateCode(s);
    // console.log(s, ":", code);
    testMainThread(code);
    testMainThreadDelazifyAll(code);
    testMainThreadCacheAll(code);
    testMainThreadCacheAll2(code);
    if (helperThreadCount() > 0) {
        testOffThread(code);
    }
}
