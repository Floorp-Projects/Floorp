// Currently the Jit integration has a few issues, let's keep this test
// case deterministic.
//
//  - Baseline OSR increments the loop header twice.
//  - Ion is not updating any counter yet.
//
if (getJitCompilerOptions()["ion.warmup.trigger"] != 30)
  setJitCompilerOption("ion.warmup.trigger", 30);
if (getJitCompilerOptions()["baseline.warmup.trigger"] != 10)
  setJitCompilerOption("baseline.warmup.trigger", 10);

/*
 * These test cases are annotated with the output produced by LCOV [1].  Comment
 * starting with //<key> without any spaces are used as a reference for the code
 * coverage output.  Any "$" in these line comments are replaced by the current
 * line number, and any "%" are replaced with the current function name (defined
 * by the FN key).
 *
 * [1]  http://ltp.sourceforge.net/coverage/lcov/geninfo.1.php
 */
function checkGetOffsetsCoverage(fun) {
  var keys = [ "TN", "SF", "FN", "FNDA", "FNF", "FNH", "BRDA", "BRF", "BRH", "DA", "LF", "LH" ];
  function startsWithKey(s) {
    for (k of keys) {
      if (s.startsWith(k))
        return true;
    }
    return false;
  };

  // Extract the body of the function, as the code to be executed.
  var source = fun.toSource();
  source = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}'));

  // Extract comment starting with the previous keys, as a reference.
  var lcovRef = [];
  var currLine = 0;
  var currFun = [{name: "top-level", braces: 1}];
  for (var line of source.split('\n')) {
    currLine++;

    for (var comment of line.split("//").slice(1)) {
      if (!startsWithKey(comment))
        continue;
      comment = comment.trim();
      if (comment.startsWith("FN:"))
        currFun.push({ name: comment.split(',')[1], braces: 0 });
      var name = currFun[currFun.length - 1].name;
      if (!comment.startsWith("DA:"))
        continue;
      comment = {
        offset: null,
        lineNumber: currLine,
        columnNumber: null,
        count: comment.split(",")[1] | 0,
        script: (name == "top-level" ? undefined : name)
      };
      lcovRef.push(comment);
    }

    var deltaBraces = line.split('{').length - line.split('}').length;
    currFun[currFun.length - 1].braces += deltaBraces;
    if (currFun[currFun.length - 1].braces == 0)
      currFun.pop();
  }

  // Create a new global and instrument it with a debugger, to find all scripts,
  // created in the current global.
  var g = newGlobal();
  var dbg = Debugger(g);
  dbg.collectCoverageInfo = true;

  var topLevel = null;
  dbg.onNewScript = function (s) {
    topLevel = s;
    dbg.onNewScript = function () {};
  };

  // Evaluate the code, and collect the hit counts for each scripts / lines.
  g.eval(source);

  var coverageRes = [];
  function collectCoverage(s) {
    var res = s.getOffsetsCoverage();
    if (res == null)
      res = [{
        offset: null,
        lineNumber: null,
        columnNumber: null,
        script: s.displayName,
        count: 0
      }];
    else {
      res.map(function (e) {
        e.script = s.displayName;
        return e;
      });
    }
    coverageRes.push(res);
    s.getChildScripts().forEach(collectCoverage);
  };
  collectCoverage(topLevel);
  coverageRes = [].concat(...coverageRes);

  // Check that all the lines are present the result.
  function match(ref) {
    return function (entry) {
      return ref.lineNumber == entry.lineNumber && ref.script == entry.script;
    }
  }
  function ppObj(entry) {
    var str = "{";
    for (var k in entry) {
      if (entry[k] != null)
        str += " '" + k + "': " + entry[k] + ",";
    }
    str += "}";
    return str;
  }
  for (ref of lcovRef) {
    var res = coverageRes.find(match(ref));
    if (!res) {
      // getOffsetsCoverage returns null if we have no result for the
      // script. We added a fake entry with an undefined lineNumber, which is
      // used to match against the modified reference.
      var missRef = Object.create(ref);
      missRef.lineNumber = null;
      res = coverageRes.find(match(missRef));
    }

    if (!res || res.count != ref.count) {
      print("Cannot find `" + ppObj(ref) + "` in the following results:\n", coverageRes.map(ppObj).join("\n"));
      print("In the following source:\n", source);
      assertEq(true, false);
    }
  }
}

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  ",".split(','); //DA:$,1
  //FNF:1
  //FNH:1
  //LF:1
  //LH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  function f() {    //FN:$,f
    ",".split(','); //DA:$,0
  }
  ",".split(',');   //DA:$,1
  //FNF:2
  //FNH:1
  //LF:2
  //LH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  function f() {    //FN:$,f //FNDA:1,%
    ",".split(','); //DA:$,1
  }
  f();              //DA:$,1
  //FNF:2
  //FNH:2
  //LF:2
  //LH:2
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 3)      //DA:$,1
    l.push('');           //DA:$,0
  else
    l.pop();              //DA:$,1
  //FNF:1
  //FNH:1
  //LF:4
  //LH:3
  //BRF:1
  //BRH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 2)      //DA:$,1
    l.push('');           //DA:$,1
  else
    l.pop();              //DA:$,0
  //FNF:1
  //FNH:1
  //LF:4
  //LH:3
  //BRF:1
  //BRH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 2)      //DA:$,1
    l.push('');           //DA:$,1
  else {
    if (l.length == 1)    //DA:$,0
      l.pop();            //DA:$,0
  }
  //FNF:1
  //FNH:1
  //LF:5
  //LH:3
  //BRF:2
  //BRH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  function f(i) { //FN:$,f //FNDA:2,%
    var x = 0;    //DA:$,2
    while (i--) { // Currently OSR wrongly count the loop header twice.
                  // So instead of DA:$,12 , we have DA:$,13 .
      x += i;     //DA:$,10
      x = x / 2;  //DA:$,10
    }
    return x;     //DA:$,2
    //BRF:1
    //BRH:1
  }

  f(5);           //DA:$,1
  f(5);           //DA:$,1
  //FNF:2
  //FNH:2
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  try {                     //DA:$,1
    var l = ",".split(','); //DA:$,1
    if (l.length == 2) {    //DA:$,1 // BRDA:$,0
      l.push('');           //DA:$,1
      throw l;              //DA:$,1
    }
    l.pop();                //DA:$,0
  } catch (x) {             //DA:$,1
    x.pop();                //DA:$,1
  }
  //FNF:1
  //FNH:1
  //LF:9 // Expected LF:8 , Apparently if the first statement is a try, the
         // statement following the "try{" statement is visited twice.
  //LH:8 // Expected LH:7
  //BRF:1
  //BRH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(',');   //DA:$,1
  try {                     //DA:$,1
    try {                   //DA:$,1
      if (l.length == 2) {  //DA:$,1 // BRDA:$,0
        l.push('');         //DA:$,1
        throw l;            //DA:$,1
      }
      l.pop();              //DA:$,0 // BRDA:$,-
    } finally {             //DA:$,1
      l.pop();              //DA:$,1
    }
  } catch (x) {             //DA:$,1
  }
  //FNF:1
  //FNH:1
  //LF:10
  //LH:9
  //BRF:2
  //BRH:1
});

checkGetOffsetsCoverage(function () { //FN:$,top-level //FNDA:1,%
  function f() {            //FN:$,f //FNDA:1,%
    throw 1;                //DA:$,1
    f();                    //DA:$,0
  }
  var l = ",".split(',');   //DA:$,1
  try {                     //DA:$,1
    f();                    //DA:$,1
    f();                    //DA:$,0
  } catch (x) {             //DA:$,1
  }
  //FNF:2
  //FNH:2
  //LF:7
  //LH:5
  //BRF:0
  //BRH:0
});

// If you add a test case here, do the same in
// jit-test/tests/coverage/simple.js
