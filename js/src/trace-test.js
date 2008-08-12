var testName = null;
if ("arguments" in this && arguments.length > 0)
  testName = arguments[0];
var fails = [], passes=[];

function test(f)
{
  if (!testName || testName == f.name)
    check(f.name, f(), f.expected);
}

function check(desc, actual, expected)
{
  if (expected == actual) {
    passes.push(desc);
    return print(desc, ": passed");
  }
  fails.push(desc);
  print(desc, ": FAILED: expected", typeof(expected), "(", expected, ") != actual",
	typeof(actual), "(", actual, ")");
}

function ifInsideLoop()
{
  var cond = true, intCond = 5, count = 0;
  for (var i = 0; i < 100; i++) {
    if (cond)
      count++;
    if (intCond)
      count++;
  }
  return count;
}
ifInsideLoop.expected = 200;
test(ifInsideLoop);

function bitwiseAnd_inner(bitwiseAndValue) {
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  return bitwiseAndValue;
}
function bitwiseAnd()
{
  return bitwiseAnd_inner(12341234);
}
bitwiseAnd.expected = 0;
test(bitwiseAnd);

if (!testName || testName == "bitwiseGlobal") {
  bitwiseAndValue = Math.pow(2,32);
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  check("bitwiseGlobal", bitwiseAndValue, 0);
}


function equalInt()
{
  var i1 = 55, one = 1, zero = 0, undef;
  var o1 = { }, o2 = { };
  var s = "5";
  var hits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  for (var i = 0; i < 5000; i++) {
    if (i1 == 55) hits[0]++;
    if (i1 != 56) hits[1]++;
    if (i1 < 56)  hits[2]++;
    if (i1 > 50)  hits[3]++;
    if (i1 <= 60) hits[4]++;
    if (i1 >= 30) hits[5]++;
    if (i1 == 7)  hits[6]++;
    if (i1 != 55) hits[7]++;
    if (i1 < 30)  hits[8]++;
    if (i1 > 90)  hits[9]++;
    if (i1 <= 40) hits[10]++;
    if (i1 >= 70) hits[11]++;
    if (o1 == o2) hits[12]++;
    if (o2 != null) hits[13]++;
    if (s < 10) hits[14]++;
    if (true < zero) hits[15]++;
    if (undef > one) hits[16]++;
    if (undef < zero) hits[17]++;
  }
  return hits.toString();
}
equalInt.expected = "5000,5000,5000,5000,5000,5000,0,0,0,0,0,0,0,5000,5000,0,0,0";
test(equalInt);

var a;
function setelem()
{
  a = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
  a = a.concat(a, a, a);
  var l = a.length;
  for (var i = 0; i < l; i++) {
    a[i] = i;
  }
  return a.toString();
}
setelem.expected = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83";
test(setelem);

function getelem_inner(a)
{
  var accum = 0;
  var l = a.length;
  for (var i = 0; i < l; i++) {
    accum += a[i];
  }
  return accum;
}
function getelem()
{
  return getelem_inner(a);
}
getelem.expected = 3486;
test(getelem);

globalName = 907;
function name()
{
  var a = 0;
  for (var i = 0; i < 100; i++)
    a = globalName;
  return a;
}
name.expected = 907;
test(name);

var globalInt = 0;
if (!testName || testName == "globalGet") {
  for (var i = 0; i < 500; i++)
    globalInt = globalName + i;
  check("globalGet", globalInt, globalName + 499);
}

if (!testName || testName == "globalSet") {
  for (var i = 0; i < 500; i++)
    globalInt = i;
  check("globalSet", globalInt, 499);
}

function arith()
{
  var accum = 0;
  for (var i = 0; i < 100; i++) {
    accum += (i * 2) - 1;
  }
  return accum;
}
arith.expected = 9800;
test(arith);

function lsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x1 << n;
  return r;
}
function lsh()
{
  return [lsh_inner(15),lsh_inner(55),lsh_inner(1),lsh_inner(0)];
}
lsh.expected = "32768,8388608,2,1";
test(lsh);

function rsh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x11010101 >> n;
  return r;
}
function rsh()
{
  return [rsh_inner(8),rsh_inner(5),rsh_inner(35),rsh_inner(-1)];
}
rsh.expected = "1114369,8914952,35659808,0";
test(rsh);

