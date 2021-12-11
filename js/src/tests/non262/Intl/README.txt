Integration Tests for ECMAScript Internationalization API
=========================================================

The tests in this directory test the integration of the ICU library
(Internationalization Components for Unicode) into the implementation of the
ECMAScript Internationalization API in SpiderMonkey.

These integration tests are complementary to:

- The Test402 test suite maintained by Ecma TC39, which tests conformance of
  an implementation to standard ECMA-402, ECMAScript Internationalization API
  Specification. Test402 is currently maintained as part of Test262, the overall
  conformance test suite for ECMAScript; for more information, see
  http://wiki.ecmascript.org/doku.php?id=test262:test262

- The test suite of the ICU library, which tests the implementation of ICU
  itself and correct interpretation of the locale data it obtains from CLDR
  (Common Locale Data Repository). For information on ICU, see
  http://site.icu-project.org

The integration tests check for a variety of locales and options whether the
results are localized in a way that indicates correct integration with ICU.
Such tests are somewhat fragile because the underlying locale data reflects
real world usage and is therefore subject to change. When the ICU library used
by Mozilla is upgraded, it is likely that some of the integration tests will
fail because of locale data changes; however, others might fail because of
actual software bugs. Failures therefore have to be examined carefully.
