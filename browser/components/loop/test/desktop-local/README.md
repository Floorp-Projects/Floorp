== Mocha unit tests ==

These unit tests use the browser build of the [Mocha test framework][1]
and the Chai Assertion Library's [BDD interface][2].

[1]: http://visionmedia.github.io/mocha/
[2]: http://chaijs.com/api/bdd/

Aim your browser at the index.html in this directory on your localhost using
a file: or HTTP URL to run the tests.  Alternately, from the top-level of your
Gecko source directory, execute:

```
./mach marionette-test browser/components/loop/test/manifest.ini
```

Next steps:

* run using JS http server so the property security context for DOM elements
is used

