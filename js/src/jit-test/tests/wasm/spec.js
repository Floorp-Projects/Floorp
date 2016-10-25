// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

// This is meant to be a small and dumb interpreter for wast files. Either it
// is imported by another script, which needs to define an array of arguments
// called importedArgs, or args need to be passed to the command line.
//
// Possible arguments include:
// -d           enable debug verbose mode
// -c           computes line numbers in wast files (disabled by default as it
//              slows down the parser a lot)
// -s           soft fail mode: if a test fails, don't abort but continue to
//              the next one.
// *            anything else is considered a relative path to the wast file to
//              load and run. The path is relative to to the runner script,
//              i.e. this file..
//
//  If there are no arguments, the wast interpreter will run sanity checks
//  (testing that NaN payload comparisons works, e.g.).

if (typeof assert === 'undefined') {
    var assert = function(c, msg) {
        assertEq(c, true, msg);
    };
}

// Element list or string.
function Element(str, dollared, quoted) {
    this.list = [];
    this.str = str === undefined ? null : str;
    this.dollared = !!dollared;
    this.quoted = !!quoted;
}

Element.prototype.toString = function() {
    if (this.str !== null) {
        if (this.dollared) {
            return "$" + this.str;
        }
        if (this.quoted) {
            return `"${this.str}"`;
        }
        return this.str;
    }
    return `(${this.list.map(x => x.toString()).join(" ")})`;
};

setJitCompilerOption('wasm.test-mode', 1);

// Creates a tree of s-expressions. Ported from Binaryen's SExpressionParser.
function parseSExpression(text) {
    var pos = 0;

    function isSpace(c) {
        switch (c) {
            case '\n':
            case ' ':
            case '\r':
            case '\t':
            case '\v':
            case '\f':
                return true;
            default:
                return false;
        }
    }

    function skip() {
        while (true) {
            let prevPos = pos;

            if (pos + 2 < text.length) {

                // Block comments.
                if (text[pos] === '(' && text[pos + 1] === ';')
                {
                    pos += 2;
                    let blockDepth = 1;
                    while (pos + 2 < text.length) {
                        if (text[pos] === '(' && text[pos + 1] === ';') {
                            pos += 2;
                            blockDepth++;
                        } else if (text[pos] === ';' && text[pos + 1] === ')') {
                            pos += 2;
                            if (!--blockDepth)
                                break;
                        } else {
                            pos++;
                        }
                    }
                }

                // Inline comments.
                if (text[pos] === ';' && text[pos + 1] === ';') {
                    pos += 2;
                    while (text[pos] !== '\n')
                        pos++;
                }
            }

            // Whitespaces.
            while (isSpace(text[pos])) {
                pos++;
            }

            if (pos === prevPos)
                break;
        }
    }

    function parse() {
        skip();

        if (text.length === pos || text[pos] === ')')
            return null;

        if (text[pos] === '(') {
            pos++;
            var ret = parseInParens();
            skip();
            assert(text[pos] === ')', 'inner list ends with a )');
            pos++;
            return ret;
        }

        return parseString();
    }

    function parseInParens() {
        skip();
        var start = pos;
        var ret = new Element();
        while (true) {
            var curr = parse();
            if (!curr) {
                ret.lineno = countLines(text, pos);
                return ret;
            }
            ret.list.push(curr);
        }
    }

    function parseString() {
        var dollared = false;
        var quoted = false;
        if (text[pos] === '$') {
            pos++;
            dollared = true;
        }

        var start = pos;
        if (text[pos] === '"') {
            quoted = true;
            // Parse escaping \", but leave code escaped - we'll handle escaping in memory segments specifically.
            pos++;
            var str = "";
            while (true) {
                if (text[pos] === '"') break;
                if (text[pos] === '\\') {
                    str += text[pos];
                    str += text[pos + 1];
                    pos += 2;
                    continue;
                }
                str += text[pos];
                pos++;
            }
            pos++;
            return new Element(str, dollared, quoted);
        }

        while (pos < text.length &&
               !isSpace(text[pos]) &&
               text[pos] != ';' &&
               text[pos] != ')' &&
               text[pos] != '(') {
            pos++;
        }

        return new Element(text.substring(start, pos), dollared);
    }

    var root = null;
    while (!root) { // Keep parsing until we pass an initial comment.
        root = parseInParens();
    }
    return root;
}

var imports = {
    spectest: {
        print,
        global: 666,
        table: new WebAssembly.Table({ initial: 10, maximum: 20, element: "anyfunc" }),
        memory: new WebAssembly.Memory({ initial: 1, maximum: 2 }),
    }
};

function handleNonStandard(exprName, e)
{
    if (exprName === 'quit') {
        quit();
    }
    if (exprName === 'print') {
        print.apply(null, e.list.slice(1).map(exec))
        return true;
    }
    return false;
}

