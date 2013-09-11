// Debugger.prototype.findScripts can filter by global, url, and line number.

// Two scripts, with different functions at the same line numbers.
var url1 = scriptdir + 'Debugger-findScripts-12-script1';
var url2 = scriptdir + 'Debugger-findScripts-12-script2';

// Three globals: two with code, one with nothing.
var g1 = newGlobal();
g1.toSource = function () "[global g1]";
g1.load(url1);
g1.load(url2);
var g2 = newGlobal();
g2.toSource = function () "[global g2]";
g2.load(url1);
g2.load(url2);
var g3 = newGlobal();

var dbg = new Debugger(g1, g2, g3);

function script(func) {
    var gw = dbg.addDebuggee(func.global);
    var script = gw.makeDebuggeeValue(func).script;
    script.toString = function ()
        "[Debugger.Script for " + func.name + " in " + uneval(func.global) + "]";
    return script;
}

// The function scripts we know of. There may be random eval scripts involved, but
// we don't care about those.
var allScripts = ([g1.f, g1.f(), g1.g, g1.h, g1.h(), g1.i,
                   g2.f, g2.f(), g2.g, g2.h, g2.h(), g2.i].map(script));

// Search for scripts using |query|, expecting no members of allScripts
// except those given in |expected| in the result. If |expected| is
// omitted, expect no members of allScripts at all.
function queryExpectOnly(query, expected) {
    print();
    print("queryExpectOnly(" + uneval(query) + ")");
    var scripts = dbg.findScripts(query);
    var present = allScripts.filter(function (s) { return scripts.indexOf(s) != -1; });
    if (expected) {
        expected = expected.map(script);
        expected.forEach(function (s) {
                             if (present.indexOf(s) == -1)
                                 assertEq(s + " not present", "is present");
                         });
        present.forEach(function (s) {
                             if (expected.indexOf(s) == -1)
                                 assertEq(s + " is present", "not present");
                         });
    } else {
        assertEq(present.length, 0);
    }
}

// We have twelve functions: two globals, each with two urls, each
// defining three functions. Show that all the different combinations of
// query parameters select what they should.

// There are gaps in the pattern:
// - You can only filter by line if you're also filtering by url.
// - You can't ask for only the innermost scripts unless you're filtering by line.

// Filtering by global, url, and line produces one function, or two
// where they are nested.
queryExpectOnly({ global:g1, url:url1, line:  6 }, [g1.f        ]);
queryExpectOnly({ global:g1, url:url1, line:  8 }, [g1.f, g1.f()]);
queryExpectOnly({ global:g1, url:url1, line: 15 }, [g1.g        ]);
queryExpectOnly({ global:g1, url:url2, line:  6 }, [g1.h        ]);
queryExpectOnly({ global:g1, url:url2, line:  8 }, [g1.h, g1.h()]);
queryExpectOnly({ global:g1, url:url2, line: 15 }, [g1.i        ]);
queryExpectOnly({ global:g2, url:url1, line:  6 }, [g2.f        ]);
queryExpectOnly({ global:g2, url:url1, line:  8 }, [g2.f, g2.f()]);
queryExpectOnly({ global:g2, url:url1, line: 15 }, [g2.g        ]);
queryExpectOnly({ global:g2, url:url2, line:  6 }, [g2.h        ]);
queryExpectOnly({ global:g2, url:url2, line:  8 }, [g2.h, g2.h()]);
queryExpectOnly({ global:g2, url:url2, line: 15 }, [g2.i        ]); 

// Filtering by global, url, and line, and requesting only the innermost
// function at each point, should produce only one function.
queryExpectOnly({ global:g1, url:url1, line:  6, innermost: true }, [g1.f  ]);
queryExpectOnly({ global:g1, url:url1, line:  8, innermost: true }, [g1.f()]);
queryExpectOnly({ global:g1, url:url1, line: 15, innermost: true }, [g1.g  ]);
queryExpectOnly({ global:g1, url:url2, line:  6, innermost: true }, [g1.h  ]);
queryExpectOnly({ global:g1, url:url2, line:  8, innermost: true }, [g1.h()]);
queryExpectOnly({ global:g1, url:url2, line: 15, innermost: true }, [g1.i  ]);
queryExpectOnly({ global:g2, url:url1, line:  6, innermost: true }, [g2.f  ]);
queryExpectOnly({ global:g2, url:url1, line:  8, innermost: true }, [g2.f()]);
queryExpectOnly({ global:g2, url:url1, line: 15, innermost: true }, [g2.g  ]);
queryExpectOnly({ global:g2, url:url2, line:  6, innermost: true }, [g2.h  ]);
queryExpectOnly({ global:g2, url:url2, line:  8, innermost: true }, [g2.h()]);
queryExpectOnly({ global:g2, url:url2, line: 15, innermost: true }, [g2.i  ]); 

// Filtering by url and global should produce sets of three scripts.
queryExpectOnly({ global:g1, url:url1 }, [g1.f, g1.f(), g1.g]);
queryExpectOnly({ global:g1, url:url2 }, [g1.h, g1.h(), g1.i]);
queryExpectOnly({ global:g2, url:url1 }, [g2.f, g2.f(), g2.g]);
queryExpectOnly({ global:g2, url:url2 }, [g2.h, g2.h(), g2.i]);

// Filtering by url and line, innermost-only, should produce sets of two scripts,
// or four where there are nested functions.
queryExpectOnly({ url:url1, line: 6 }, [g1.f,         g2.f        ]);
queryExpectOnly({ url:url1, line: 8 }, [g1.f, g1.f(), g2.f, g2.f()]);
queryExpectOnly({ url:url1, line:15 }, [g1.g,         g2.g        ]);
queryExpectOnly({ url:url2, line: 6 }, [g1.h,         g2.h        ]);
queryExpectOnly({ url:url2, line: 8 }, [g1.h, g1.h(), g2.h, g2.h()]);
queryExpectOnly({ url:url2, line:15 }, [g1.i,         g2.i        ]);

// Filtering by url and line, and requesting only the innermost scripts,
// should always produce pairs of scripts.
queryExpectOnly({ url:url1, line: 6, innermost: true }, [g1.f,   g2.f  ]);
queryExpectOnly({ url:url1, line: 8, innermost: true }, [g1.f(), g2.f()]);
queryExpectOnly({ url:url1, line:15, innermost: true }, [g1.g,   g2.g  ]);
queryExpectOnly({ url:url2, line: 6, innermost: true }, [g1.h,   g2.h  ]);
queryExpectOnly({ url:url2, line: 8, innermost: true }, [g1.h(), g2.h()]);
queryExpectOnly({ url:url2, line:15, innermost: true }, [g1.i,   g2.i  ]);

// Filtering by global only should produce sets of six scripts.
queryExpectOnly({ global:g1 }, [g1.f, g1.f(), g1.g, g1.h, g1.h(), g1.i]);
queryExpectOnly({ global:g2 }, [g2.f, g2.f(), g2.g, g2.h, g2.h(), g2.i]);

// Filtering by url should produce sets of six scripts.
queryExpectOnly({ url:url1 }, [g1.f, g1.f(), g1.g, g2.f, g2.f(), g2.g]);
queryExpectOnly({ url:url2 }, [g1.h, g1.h(), g1.i, g2.h, g2.h(), g2.i]);

// Filtering by no axes should produce all twelve scripts.
queryExpectOnly({}, [g1.f, g1.f(), g1.g, g1.h, g1.h(), g1.i,
                     g2.f, g2.f(), g2.g, g2.h, g2.h(), g2.i]);
