// |jit-test| error: ReferenceError; --fuzzing-safe; --thread-count=1; --ion-eager
const ALL_TESTS =  [
    "CONTEXT_OBJECT_PROPERTY_DOT_REFERENCE_IS_FUNCTION",
    ];
function r(keyword, tests) {
  function Reserved(keyword, tests) {
    this.keyword = keyword;
    if (tests)
      this.tests = tests;
    else
      this.tests = ALL_TESTS;
  }
  return new Reserved(keyword, tests);
}
for (var i = 2; i >= 0; i--) {
  gc();
  gczeal(14, 17);
  [
    r("break"),
    r("case"),
    r("catch"),
    r("continue"),
    ];
  [
    r("true"),
    r("null"),
    r("each"),
    r("let")
    ];
}
Failure;