function ursh_inner(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = -55 >>> n;
  return r;
}
function ursh() {
  return [ursh_inner(8),ursh_inner(33),ursh_inner(0),ursh_inner(1)];
}
ursh.expected = "16777215,2147483620,4294967241,2147483620";
test(ursh);

function doMath_inner(cos)
{
    var s = 0;
    var sin = Math.sin;
    for (var i = 0; i < 200; i++)
        s = -Math.pow(sin(i) + cos(i * 0.75), 4);
    return s;
}
function doMath() {
  return doMath_inner(Math.cos);
}
doMath.expected = -0.5405549555611059;
test(doMath);

function fannkuch() {
   var count = Array(8);
   var r = 8;
   var done = 0;
   while (done < 40) {
      // write-out the first 30 permutations
      done += r;
      while (r != 1) { count[r - 1] = r; r--; }
      while (true) {
         count[r] = count[r] - 1;
         if (count[r] > 0) break;
         r++;
      }
   }
   return done;
}
fannkuch.expected = 41;
test(fannkuch);

function xprop()
{
  a = 0;
  for (var i = 0; i < 20; i++)
    a += 7;
  return a;
}
xprop.expected = 140;
test(xprop);

var a = 2;
function getprop_inner(o2)
{
  var o = {a:5};
  var t = this;
  var x = 0;
  for (var i = 0; i < 20; i++) {
    t = this;
    x += o.a + o2.a + this.a + t.a;
  }
  return x;
}
function getprop() {
  return getprop_inner({a:9});
}
getprop.expected = 360;
test(getprop);

function mod()
{
  var mods = [-1,-1,-1,-1];
  var a = 9.5, b = -5, c = 42, d = (1/0);
  for (var i = 0; i < 20; i++) {
    mods[0] = a % b;
    mods[1] = b % 1;
    mods[2] = c % d;
    mods[3] = c % a;
    mods[4] = b % 0;
  }
  return mods.toString();
}
mod.expected = "4.5,0,42,4,NaN";
test(mod);

function glob_f1() {
  return 1;
}
function glob_f2() {
  return glob_f1();
}
function call()
{
  var q1 = 0, q2 = 0, q3 = 0, q4 = 0, q5 = 0;
  var o = {};
  function f1() {
      return 1;
  }
  function f2(f) {
      return f();
  }
  o.f = f1;
  for (var i = 0; i < 100; ++i) {
      q1 += f1();
      q2 += f2(f1);
      q3 += glob_f1();
      q4 += o.f();
      q5 += glob_f2();
  }
  var ret = [q1, q2, q3, q4, q5];
  return ret;
}
call.expected =  "100,100,100,100,100";
test(call);

function setprop()
{
  var obj = { a:-1 };
  var obj2 = { b:-1, a:-1 };
  for (var i = 0; i < 20; i++) {
    obj2.b = obj.a = i;
  }
  return [obj.a, obj2.a, obj2.b].toString();
}
setprop.expected =  "19,-1,19";
test(setprop);

function testif() {
	var q = 0;
	for (var i = 0; i < 100; i++) {
		if ((i & 1) == 0)
			q++;
		else
			q--;
	}
    return q;
}
testif.expected = "0";
test(testif);

var globalinc = 0;
function testincops(n) {
  var i = 0, o = {p:0}, a = [0];
  n = 100;

  for (i = 0; i < n; i++);
  while (i-- > 0);
  for (i = 0; i < n; ++i);
  while (--i >= 0);

  for (o.p = 0; o.p < n; o.p++) globalinc++;
  while (o.p-- > 0) --globalinc;
  for (o.p = 0; o.p < n; ++o.p) ++globalinc;
  while (--o.p >= 0) globalinc--;

  ++i; // set to 0
  for (a[i] = 0; a[i] < n; a[i]++);
  while (a[i]-- > 0);
  for (a[i] = 0; a[i] < n; ++a[i]);
  while (--a[i] >= 0);

  return [++o.p, ++a[i], globalinc].toString();
}
testincops.expected = "0,0,0";
test(testincops);

function trees() {
  var i = 0, o = [0,0,0];
  for (i = 0; i < 100; ++i) {
    if ((i & 1) == 0) o[0]++;
    else if ((i & 2) == 0) o[1]++;
    else o[2]++;
  }
  return o;
}
trees.expected = "50,25,25";
test(trees);