function testNaNEqualityFunction() {
    // Test NaN equality functions.
    let u8 = new Uint8Array(16);
    let i32 = new Int32Array(u8.buffer);
    let f64 = new Float64Array(u8.buffer);
    let f32 = new Float32Array(u8.buffer);

    // F64 NaN
    let someNaN = wasmEvalText('(module (func (result f64) (f64.const -nan:0x12345678)) (export "" 0))').exports[""]();
    i32[0] = someNaN.nan_low;
    i32[1] = someNaN.nan_high;
    assert(Number.isNaN(f64[0]), "we've stored a f64 NaN");

    assertEq(u8[0], 0x78);
    assertEq(u8[1], 0x56);
    assertEq(u8[2], 0x34);
    assertEq(u8[3], 0x12);

    assertEqNaN(someNaN, someNaN);

    // F32 NaN
    someNaN = wasmEvalText('(module (func (result f32) (f32.const -nan:0x123456)) (export "" 0))').exports[""]();
    i32[0] = someNaN.nan_low;
    assert(Number.isNaN(f32[0]), "we've stored a f32 NaN");

    assertEq(u8[0], 0x56);
    assertEq(u8[1], 0x34);
    assertEq(u8[2] & 0x7f, 0x12);

    assertEqNaN(someNaN, someNaN);

    // Compare a NaN value against another one.
    let pNaN = wasmEvalText('(module (func (result f64) (f64.const nan)) (export "" 0))').exports[""]();
    let nNaN = wasmEvalText('(module (func (result f64) (f64.const -nan)) (export "" 0))').exports[""]();

    i32[0] = pNaN.nan_low;
    i32[1] = pNaN.nan_high;
    i32[2] = nNaN.nan_low;
    i32[3] = nNaN.nan_high;

    assertEq(f64[0], f64[1]);
    assertErrorMessage(() => assertEqNaN(pNaN, nNaN), Error, /Assertion failed/);
    assertEqNaN(pNaN, pNaN);
    assertEqNaN(nNaN, nNaN);
}

var constantCache = new Map;
var moduleCache = new Map;

function getModuleAndField(e) {
    let nextArgIndex = 1;
    let nameExpr = e.list[nextArgIndex];
    let name = nameExpr.str;

    let moduleName = '__last_module__';
    if (nameExpr.dollared && !nameExpr.quoted) {
        moduleName = name;
        nextArgIndex += 1;
    }

    if (!moduleCache.has(moduleName)) {
        throw new Error('We should have a module here before trying to invoke things!');
    }

    let module = moduleCache.get(moduleName);
    let fieldName = e.list[nextArgIndex++].str;
    let rest = e.list.slice(nextArgIndex).map(exec);

    return [module, fieldName, rest];
}

