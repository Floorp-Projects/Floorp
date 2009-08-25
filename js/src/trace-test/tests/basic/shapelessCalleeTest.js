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
assertEq(shapelessCalleeTest(), "01-2-3-4-5-6-7-8-901-2-3-4-5-6-7-8-9012345678901-2-3-4-5-6-7-8-9");
