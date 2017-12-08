// |reftest| skip-if(!xulRuntime.shell)

// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 822041;
var summary = "Live generators should not cache Gecko Profiler state";

print(BUGNUMBER + ": " + summary);

function* gen() {
  var x = yield turnoff();
  yield x;
  yield 'bye';
}

function turnoff() {
  print("Turning off profiler\n");
  disableGeckoProfiling();
  return 'hi';
}

for (var slowAsserts of [ true, false ]) {
  // The slowAssertions setting is not expected to matter
  if (slowAsserts)
    enableGeckoProfilingWithSlowAssertions();
  else
    enableGeckoProfiling();

  g = gen();
  assertEq(g.next().value, 'hi');
  assertEq(g.next('gurgitating...').value, 'gurgitating...');
  for (var x of g)
    assertEq(x, 'bye');
}

// This is really a crashtest
reportCompare(0, 0, 'ok');
