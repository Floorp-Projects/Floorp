let expected = 'o!o!o!';
let actual = '';

// g is a function that needs an implicit |this| if called within a |with|
// statement.  If we fail to provide that implicit |this|, it will append
// "[object global]" instead of "o!".
let o = {
  g: function() { actual += this.toString(); },
  toString: function() { return "o!"; }
}

// g's presence within the |with| is detected by simple tracking of |with|s
// during parsing.
with (o) {
  (function() { g(); })();
}

// The eval() defeats the tracking of |with| during parsing.  Instead, g's
// presence within the |with| is detected by looking at the scopeChain of the
// ParseContext.
with (o) {
  eval("(function() { g(); })()");
}

// This is like the above case, but the knowledge of the |with| presence must
// be inherited by the inner function.  This is the case that was missed in bug
// 786114.
with (o) {
  eval("(function() { (function() { g(); })(); })()");
}

assertEq(actual, expected);
