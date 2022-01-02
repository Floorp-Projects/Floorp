// |jit-test| --code-coverage

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
function checkLcov(fun) {
  var keys = [ "TN", "SF", "FN", "FNDA", "FNF", "FNH", "BRDA", "BRF", "BRH", "DA", "LF", "LH" ];
  function startsWithKey(s) {
    for (k of keys) {
      if (s.startsWith(k))
        return true;
    }
    return false;
  };

  // Extract the body of the function, as the code to be executed.
  var source = fun.toString();
  source = source.slice(source.indexOf('{') + 1, source.lastIndexOf('}'));

  // Extract comment starting with the previous keys, as a reference of the
  // output expected from getLcovInfo.
  var lcovRef = [];
  var currLine = 0;
  var currFun = "<badfunction>";
  for (var line of source.split('\n')) {
    currLine++;
    for (var comment of line.split("//").slice(1)) {
      if (!startsWithKey(comment))
        continue;
      comment = comment.trim();
      if (comment.startsWith("FN:"))
        currFun = comment.split(',')[1];
      comment = comment.replace('$', currLine);
      comment = comment.replace('%', currFun);
      lcovRef.push(comment);
    }
  }

  // Evaluate the code, and generate the Lcov result from the execution.
  // Enabling LCov disables lazy parsing, as we rely on the ZoneCellIter to
  // emulate the behaviour of the finalizer.
  var g = newGlobal();
  g.eval(source);
  var lcovResRaw = getLcovInfo(g);

  // Check that all the lines are present the result.
  var lcovRes = lcovResRaw.split('\n');
  for (ref of lcovRef) {
    if (lcovRes.indexOf(ref) == -1) {
      print("Cannot find `" + ref + "` in the following Lcov result:\n", lcovResRaw);
      print("In the following source:\n", source);
      assertEq(true, false);
    }
  }
}

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  ",".split(','); //DA:$,1
  //FNF:1
  //FNH:1
  //LF:1
  //LH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  function f() {    //FN:$,f
    ",".split(','); //DA:$,0
  }
  ",".split(',');   //DA:$,1
  //FNF:2
  //FNH:1
  //LF:2
  //LH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  function f() {    //FN:$,f //FNDA:1,%
    ",".split(','); //DA:$,1
  }
  f();              //DA:$,1
  //FNF:2
  //FNH:2
  //LF:2
  //LH:2
});

checkLcov(function () { ','.split(','); //FN:$,top-level //FNDA:1,% //DA:$,1
  //FNF:1
  //FNH:1
  //LF:1
  //LH:1
});

checkLcov(function () { function f() { ','.split(','); } //FN:$,top-level //FNDA:1,% //FN:$,f //FNDA:1,f //DA:$,1
  f(); //DA:$,1
  //FNF:2
  //FNH:2
  //LF:2
  //LH:2
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 3)      //DA:$,1 //BRDA:$,0,0,1 //BRDA:$,0,1,0
    l.push('');           //DA:$,0
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:4
  //LH:3
  //BRF:2
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 2)      //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1
    l.push('');           //DA:$,1
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:4
  //LH:4
  //BRF:2
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 3)      //DA:$,1 //BRDA:$,0,0,1 //BRDA:$,0,1,0
    l.push('');           //DA:$,0
  else
    l.pop();              //DA:$,1
  //FNF:1
  //FNH:1
  //LF:4
  //LH:3
  //BRF:2
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 2)      //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1
    l.push('');           //DA:$,1
  else
    l.pop();              //DA:$,0
  //FNF:1
  //FNH:1
  //LF:4
  //LH:3
  //BRF:2
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  if (l.length == 2)      //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1
    l.push('');           //DA:$,1
  else {
    if (l.length == 1)    //DA:$,0 //BRDA:$,1,0,- //BRDA:$,1,1,-
      l.pop();            //DA:$,0
  }
  //FNF:1
  //FNH:1
  //LF:5
  //LH:3
  //BRF:4
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  function f(i) { //FN:$,f //FNDA:2,%
    var x = 0;    //DA:$,2
    while (i--) { // Currently OSR wrongly count the loop header twice.
                  // So instead of DA:$,12 , we have DA:$,13 .
      x += i;     //DA:$,10
      x = x / 2;  //DA:$,10
    }
    return x;     //DA:$,2
    //BRF:2
    //BRH:2
  }

  f(5);           //DA:$,1
  f(5);           //DA:$,1
  //FNF:2
  //FNH:2
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  try {                     //DA:$,1
    var l = ",".split(','); //DA:$,1
    if (l.length == 2) {    //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1
      l.push('');           //DA:$,1
      throw l;              //DA:$,1
    }
    l.pop();                //DA:$,0
  } catch (x) {             //DA:$,1
    x.pop();                //DA:$,1
  }
  //FNF:1
  //FNH:1
  //LF:8
  //LH:7
  //BRF:2
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(',');   //DA:$,1
  try {                     //DA:$,1
    try {                   //DA:$,1
      if (l.length == 2) {  //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1
        l.push('');         //DA:$,1
        throw l;            //DA:$,1
      }
      l.pop();              //DA:$,0
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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

// Test TableSwitch opcode
checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0 //BRDA:$,0,4,0
    case 0:
      l.push('0');        //DA:$,0
      break;
    case 1:
      l.push('1');        //DA:$,0
      break;
    case 2:
      l.push('2');        //DA:$,1
      break;
    case 3:
      l.push('3');        //DA:$,0
      break;
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:4
  //BRF:5
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0 //BRDA:$,0,4,0
    case 0:
      l.push('0');        //DA:$,0
    case 1:
      l.push('1');        //DA:$,0
    case 2:
      l.push('2');        //DA:$,1
    case 3:
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:5
  //BRF:5
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
                          // Branches are ordered, and starting at 0
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,1 //BRDA:$,0,1,0 //BRDA:$,0,2,0 //BRDA:$,0,3,0 //BRDA:$,0,4,0
    case 5:
      l.push('5');        //DA:$,0
    case 4:
      l.push('4');        //DA:$,0
    case 3:
      l.push('3');        //DA:$,0
    case 2:
      l.push('2');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:4
  //BRF:5
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,1 //BRDA:$,0,1,0 //BRDA:$,0,2,0
    case 2:
      l.push('2');        //DA:$,1
    case 5:
      l.push('5');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:5
  //LH:5
  //BRF:3
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1 //BRDA:$,0,2,0
    case 3:
      l.push('1');        //DA:$,0
    case 5:
      l.push('5');        //DA:$,0
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:5
  //LH:3
  //BRF:3
  //BRH:1
});

