// |jit-test| --code-coverage;

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
  var source = fun.toSource();
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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

checkLcov(function () { //FN:$,top-level //FNDA:1,%
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
