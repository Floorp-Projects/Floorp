# diffpatcher

[![Build Status](https://secure.travis-ci.org/Gozala/diffpatcher.png)](http://travis-ci.org/Gozala/diffpatcher)

[![Browser support](https://ci.testling.com/Gozala/diffpatcher.png)](http://ci.testling.com/Gozala/diffpatcher)

Diffpatcher is a small library that lets you treat hashes as if they were
git repositories.

## diff

Diff function that takes two hashes and returns delta hash.

```js
var diff = require("diffpatcher/diff")

diff({ a: { b: 1 }, c: { d: 2 } },      // hash#1
     { a: { e: 3 }, c: { d: 4 } })      // hash#2

// => {                                 // delta
//      a: {
//        b: null,        // -
//        e: 3            // +
//      },
//      c: {
//        d: 4            // ±
//      }
//    }
```

As you can see from the example above `delta` makes no real distinction between
proprety upadate and property addition. Try to think of additions as an update
from `undefined` to whatever it's being updated to.

## patch

Patch fuction takes a `hash` and a `delta` and returns a new `hash` which is
just like orginial but with delta applied to it. Let's apply delta from the
previous example to the first hash from the same example


```js
var patch = require("diffpatcher/patch")

patch({ a: { b: 1 }, c: { d: 2 } },     // hash#1
      {                                 // delta
        a: {
          b: null,        // -
          e: 3            // +
        },
        c: {
          d: 4            // ±
        }
      })

// => { a: { e: 3 }, c: { d: 4 } }      // hash#2
```

That's about it really, just diffing hashes and applying thes diffs on them.


### rebase

And as Linus mentioned everything in git can be expressed with `rebase`, that
also pretty much the case for `diffpatcher`. `rebase` takes `target` hash,
and rebases `parent` onto it with `diff` applied.

## Install

    npm install diffpatcher