// Unfortunately the differences between switch implementations leaks in the
// code coverage reports.
checkLcov(function () { //FN:$,top-level //FNDA:1,%
  function f(a) {         //FN:$,f //FNDA:2,%
    return a;             //DA:$,2
  }
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1
    case f(-42):          //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1
      l.push('1');        //DA:$,0
    case f(51):           //DA:$,1 //BRDA:$,1,0,0 //BRDA:$,1,1,1
      l.push('5');        //DA:$,0
  }
  l.pop();                //DA:$,1
  //FNF:2
  //FNH:2
  //LF:8
  //LH:6
  //BRF:4
  //BRH:2
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,1 //BRDA:$,0,2,0 //BRDA:$,0,3,0
    case 0:
    case 1:
      l.push('0');        //DA:$,0
      l.push('1');        //DA:$,0
    case 2:
      l.push('2');        //DA:$,1
    case 3:
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:5
  //BRF:4
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0
    case 0:
      l.push('0');        //DA:$,0
    case 1:
      l.push('1');        //DA:$,0
    case 2:
    case 3:
      l.push('2');        //DA:$,1
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:5
  //BRF:4
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0
    case 0:
      l.push('0');        //DA:$,0
    case 1:
    default:
      l.push('1');        //DA:$,0
    case 2:
      l.push('2');        //DA:$,1
    case 3:
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:5
  //BRF:4
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0
    case 0:
      l.push('0');        //DA:$,0
    case 1:
      l.push('1');        //DA:$,0
    default:
    case 2:
      l.push('2');        //DA:$,1
    case 3:
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:5
  //BRF:4
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0 //BRDA:$,0,4,0
    case 0:
      l.push('0');        //DA:$,0
    case 1:
      l.push('1');        //DA:$,0
    default:
      l.push('default');  //DA:$,0
    case 2:
      l.push('2');        //DA:$,1
    case 3:
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:8
  //LH:5
  //BRF:5
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ",".split(','); //DA:$,1
  switch (l.length) {     //DA:$,1 //BRDA:$,0,0,0 //BRDA:$,0,1,0 //BRDA:$,0,2,1 //BRDA:$,0,3,0
    case 0:
      l.push('0');        //DA:$,0
    case 1:
      l.push('1');        //DA:$,0
    default:
      l.push('2');        //DA:$,1
    case 3:
      l.push('3');        //DA:$,1
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:7
  //LH:5
  //BRF:4
  //BRH:1
});

checkLcov(function () { //FN:$,top-level //FNDA:1,%
  var l = ','.split(','); //DA:$,1
  if (l.length === 45) {  //DA:$,1 //BRDA:$,0,0,1 //BRDA:$,0,1,0
    switch (l[0]) {       //DA:$,0 //BRDA:$,1,0,- //BRDA:$,1,1,-
      case ',':
        l.push('0');      //DA:$,0
      default:
        l.push('1');      //DA:$,0
    }
  }
  l.pop();                //DA:$,1
  //FNF:1
  //FNH:1
  //LF:6
  //LH:3
  //BRF:4
  //BRH:1
});

// These tests are not included in ../debug/Script-getOffsetsCoverage-01.js
// because we're specifically testing a feature of Lcov output that
// Debugger.Script doesn't have (the aggregation of hits that are on the
// same line but in different functions).
{
    checkLcov(function () { //FN:$,top-level //FNDA:1,%
        function f() { return 0; } var l = f(); //DA:$,2
        //FNF:2
        //FNH:2
        //LF:1
        //LH:1
    });

    // A single line has two functions on it, and both hit.
    checkLcov(function () { //FN:$,top-level //FNDA:1,%
        function f() { return 0; } function g() { return 1; } //DA:$,2
        var v = f() + g(); //DA:$,1
        //FNF:3
        //FNH:3
        //LF:2
        //LH:2
    });

    // A line has both function code and toplevel code, and only one of them hits.
    checkLcov(function () { //FN:$,top-level //FNDA:1,%
        if (1 === 2)  //DA:$,1
            throw "0 hits here"; function f() { return "1 hit here"; } //DA:$,1
        f();  //DA:$,1
        //FNF:2
        //FNH:2
        //LF:3
        //LH:3
    });
}

// These tests are not included in ../debug/Script-getOffsetsCoverage-01.js
// because they are testing behaviour of --code-coverage.
{
  // Test function names
  checkLcov(function () {
    //FN:1,top-level
    //FNDA:1,top-level

    var x = function() {};  //FN:$,x
    let y = function() {};  //FN:$,y

    let z = {
      z_method() { },       //FN:$,z_method
      get z_prop() { },     //FN:$,get z_prop
    }
  });
}

// If you add a test case here, do the same in
// jit-test/tests/debug/Script-getOffsetsCoverage-01.js
