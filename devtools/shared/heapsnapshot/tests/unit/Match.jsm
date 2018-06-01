// A little pattern-matching library.
//
// Ported from js/src/tests/js1_8_5/reflect-parse/Match.js for use with devtools
// server xpcshell tests.

"use strict";

this.EXPORTED_SYMBOLS = ["Match"];

this.Match = (function() {
  function Pattern(template) {
      // act like a constructor even as a function
    if (!(this instanceof Pattern)) {
      return new Pattern(template);
    }

    this.template = template;
  }

  Pattern.prototype = {
    match: function(act) {
      return match(act, this.template);
    },

    matches: function(act) {
      try {
        return this.match(act);
      } catch (e) {
        if (e instanceof MatchError) {
          return false;
        }
      }
      return false;
    },

    assert: function(act, message) {
      try {
        return this.match(act);
      } catch (e) {
        if (e instanceof MatchError) {
          throw new Error((message || "failed match") + ": " + e.message);
        }
      }
      return false;
    },

    toString: () => "[object Pattern]"
  };

  Pattern.ANY = new Pattern();
  Pattern.ANY.template = Pattern.ANY;

  Pattern.NUMBER = new Pattern();
  Pattern.NUMBER.match = function(act) {
    if (typeof act !== "number") {
      throw new MatchError("Expected number, got: " + quote(act));
    }
  };

  Pattern.NATURAL = new Pattern();
  Pattern.NATURAL.match = function(act) {
    if (typeof act !== "number" || act !== Math.floor(act) || act < 0) {
      throw new MatchError("Expected natural number, got: " + quote(act));
    }
  };

  const quote = uneval;

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

  function isFunction(x) {
    return typeof x === "function";
  }

  function isArrayLike(x) {
    return isObject(x) && ("length" in x);
  }

  function matchAtom(act, exp) {
    if ((typeof exp) === "number" && isNaN(exp)) {
      if ((typeof act) !== "number" || !isNaN(act)) {
        throw new MatchError("expected NaN, got: " + quote(act));
      }
      return true;
    }

    if (exp === null) {
      if (act !== null) {
        throw new MatchError("expected null, got: " + quote(act));
      }
      return true;
    }

    if (exp instanceof RegExp) {
      if (!(act instanceof RegExp) || exp.source !== act.source) {
        throw new MatchError("expected " + quote(exp) + ", got: " + quote(act));
      }
      return true;
    }

    switch (typeof exp) {
      case "string":
        if (act !== exp) {
          throw new MatchError("expected " + quote(exp) + ", got " + quote(act));
        }
        return true;
      case "boolean":
      case "number":
        if (exp !== act) {
          throw new MatchError("expected " + exp + ", got " + quote(act));
        }
        return true;
    }

    throw new Error("bad pattern: " + exp.toSource());
  }

  function matchObject(act, exp) {
    if (!isObject(act)) {
      throw new MatchError("expected object, got " + quote(act));
    }

    for (const key in exp) {
      if (!(key in act)) {
        throw new MatchError("expected property " + quote(key)
          + " not found in " + quote(act));
      }
      match(act[key], exp[key]);
    }

    return true;
  }

  function matchFunction(act, exp) {
    if (!isFunction(act)) {
      throw new MatchError("expected function, got " + quote(act));
    }

    if (act !== exp) {
      throw new MatchError("expected function: " + exp +
                             "\nbut got different function: " + act);
    }
  }

  function matchArray(act, exp) {
    if (!isObject(act) || !("length" in act)) {
      throw new MatchError("expected array-like object, got " + quote(act));
    }

    const length = exp.length;
    if (act.length !== exp.length) {
      throw new MatchError("expected array-like object of length "
        + length + ", got " + quote(act));
    }

    for (let i = 0; i < length; i++) {
      if (i in exp) {
        if (!(i in act)) {
          throw new MatchError("expected array property " + i + " not found in "
            + quote(act));
        }
        match(act[i], exp[i]);
      }
    }

    return true;
  }

  function match(act, exp) {
    if (exp === Pattern.ANY) {
      return true;
    }

    if (exp instanceof Pattern) {
      return exp.match(act);
    }

    if (isAtom(exp)) {
      return matchAtom(act, exp);
    }

    if (isArrayLike(exp)) {
      return matchArray(act, exp);
    }

    if (isFunction(exp)) {
      return matchFunction(act, exp);
    }

    return matchObject(act, exp);
  }

  return { Pattern, MatchError };
})();