// Recursively execute the expression.
function exec(e) {
    var exprName = e.list[0].str;

    if (exprName === "module") {
        let moduleText = e.toString();

        let moduleName = null;
        if (e.list && e.list.length >= 2 && e.list[1].str && e.list[1].dollared) {
            moduleName = e.list[1].str;
            moduleText = moduleText.replace(`$${moduleName}`, '');
        }

        let module = wasmEvalText(moduleText, imports).exports;
        moduleCache.set('__last_module__', module);
        if (moduleName) {
            moduleCache.set(moduleName, module);
        }

        return;
    }

    if (exprName === "register") {
        // (register IMPORT_NAME MODULE_NAME?)
        assert(e.list[1].quoted, "first arg to register is quoted");
        let importName = e.list[1].str;

        let moduleName = '__last_module__';
        if (e.list.length > 2) {
            moduleName = e.list[2].str;
        }

        if (!moduleCache.has(moduleName)) {
            throw new Error("can't register an unknown module for imports");
        }

        let module = moduleCache.get(moduleName);

        imports[importName] = {};

        for (let [k, v] of Object.entries(module)) {
            imports[importName][k] = v;
        }

        return;
    }

    if (exprName === "invoke") {
        let [module, field, args] = getModuleAndField(e);

        let fn = null;
        if (typeof module[field] === "function") {
            fn = module[field];
        } else {
            throw new Error("Exported function not found: " + e);
        }

        return fn.apply(null, args);
    }

    if (exprName === "get") {
        let [module, field, args] = getModuleAndField(e);
        return module[field];
    }

    if (exprName.indexOf(".const") > 0) {
        // Eval the expression using a wasm module.
        var type = exprName.substring(0, exprName.indexOf(".const"));
        var key = e.toString();

        if (constantCache.has(key)) {
            return constantCache.get(key);
        }

        var val = wasmEvalText(`(module (func (result ${type}) ${e}) (export "" 0))`).exports[""]();
        constantCache.set(key, val);
        return val;
    }

    if (exprName === "assert_return") {
        let lhs = exec(e.list[1]);
        // There might be a value to test against.
        if (e.list[2]) {
            let rhs = exec(e.list[2]);
            if (typeof lhs === 'number') {
                assertEq(lhs, rhs);
            } else if (typeof lhs.nan_low === 'number') {
                assertEqNaN(lhs, rhs);
            } else {
                // Int64 are emulated with objects with shape:
                // {low: Number, high: Number}
                assert(typeof lhs.low === 'number', 'assert_return expects NaN, int64 or number');
                assert(typeof lhs.high === 'number', 'assert_return expects NaN, int64 or number');
                assertEq(lhs.low, rhs.low);
                assertEq(lhs.high, rhs.high);
            }
        }
        return;
    }

    if (exprName === "assert_return_nan") {
        let res = exec(e.list[1]);
        if (typeof res === 'number') {
            assertEq(res, NaN);
        } else {
            assert(typeof res.nan_low === 'number',
                   "assert_return_nan expects either a NaN number or a NaN custom object");

            let f64 = new Float64Array(1);
            let f32 = new Float32Array(f64.buffer);
            let i32 = new Int32Array(f64.buffer);

            i32[0] = res.nan_low;
            i32[1] = res.nan_high;
            assert(Number.isNaN(f64[0]) || Number.isNaN(f32[0]), "assert_return_nan test failed.");
        }
        return;
    }

    if (exprName === "assert_invalid") {
        let moduleText = e.list[1].toString();
        let errMsg = e.list[2];
        if (errMsg) {
            assert(errMsg.quoted, "assert_invalid second argument must be a string");
            errMsg.quoted = false;
        }
        // assert_invalid tests both the decoder *and* the parser itself.
        try {
            assertEq(WebAssembly.validate(wasmTextToBinary(moduleText)), false, "assert_invalid failed");
        } catch(e) {
            if (/wasm text error/.test(e.toString()))
                return;
            throw e;
        }
        return;
    }

    if (exprName === 'assert_trap') {
        let caught = false;
        let errMsg = e.list[2];
        assert(errMsg.quoted, "assert_trap second argument must be a string");
        errMsg.quoted = false;
        try {
            exec(e.list[1]);
        } catch(err) {
            caught = true;
            if (err.toString().indexOf(errMsg) === -1)
                warn(`expected error message "${errMsg}", got "${err}"`);
        }
        assert(caught, "assert_trap exception not caught");
        return;
    }

    if (exprName === 'assert_unlinkable') {
        let moduleText = e.list[1].toString();
        let errMsg = e.list[2];
        if (errMsg) {
            assert(errMsg.quoted, "assert_invalid second argument must be a string");
            errMsg.quoted = false;
        }
        let module = new WebAssembly.Module(wasmTextToBinary(moduleText));
        let caught = false;
        try {
            new WebAssembly.Instance(module, imports);
        } catch(err) {
            caught = true;
            if (err.toString().indexOf(errMsg) === -1)
                warn(`expected error message "${errMsg}", got "${err}"`);
        }
        assert(caught, "assert_unlinkable exception not caught");
        return;
    }

    if (!handleNonStandard(exprName, e)) {
        assert(false, "NYI: " + e);
    }
}

var args = typeof importedArgs !== 'undefined' ? importedArgs : scriptArgs;

// Whether we should keep on executing tests if one of them failed or throw.
var softFail = false;

// Debug function
var debug = function() {};
var debugImpl = print;

var warn = print;

// Count number of lines from start to `input` in `text.
var countLines = function() { return -1; }
var countLinesImpl = function(text, input) {
    return text.substring(0, input).split('\n').length;
}

// Specific tests to be run
var targets = [];
for (let arg of args) {
    switch (arg) {
      case '-c':
        countLines = countLinesImpl;
        break;
      case '-d':
        debug = debugImpl;
        break;
      case '-s':
        softFail = true;
        break;
      default:
        targets.push(arg);
        break;
    }
}

if (!args.length) {
    testNaNEqualityFunction();
}

top_loop:
for (var test of targets) {
    module = null;

    debug(`Running test ${test}...`);

    let source = read(scriptdir + test);

    let root = new parseSExpression(source);

    let success = true;
    for (let e of root.list) {
        try {
            exec(e);
        } catch(err) {
            success = false;
            debug(`Error in ${test}:${e.lineno}: ${err.stack ? err.stack : ''}\n${err}`);
            if (!softFail) {
                throw err;
            }
        }
    }

    if (success)
        debug(`\n${test} PASSED`);
    else
        debug(`\n${test} FAILED`);
}
