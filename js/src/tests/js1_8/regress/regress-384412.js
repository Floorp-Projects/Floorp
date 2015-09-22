// |reftest| skip-if(!xulRuntime.shell)
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 384412;
var summary = 'Exercise frame handling code';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
enableNoSuchMethod();
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
/*
 * Generators
 */

/* Generator yields properly */
  f = (function(n) { for (var i = 0; i != n; i++) yield i });
  g = f(3);
  expect(0, g.next());
  expect(1, g.next());
  expect(2, g.next());
  s = "no exception";
  try { g.next(); } catch (e) { s = e + ""; }
  expect("[object StopIteration]", s);

/* Generator yields properly in finally */
  f = (function(n) {
      try {
        for (var i = 0; i != n; i++) 
          yield i;
      } finally {
        yield "finally";
      }
    });

  g = f(3);
  expect(0, g.next());
  expect(1, g.next());
  expect(2, g.next());
  expect("finally", g.next());

/* Generator throws when closed with yield in finally */
  g = f(3);
  expect(0, g.next());
  s = "no exception";
  try { g.close(); } catch (e) { s = e + ""; };
  expect("TypeError: yield from closing generator " + f.toSource(), s);


/*
 * Calls that have been replaced with js_PushFrame() &c...
 */
  f = (function() { return arguments[(arguments.length - 1) / 2]; });
  expect(2, f(1, 2, 3));
  expect(2, f.call(null, 1, 2, 3));
  expect(2, f.apply(null, [1, 2, 3]));
  expect("a1c", "abc".replace("b", f));
  s = "no exception";
  try {
    "abc".replace("b", (function() { throw "hello" }));
  } catch (e) {
    s = e + "";
  }
  expect("hello", s);
  expect(6, [1, 2, 3].reduce(function(a, b) { return a + b; }));
  s = "no exception";
  try {
    [1, 2, 3].reduce(function(a, b) { if (b == 2) throw "hello"; });
  } catch (e) {
    s = e + "";
  }
  expect("hello", s);

/*
 * __noSuchMethod__
 */
  o = {};
  s = "no exception";
  try {
    o.hello();
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: o.hello is not a function", s);
  o.__noSuchMethod__ = (function() { return "world"; });
  expect("world", o.hello());
  o.__noSuchMethod__ = 1;
  s = "no exception";
  try {
    o.hello();
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: o.hello is not a function", s);
  o.__noSuchMethod__ = {};
  s = "no exception";
  try {
    o.hello();
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: ({}) is not a function", s);
  s = "no exception";
  try {
    eval("o.hello()");
  } catch (e) {
    s = e + "";
  }
  expect("TypeError: ({}) is not a function", s);
  s = "no exception";
  try { [2, 3, 0].sort({}); } catch (e) { s = e + ""; }
  expect("TypeError: ({}) is not a function", s);

/*
 * Generator expressions.
 */
  String.prototype.__iterator__ = (function () {
      /*
       * NOTE:
       * Without the "0 + ", the loop over <x/> does not terminate because
       * the iterator gets run on a string with an empty length property.
       */
      for (let i = 0; i != 0 + this.length; i++)
        yield this[i];
    });
  expect(["a1", "a2", "a3", "b1", "b2", "b3", "c1", "c2", "c3"] + "",
         ([a + b for (a in 'abc') for (b in '123')]) + "");

  print("End of Tests");

/*
 * Utility functions
 */
  function expect(a, b) {
    print('expect: ' + a + ', actual: ' + b);
    reportCompare(a, b, summary);
  }


  exitFunc ('test');
}
