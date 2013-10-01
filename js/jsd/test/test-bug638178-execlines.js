
var jsdIScript = SpecialPowers.Ci.jsdIScript;

function f1() {
    var x;
}

function f2() {


    var x; var y; x = 1;
}

function f3() {


    var x;

    var y; var y2; y = 1;
    var z;

}

var jsdIFilter = SpecialPowers.Ci.jsdIFilter;

function testJSD(jsd) {
    ok(jsd.isOn, "JSD needs to be running for this test.");

    jsd.functionHook = ({
        onCall: function(frame, type) {
            //console.log("Got " + type);
            console.log("Got " + frame.script.fileName);
        }
    });

    console.log("Triggering functions");
    f1();
    f2();
    f3();
    console.log("Done with functions");

    var linemap = {};
    var firsts = {};
    var rests = {};
    var startlines = {};
    jsd.enumerateScripts({
        enumerateScript: function(script) {
            if (/execlines\.js$/.test(script.fileName)) {
                console.log("script: " + script.fileName + " " + script.functionName);
                var execLines = script.getExecutableLines(jsdIScript.PCMAP_SOURCETEXT, 0, 10000);
                console.log(execLines.toSource());
                linemap[script.functionName] = execLines;
                startlines[script.functionName] = script.baseLineNumber;

                execLines = script.getExecutableLines(jsdIScript.PCMAP_SOURCETEXT, 0, 1);
                firsts[script.functionName] = execLines;
                execLines = script.getExecutableLines(jsdIScript.PCMAP_SOURCETEXT, execLines[0]+1, 10000);
                rests[script.functionName] = execLines;
            }
        }
    });

    var checklines = function (funcname, linemap, rellines) {
        var base = startlines[funcname];
        var b = [];
        for (var i = 0; i < rellines.length; ++i) {
            b[i] = rellines[i] + base;
        }
        is(linemap[funcname].toSource(), b.toSource(), funcname + " lines");
    };

    checklines('f1', linemap, [ 1 ]);
    checklines('f2', linemap, [ 3 ]);
    checklines('f3', linemap, [ 3, 5, 6 ]);

    checklines('f1', firsts, [ 1 ]);
    checklines('f1', rests, []);
    checklines('f3', firsts, [ 3 ]);
    checklines('f3', rests, [ 5, 6 ]);

    jsd.functionHook = null;

    if (!jsdOnAtStart) {
        // turn JSD off if it wasn't on when this test started
        jsd.off();
        ok(!jsd.isOn, "JSD shouldn't be running at the end of this test.");
    }

    SimpleTest.finish();
}

testJSD(jsd);
