function test(desc, actual, expected)
{
  if (expected == actual)
    return print(desc, ": passed");
  print(desc, ": FAILED: expected", typeof(expected), "(", expected, ") != actual",
	typeof(actual), "(", actual, ")");
}

function ifInsideLoop()
{
  var cond = true, count = 0;
  for (var i = 0; i < 5000; i++) {
    if (cond)
      count++;
  }
  return count;
}
test("tracing if", ifInsideLoop(), 5000);

function bitwiseAnd(bitwiseAndValue) {
  for (var i = 0; i < 60000; i++)
    bitwiseAndValue = bitwiseAndValue & i;
  return bitwiseAndValue;
}
test("bitwise and with arg/var", bitwiseAnd(12341234), 0)

bitwiseAndValue = Math.pow(2,32);
for (var i = 0; i < 60000; i++)
  bitwiseAndValue = bitwiseAndValue & i;
test("bitwise on undeclared globals", bitwiseAndValue, 0);

function equalInt()
{
  var i1 = 55;
  var hits = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
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
  }
  return hits.toString();
}
test("int equality", equalInt(),
     "5000,5000,5000,5000,5000,5000,0,0,0,0,0,0,0,0,0,0,0,0,0");


function setelem(a)
{
  var l = a.length;
  for (var i = 0; i < l; i++) {
    a[i] = i;
  }
  return a.toString();
}

var a = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
a = a.concat(a, a, a);
setelem(a)
test("setelem", a, "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83");

function getelem(a)
{
  var accum = 0;
  var l = a.length;
  for (var i = 0; i < l; i++) {
    accum += a[i];
  }
  return accum;
}
test("getelem", getelem(a), 3486);

globalName = 907;
function name()
{
  var a = 0;
  for (var i = 0; i < 100; i++)
    a = globalName;
  return a;
}
test("undeclared globals from function", name(), 907);

var globalInt = 0;
for (var i = 0; i < 500; i++)
  globalInt = globalName + i;
test("get undeclared global at top level", globalInt, globalName + 499);

for (var i = 0; i < 500; i++)
  globalInt = i;
test("setting global variable", globalInt, 499);

function arith()
{
  var accum = 0;
  for (var i = 0; i < 100; i++) {
    accum += (i * 2) - 1;
  }
  return accum;
}
test("basic arithmetic", arith(), 9800);

function lsh(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x1 << n;
  return r;
}
test("lsh", [lsh(15),lsh(55),lsh(1),lsh(0)],"32768,8388608,2,1");

function rsh(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = 0x11010101 >> n;
  return r;
}
test("rsh", [rsh(8),rsh(5),rsh(35),rsh(-1)],"1114369,8914952,35659808,0");

function ursh(n)
{
  var r;
  for (var i = 0; i < 35; i++)
    r = -55 >>> n;
  return r;
}
test("ursh", [ursh(8),ursh(33),ursh(0),ursh(1)],
     "16777215,2147483620,4294967241,2147483620");

function doMath(cos)
{
    var s = 0;
    var sin = Math.sin;
    for (var i = 0; i < 200; i++)
        s = -Math.pow(sin(i) + cos(i * 0.75), 4);
    return s;
}
test("Math.sin/cos/pow", doMath(Math.cos), -0.5405549555611059);

function unknownCall(Math)
{
   var s = 0;
   for (var i = 0; i < 200; i++)
     s = Math.log(i);
   return s;
}
test("untraced call", unknownCall(Math), 5.293304824724492);

function fannkuch(n) {
   var count = Array(n);

   var r = n;
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
test("fannkuch", fannkuch(8), 41);

function xprop()
{
  a = 0;
  for (var i = 0; i < 20; i++)
    a += 7;
  return a;
}
test("xprop", xprop(), 140);

var a = 2;
function getprop(o2)
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
test("getprop", getprop({a:9}), 360);

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
test("mod", mod(), "4.5,0,42,4,NaN");

function call()
{
  var q1 = 0, q2 = 0;
  function f1() {
      return 1;
  }
  function f2(f) {
      return f();
  }
  for (var i = 0; i < 100; ++i) {
      q1 += f1();
      q2 += f2(f1);
  }  
  var ret = [q1, q2];
  return ret;
}
test("call", call(), "100,100");