function unboxint() {
    var q = 0;
    var o = [4];
    for (var i = 0; i < 100; ++i)
	q = o[0] << 1;
    return q;
}
unboxint.expected = "8";
test(unboxint);

function strings()
{
  var a = [], b = -1;
  var s = "abcdefghij", s2 = "a";
  var f = "f";
  var c = 0, d = 0, e = 0, g = 0;
  for (var i = 0; i < 10; i++) {
    a[i] = (s.substring(i, i+1) + s[i] + String.fromCharCode(s2.charCodeAt(0) + i)).concat(i) + i;
    if (s[i] == f)
      c++;
    if (s[i] != 'b')
      d++;
    if ("B" > s2)
      g++; // f already used
    if (s2 < "b")
      e++;
    b = s.length;
  }
  return a.toString() + b + c + d + e + g;
}
strings.expected = "aaa00,bbb11,ccc22,ddd33,eee44,fff55,ggg66,hhh77,iii88,jjj991019100";
test(strings);

function dependentStrings()
{
  var a = [];
  var t = "abcdefghijklmnopqrst";
  for (var i = 0; i < 10; i++) {
    var s = t.substring(2*i, 2*i + 2);
    a[i] = s + s.length;
  }
  return a.join("");
}
dependentStrings.expected = "ab2cd2ef2gh2ij2kl2mn2op2qr2st2";
test(dependentStrings);

function stringConvert()
{
  var a = [];
  var s1 = "F", s2 = "1.3", s3 = "5";
  for (var i = 0; i < 10; i++) {
    a[0] = 1 >> s1;
    a[1] = 10 - s2;
    a[2] = 15 * s3;
    a[3] = s3 | 32;
    a[4] = s2 + 60;
    // a[5] = 9 + s3;
    // a[6] = -s3;
    a[7] = s3 & "7";
    // a[8] = ~s3;
  }
  return a.toString();
}
stringConvert.expected = "1,8.7,75,37,1.360,,,5";
test(stringConvert);

function orTestHelper(a, b, n)
{
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (a || b)
      k += i;
  }
  return k;
}

var orNaNTest1, orNaNTest2;

orNaNTest1 = new Function("return orTestHelper(NaN, NaN, 10);");
orNaNTest1.name = 'orNaNTest1';
orNaNTest1.expected = '0';
orNaNTest2 = new Function("return orTestHelper(NaN, 1, 10);");
orNaNTest2.name = 'orNaNTest2';
orNaNTest2.expected = '45';
test(orNaNTest1);
test(orNaNTest2);

function andTestHelper(a, b, n)
{
  var k = 0;
  for (var i = 0; i < n; i++) {
    if (a && b)
      k += i;
  }
  return k;
}

if (!testName || testName == "truthies") {
  (function () {
     var opsies   = ["||", "&&"];
     var falsies  = [null, undefined, false, NaN, 0, ""];
     var truthies = [{}, true, 1, 42, 1/0, -1/0, "blah"];
     var boolies  = [falsies, truthies];

     // The for each here should abort tracing, so that this test framework
     // relies only on the interpreter while the orTestHelper and andTestHelper
     //  functions get trace-JITed.
     for each (var op in opsies) {
       for (var i in boolies) {
	 for (var j in boolies[i]) {
           var x = uneval(boolies[i][j]);
           for (var k in boolies) {
             for (var l in boolies[k]) {
               var y = uneval(boolies[k][l]);
               var prefix = (op == "||") ? "or" : "and";
               var f = new Function("return " + prefix + "TestHelper(" + x + "," + y + ",10)");
               f.name = prefix + "Test(" + x + "," + y + ")";
               f.expected = eval(x + op + y) ? 45 : 0;
               test(f);
             }
           }
	 }
       }
     }
   })();
}

function nonEmptyStack1Helper(o, farble) {
    var a = [];
    var j = 0;
    for (var i in o)
        a[j++] = i;
    return a.join("");
}

function nonEmptyStack1() {
    return nonEmptyStack1Helper({a:1,b:2,c:3,d:4,e:5,f:6,g:7,h:8}, "hi");
}

nonEmptyStack1.expected = "abcdefgh";
test(nonEmptyStack1);

