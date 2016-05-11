load(libdir + "wasm.js");
load(scriptdir + "spec/list.js");

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

var module;

setJitCompilerOption('wasm.test-mode', 1);

// Creates a tree of s-expressions. Ported from Binaryen's SExpressionParser.
function parseSExpression(text) {
    var input = 0;

    var commentDepth = 0;
    function skipBlockComment() {
        while (true) {
            if (text[input] === '(' && text[input + 1] === ';') {
                input += 2;
                commentDepth++;
            } else if (text[input] === ';' && text[input + 1] === ')') {
                input += 2;
                commentDepth--;
                if (!commentDepth) {
                    return;
                }
            } else {
                input++;
            }
        }
    }

    function parseInnerList() {
        if (text[input] === ';') {
            // Parse comment.
            input++;
            if (text[input] === ';') {
                while (text[input] != '\n') input++;
                return null;
            }
            assert(false, 'malformed comment');
        }

        if (text[input] === '(' && text[input + 1] === ';') {
            skipBlockComment();
            return null;
        }

        var start = input;
        var ret = new Element();
        while (true) {
            var curr = parse();
            if (!curr) {
                ret.lineno = countLines(text, input);
                return ret;
            }
            ret.list.push(curr);
        }
    }

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

    function skipWhitespace() {
        while (true) {
            while (isSpace(text[input]))
                input++;

            if (text[input] === ';' && text[input + 1] === ';') {
                while (text.length > input && text[input] != '\n') input++;
            } else if (text[input] === '(' && text[input + 1] === ';') {
                skipBlockComment();
            } else {
                return;
            }
        }
    }

    function parseString() {
        var dollared = false;
        var quoted = false;
        if (text[input] === '$') {
            input++;
            dollared = true;
        }

        var start = input;
        if (text[input] === '"') {
            quoted = true;
            // Parse escaping \", but leave code escaped - we'll handle escaping in memory segments specifically.
            input++;
            var str = "";
            while (true) {
                if (text[input] === '"') break;
                if (text[input] === '\\') {
                    str += text[input];
                    str += text[input + 1];
                    input += 2;
                    continue;
                }
                str += text[input];
                input++;
            }
            input++;
            return new Element(str, dollared, quoted);
        }

        while (text.length > input &&
               !isSpace(text[input]) &&
               text[input] != ')' &&
               text[input] != '(') {
            input++;
        }

        return new Element(text.substring(start, input), dollared);
    }

    function parse() {
        skipWhitespace();

        if (text.length === input || text[input] === ')')
            return null;

        if (text[input] === '(') {
            input++;
            var ret = parseInnerList();
            skipWhitespace();
            assert(text[input] === ')', 'inner list ends with a )');
            input++;
            return ret;
        }

        return parseString();
    }

    var root = null;
    while (!root) { // Keep parsing until we pass an initial comment.
        root = parseInnerList();
    }
    return root;
}

var imports = {
    spectest: {
        print: function (x) {
            print(x);
        }
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

// Recursively execute the expression.
function exec(e) {
    var exprName = e.list[0].str;

    if (exprName === "module") {
        let moduleText = e.toString();
        module = wasmEvalText(moduleText, imports);
        return;
    }

    if (exprName === "invoke") {
        var name = e.list[1].str;
        var args = e.list.slice(2).map(exec);
        var fn = null;

        if (module === null) {
            debug('We should have a module here before trying to invoke things!');
            quit();
        }

        if (typeof module[name] === "function") {
            fn = module[name];
        } else if (name === "") {
            fn = module;
            assert(typeof fn === "function", "Default exported function not found: " + e);
        } else {
            assert(false, "Exported function not found: " + e);
        }
        return fn.apply(null, args);
    }

    if (exprName.indexOf(".const") > 0) {
        // Eval the expression using a wasm module.
        var type = exprName.substring(0, exprName.indexOf(".const"));
        return wasmEvalText('(module (func (result ' + type + ') ' + e + ') (export "" 0))')()
    }

    if (exprName === "assert_return") {
        let lhs = exec(e.list[1]);
        // There might be a value to test against.
        if (e.list[2]) {
            let rhs = exec(e.list[2]);
            if (typeof lhs === 'number') {
                assertEq(lhs, rhs);
            } else {
                // Int64 are emulated with objects with shape:
                // {low: Number, high: Number}
                assert(typeof lhs.low === 'number', 'assert_return expects int64 or number');
                assert(typeof lhs.high === 'number', 'assert_return expects int64 or number');
                assertEq(lhs.low, rhs.low);
                assertEq(lhs.high, rhs.high);
            }
        }
        return;
    }

    if (exprName === "assert_return_nan") {
        assertEq(exec(e.list[1]), NaN);
        return;
    }

    if (exprName === "assert_invalid") {
        let moduleText = e.list[1].toString();
        let errMsg = e.list[2];
        if (errMsg) {
            assert(errMsg.quoted, "assert_invalid second argument must be a string");
            errMsg.quoted = false;
        }
        let caught = false;
        try {
            wasmEvalText(moduleText, imports);
        } catch(e) {
            if (errMsg && e.toString().indexOf(errMsg) === -1)
                warn(`expected error message "${errMsg}", got "${e}"`);
            caught = true;
        }
        assert(caught, "assert_invalid error");
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
            assert(err.toString().indexOf(errMsg) !== -1, `expected error message "${errMsg}", got "${err}"`);
        }
        assert(caught, "assert_trap exception not caught");
        return;
    }

    if(!handleNonStandard(exprName, e)) {
        assert(false, "NYI: " + e);
    }
}

var args = scriptArgs;

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

if (targets.length)
    specTests = targets;

top_loop:
for (var test of specTests) {
    module = null;

    debug(`Running test ${test}...`);

    let source = read(scriptdir + "spec/" + test);

    let root = new parseSExpression(source);

    let success = true;
    for (let e of root.list) {
        try {
            exec(e);
        } catch(err) {
            if (err && err.message && err.message.indexOf("i64 NYI") !== -1) {
                assert(!hasI64(), 'i64 NYI should happen only on platforms without i64');
                warn(`Skipping test file ${test} as it contains int64, NYI.\n`)
                continue top_loop;
            }
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
