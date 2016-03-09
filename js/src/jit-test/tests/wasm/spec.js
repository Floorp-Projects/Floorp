load(libdir + "wasm.js");
load(scriptdir + "spec/list.js");

if (typeof assert === 'undefined') {
    var assert = function assert(c, msg) {
        if (!c) {
            throw new Error("Assertion failed: " + msg);
        }
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
        } else if (this.quoted) {
            return `"${this.str}"`;
        }
        return this.str;
    }
    return `(${this.list.map(x => x.toString()).join(" ")})`;
};

// Creates a tree of s-expressions. Ported from Binaryen's SExpressionParser.
function parseSExpression(text) {
    var input = 0;
    function parseInnerList() {
        if (text[input] === ';') {
            // Parse comment.
            input++;
            if (text[input] === ';') {
                while (text[input] != '\n') input++;
                return null;
            }
            input = text.substring(";)", input);
            assert(input >= 0);
            return null;
        }
        var ret = new Element();
        while (true) {
            var curr = parse();
            if (!curr) return ret;
            ret.list.push(curr);
        }
    }
    function isSpace(c) {
        switch (c) {
            case ' ':
            case '\n':
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
            while (isSpace(text[input])) input++;
            if (text[input] === ';' && text[input + 1] === ';') {
                while (text.length > input && text[input] != '\n') input++;
            } else if (text[input] === '(' && text[input + 1] === ';') {
                input = text.substring(";)", input) + 2;
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
        while (text.length > input && !isSpace(text[input]) && text[input] != ')' && text[input] != '(') input++;
        return new Element(text.substring(start, input), dollared);
    }
    function parse() {
        skipWhitespace();
        if (text.length === input || text[input] === ')') return null;
        if (text[input] === '(') {
            input++;
            var ret = parseInnerList();
            skipWhitespace();
            assert(text[input] === ')');
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
var module;
var moduleText;

// Recursively execute the expression.
function exec(e) {
    var exprName = e.list[0].str;
    if (exprName === "module") {
        moduleText = e.toString();
        try {
            module = wasmEvalText(moduleText, imports);
        } catch (x) {
            assert(false, x.toString());
        }
    } else if (exprName === "invoke") {
        var name = e.list[1].str;
        var args = e.list.slice(2).map(exec);
        var fn = null;
        assert(module !== null);
        if (typeof module[name] === "function") {
            fn = module[name];
        } else if (name === "") {
            fn = module;
            assert(typeof fn === "function", "Default exported function not found: " + e);
        } else {
            assert(false, "Exported function not found: " + e);
        }
        return fn.apply(null, args);
    } else if (exprName.indexOf(".const") > 0) {
        // Eval the expression using a wasm module.
        var type = exprName.substring(0, exprName.indexOf(".const"));
        return wasmEvalText('(module (func (result ' + type + ') ' + e + ') (export "" 0))')()
    } else if (exprName === "assert_return") {
        assertEq(exec(e.list[1]), exec(e.list[2]));
    } else if (exprName === "assert_return_nan") {
        assertEq(exec(e.list[1]), NaN);
    } else {
        assert(false, "NYI: " + e);
    }
}

for (var test of specTests) {
    module = null;
    moduleText = null;
    var root = new parseSExpression(read(scriptdir + "spec/" + test));
    for (var e of root.list) {
        exec(e);
    }
}