function nonEmptyStack2()
{
  var a = 0;
  for (var c in {a:1, b:2, c:3}) {
    for (var i = 0; i < 10; i++)
      a += i;
  }
  return String(a);
}
nonEmptyStack2.expected = "135";
test(nonEmptyStack2);

function arityMismatchMissingArg(arg)
{
  for (var a = 0, i = 1; i < 10000; i *= 2) {
    a += i;
  }
  return a;
}
arityMismatchMissingArg.expected = 16383;
test(arityMismatchMissingArg);

function arityMismatchExtraArg()
{
  return arityMismatchMissingArg(1, 2);
}
arityMismatchExtraArg.expected = 16383;
test(arityMismatchExtraArg);

function MyConstructor(i)
{
  this.i = i;
}
MyConstructor.prototype.toString = function() {return this.i + ""};

function newTest()
{
  var a = [];
  for (var i = 0; i < 10; i++)
    a[i] = new MyConstructor(i);
  return a.join("");
}
newTest.expected = "0123456789";
test(newTest);

// The following functions use a delay line of length 2 to change the value
// of the callee without exiting the traced loop. This is obviously tuned to
// match the current HOTLOOP setting of 2.
function shapelessArgCalleeLoop(f, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessVarCalleeLoop(f0, g, h, a)
{
  var f = f0;
  for (var i = 0; i < 10; i++) {
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessLetCalleeLoop(f0, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    let f = f0;
    f(i, a);
    f = g;
    g = h;
  }
}

function shapelessUnknownCalleeLoop(n, f, g, h, a)
{
  for (var i = 0; i < 10; i++) {
    (n || f)(i, a);
    f = g;
    g = h;
  }
}

function shapelessCalleeTest()
{
  var a = [];

  var helper = function (i, a) a[i] = i;
  shapelessArgCalleeLoop(helper, helper, function (i, a) a[i] = -i, a);

  helper = function (i, a) a[10 + i] = i;
  shapelessVarCalleeLoop(helper, helper, function (i, a) a[10 + i] = -i, a);

  helper = function (i, a) a[20 + i] = i;
  shapelessLetCalleeLoop(helper, helper, function (i, a) a[20 + i] = -i, a);

  helper = function (i, a) a[30 + i] = i;
  shapelessUnknownCalleeLoop(null, helper, helper, function (i, a) a[30 + i] = -i, a);

  try {
    helper = {hack: 42};
    shapelessUnknownCalleeLoop(null, helper, helper, helper, a);
  } catch (e) {
    if (e + "" != "TypeError: f is not a function")
      print("shapelessUnknownCalleeLoop: unexpected exception " + e);
  }
  return a.join("");
}
shapelessCalleeTest.expected = "01-2-3-4-5-6-7-8-901-2-3-4-5-6-7-8-9012345678901-2-3-4-5-6-7-8-9";
test(shapelessCalleeTest);

function typeofTest()
{
  var values = ["hi", "hi", "hi", null, 5, 5.1, true, undefined, /foo/, typeofTest, [], {}], types = [];
  for (var i = 0; i < values.length; i++)
    types[i] = typeof values[i];
  return types.toString();
}
typeofTest.expected = "string,string,string,object,number,number,boolean,undefined,object,function,object,object";
test(typeofTest);

function joinTest()
{
  var s = "";
  var a = [];
  for (var i = 0; i < 8; i++)
    a[i] = [String.fromCharCode(97 + i)];
  for (i = 0; i < 8; i++) {
    for (var j = 0; j < 8; j++)
      a[i][1 + j] = j;
  }
  for (i = 0; i < 8; i++)
    s += a[i].join(",");
  return s;
}
joinTest.expected = "a,0,1,2,3,4,5,6,7b,0,1,2,3,4,5,6,7c,0,1,2,3,4,5,6,7d,0,1,2,3,4,5,6,7e,0,1,2,3,4,5,6,7f,0,1,2,3,4,5,6,7g,0,1,2,3,4,5,6,7h,0,1,2,3,4,5,6,7";
test(joinTest);


/* Keep these at the end so that we can see the summary after the trace-debug spew. */
print("\npassed:", passes.length && passes.join(","));
print("\nFAILED:", fails.length && fails.join(","));
