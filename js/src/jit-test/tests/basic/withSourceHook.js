// Check that withSourceHook passes URLs, propagates exceptions, and
// properly restores the original source hooks.

load(libdir + 'asserts.js');

// withSourceHook isn't defined if you pass the shell the --fuzzing-safe
// option. Skip this test silently, to avoid spurious failures.
if (typeof withSourceHook != 'function')
  quit(0);

var log = '';

// Establish an outermost source hook.
withSourceHook(function (url) {
  log += 'o';
  assertEq(url, 'outer');
  return '(function outer() { 3; })';
}, function () {
  log += 'O';
  // Verify that withSourceHook propagates exceptions thrown by source hooks.
  assertThrowsValue(function () {
    // Establish a source hook that throws.
    withSourceHook(function (url) {
      log += 'm';
      assertEq(url, 'middle');
      throw 'borborygmus'; // middle
    }, function () {
      log += 'M';
      // Establish an innermost source hook that does not throw,
      // and verify that it is in force.
      assertEq(withSourceHook(function (url) {
                                log += 'i';
                                assertEq(url, 'inner');
                                return '(function inner() { 1; })';
                              }, function () {
                                log += 'I';
                                return evaluate('(function inner() { 2; })',
                                                { fileName: 'inner', sourceIsLazy: true })
                                       .toSource();
                              }),
               '(function inner() { 1; })');
      // Verify that the source hook that throws has been reinstated.
      evaluate('(function middle() { })',
               { fileName: 'middle', sourceIsLazy: true })
      .toSource();
    });
  }, 'borborygmus');

  // Verify that the outermost source hook has been restored.
  assertEq(evaluate('(function outer() { 4; })',
                    { fileName: 'outer', sourceIsLazy: true })
           .toSource(),
           '(function outer() { 3; })');
});

assertEq(log, 'OMIimo');
