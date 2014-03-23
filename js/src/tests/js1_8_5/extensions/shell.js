/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


// The Worker constructor can take a relative URL, but different test runners
// run in different enough environments that it doesn't all just automatically
// work. For the shell, we use just a filename; for the browser, see browser.js.
var workerDir = '';

// explicitly turn on js185
// XXX: The browser currently only supports up to version 1.8
if (typeof version != 'undefined')
{
  version(185);
}

// A little pattern-matching library.
var Match =

(function() {

    function Pattern(template) {
        // act like a constructor even as a function
        if (!(this instanceof Pattern))
            return new Pattern(template);

        this.template = template;
    }

    Pattern.prototype = {
        match: function(act) {
            return match(act, this.template);
        },

        matches: function(act) {
            try {
                return this.match(act);
            }
            catch (e if e instanceof MatchError) {
                return false;
            }
        },

        assert: function(act, message) {
            try {
                return this.match(act);
            }
            catch (e if e instanceof MatchError) {
                throw new Error((message || "failed match") + ": " + e.message);
            }
        },

        toString: function() "[object Pattern]"
    };

    Pattern.ANY = new Pattern;
    Pattern.ANY.template = Pattern.ANY;

    var quote = uneval;

    function MatchError(msg) {
        this.message = msg;
    }

    MatchError.prototype = {
        toString: function() {
            return "match error: " + this.message;
        }
    };

    function isAtom(x) {
        return (typeof x === "number") ||
            (typeof x === "string") ||
            (typeof x === "boolean") ||
            (x === null) ||
            (typeof x === "object" && x instanceof RegExp);
    }

    function isObject(x) {
        return (x !== null) && (typeof x === "object");
    }

    function isArrayLike(x) {
        return isObject(x) && ("length" in x);
    }

    function matchAtom(act, exp) {
        if ((typeof exp) === "number" && isNaN(exp)) {
            if ((typeof act) !== "number" || !isNaN(act))
                throw new MatchError("expected NaN, got: " + quote(act));
            return true;
        }

        if (exp === null) {
            if (act !== null)
                throw new MatchError("expected null, got: " + quote(act));
            return true;
        }

        if (exp instanceof RegExp) {
            if (!(act instanceof RegExp) || exp.source !== act.source)
                throw new MatchError("expected " + quote(exp) + ", got: " + quote(act));
            return true;
        }

        switch (typeof exp) {
        case "string":
            if (act !== exp)
                throw new MatchError("expected " + exp.quote() + ", got " + quote(act));
            return true;
        case "boolean":
        case "number":
            if (exp !== act)
                throw new MatchError("expected " + exp + ", got " + quote(act));
            return true;
        }

        throw new Error("bad pattern: " + exp.toSource());
    }

    function matchObject(act, exp) {
        if (!isObject(act))
            throw new MatchError("expected object, got " + quote(act));

        for (var key in exp) {
            if (!(key in act))
                throw new MatchError("expected property " + key.quote() + " not found in " + quote(act));
            match(act[key], exp[key]);
        }

        return true;
    }

    function matchArray(act, exp) {
        if (!isObject(act) || !("length" in act))
            throw new MatchError("expected array-like object, got " + quote(act));

        var length = exp.length;
        if (act.length !== exp.length)
            throw new MatchError("expected array-like object of length " + length + ", got " + quote(act));

        for (var i = 0; i < length; i++) {
            if (i in exp) {
                if (!(i in act))
                    throw new MatchError("expected array property " + i + " not found in " + quote(act));
                match(act[i], exp[i]);
            }
        }

        return true;
    }

    function match(act, exp) {
        if (exp === Pattern.ANY)
            return true;

        if (exp instanceof Pattern)
            return exp.match(act);

        if (isAtom(exp))
            return matchAtom(act, exp);

        if (isArrayLike(exp))
            return matchArray(act, exp);

        return matchObject(act, exp);
    }

    return { Pattern: Pattern,
             MatchError: MatchError };

})();

function referencesVia(from, edge, to) {
    edge = "edge: " + edge;
    var edges = findReferences(to);
    if (edge in edges && edges[edge].indexOf(from) != -1)
        return true;

    // Be nice: make it easy to fix if the edge name has just changed.
    var alternatives = [];
    for (var e in edges) {
        if (edges[e].indexOf(from) != -1)
            alternatives.push(e);
    }
    if (alternatives.length == 0) {
        print("referent not referred to by referrer after all");
    } else {
        print("referent is not referenced via: " + uneval(edge));
        print("but it is referenced via:       " + uneval(alternatives));
    }
    print("all incoming edges, from any object:");
    for (var e in edges)
        print(e);
    return false;
}

// Note that AsmJS ArrayBuffers have a minimum size, currently 4096 bytes. If a
// smaller size is given, a regular ArrayBuffer will be returned instead.
function AsmJSArrayBuffer(size) {
    var ab = new ArrayBuffer(size);
    (new Function('global', 'foreign', 'buffer', '' +
'        "use asm";' +
'        var i32 = new global.Int32Array(buffer);' +
'        function g() {};' +
'        return g;' +
''))(Function("return this")(),null,ab);
    return ab;
}
