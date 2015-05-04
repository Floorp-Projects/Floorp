/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <stdlib.h>

#ifndef MOZILLA_INTERNAL_API
// some of the includes make use of internal string types
#define nsAString_h___
#define nsString_h___
#define nsStringFwd_h___
#define nsReadableUtils_h___
class nsACString;
class nsAString;
class nsAFlatString;
class nsAFlatCString;
class nsAdoptingString;
class nsAdoptingCString;
class nsXPIDLString;
template<class T> class nsReadingIterator;
#endif

#include "nsIContentSecurityPolicy.h"
#include "nsNetUtil.h"
#include "TestHarness.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/dom/nsCSPContext.h"

#ifndef MOZILLA_INTERNAL_API
#undef nsString_h___
#undef nsAString_h___
#undef nsReadableUtils_h___
#endif

/*
 * Testing the parser is non trivial, especially since we can not call
 * parser functionality directly in compiled code tests.
 * All the tests (except the fuzzy tests at the end) follow the same schemata:
 *   a) create an nsIContentSecurityPolicy object
 *   b) set the selfURI in SetRequestContext
 *   c) append one or more policies by calling AppendPolicy
 *   d) check if the policy count is correct by calling GetPolicyCount
 *   e) compare the result of the policy with the expected output
 *      using the struct PolicyTest;
 *
 * In general we test:
 * a) policies that the parser should accept
 * b) policies that the parser should reject
 * c) policies that are randomly generated (fuzzy tests)
 *
 * Please note that fuzzy tests are *DISABLED* by default and shold only
 * be run *OFFLINE* whenever code in nsCSPParser changes.
 * To run fuzzy tests, flip RUN_OFFLINE_TESTS to 1.
 *
 */

#define RUN_OFFLINE_TESTS 0

/*
 * Offline tests are separated in three different groups:
 *  * TestFuzzyPolicies - complete random ASCII input
 *  * TestFuzzyPoliciesIncDir - a directory name followed by random ASCII
 *  * TestFuzzyPoliciesIncDirLimASCII - a directory name followed by limited ASCII
 *    which represents more likely user input.
 *
 *  We run each of this categories |kFuzzyRuns| times.
 */

#if RUN_OFFLINE_TESTS
static const uint32_t kFuzzyRuns = 10000;
#endif

// For fuzzy testing we actually do not care about the output,
// we just want to make sure that the parser can handle random
// input, therefore we use kFuzzyExpectedPolicyCount to return early.
static const uint32_t kFuzzyExpectedPolicyCount = 111;

static const uint32_t kMaxPolicyLength = 96;

struct PolicyTest
{
  char policy[kMaxPolicyLength];
  char expectedResult[kMaxPolicyLength];
};

nsresult runTest(uint32_t aExpectedPolicyCount, // this should be 0 for policies which should fail to parse
                 const char* aPolicy,
                 const char* aExpextedResult) {

  // we init the csp with http://www.selfuri.com
  nsCOMPtr<nsIURI> selfURI;
  nsresult rv = NS_NewURI(getter_AddRefs(selfURI), "http://www.selfuri.com");
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptSecurityManager> secman =
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
     nsCOMPtr<nsIPrincipal> systemPrincipal;
  rv = secman->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  // we also init the csp with a dummyChannel, which is unused
  // for the parser tests but surpresses assertions in SetRequestContext.
  nsCOMPtr<nsIChannel> dummyChannel;
  rv = NS_NewChannel(getter_AddRefs(dummyChannel),
                     selfURI,
                     systemPrincipal,
                     nsILoadInfo::SEC_NORMAL,
                     nsIContentPolicy::TYPE_OTHER);
  NS_ENSURE_SUCCESS(rv, rv);

  // create a CSP object
  nsCOMPtr<nsIContentSecurityPolicy> csp =
    do_CreateInstance(NS_CSPCONTEXT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // for testing the parser we only need to set selfURI which is needed
  // to translate the keyword 'self' into an actual URI. All other
  // arguments can be nullptrs.
  csp->SetRequestContext(selfURI,
                         nullptr,  // nsIURI* aReferrer
                         dummyChannel);
  NS_ENSURE_SUCCESS(rv, rv);

  // append a policy
  nsString policyStr;
  policyStr.AssignASCII(aPolicy);
  rv = csp->AppendPolicy(policyStr, false);
  NS_ENSURE_SUCCESS(rv, rv);

  // when executing fuzzy tests we do not care about the actual output
  // of the parser, we just want to make sure that the parser is not crashing.
  if (aExpectedPolicyCount == kFuzzyExpectedPolicyCount) {
    return NS_OK;
  }

  // verify that the expected number of policies exists
  uint32_t actualPolicyCount;
  rv = csp->GetPolicyCount(&actualPolicyCount);
  NS_ENSURE_SUCCESS(rv, rv);
  if (actualPolicyCount != aExpectedPolicyCount) {
    fail("Actual policy count not equal to expected policy count (%d != %d) for policy: %s",
          actualPolicyCount, aExpectedPolicyCount, aPolicy);
    return NS_ERROR_UNEXPECTED;
  }

  // if the expected policy count is 0, we can return, because
  // we can not compare any output anyway. Used when parsing
  // errornous policies.
  if (aExpectedPolicyCount == 0) {
    return NS_OK;
  }

  // compare the parsed policy against the expected result
  nsString parsedPolicyStr;
  // checking policy at index 0, which is the one what we appended.
  rv = csp->GetPolicy(0, parsedPolicyStr);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!NS_ConvertUTF16toUTF8(parsedPolicyStr).EqualsASCII(aExpextedResult)) {
    fail("Actual policy does not match expected policy  (%s != %s)",
          NS_ConvertUTF16toUTF8(parsedPolicyStr).get(), aExpextedResult);
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

// ============================= run Tests ========================

nsresult runTestSuite(const PolicyTest* aPolicies,
                      uint32_t aPolicyCount,
                      uint32_t aExpectedPolicyCount) {
  nsresult rv;
  for (uint32_t i = 0; i < aPolicyCount; i++) {
    rv = runTest(aExpectedPolicyCount, aPolicies[i].policy, aPolicies[i].expectedResult);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// ============================= TestDirectives ========================

nsresult TestDirectives() {

  static const PolicyTest policies[] =
  {
    { "default-src http://www.example.com",
      "default-src http://www.example.com" },
    { "script-src http://www.example.com",
      "script-src http://www.example.com" },
    { "object-src http://www.example.com",
      "object-src http://www.example.com" },
    { "style-src http://www.example.com",
      "style-src http://www.example.com" },
    { "img-src http://www.example.com",
      "img-src http://www.example.com" },
    { "media-src http://www.example.com",
      "media-src http://www.example.com" },
    { "frame-src http://www.example.com",
      "frame-src http://www.example.com" },
    { "font-src http://www.example.com",
      "font-src http://www.example.com" },
    { "connect-src http://www.example.com",
      "connect-src http://www.example.com" },
    { "report-uri http://www.example.com",
      "report-uri http://www.example.com/" },
    { "script-src 'nonce-correctscriptnonce'",
      "script-src 'nonce-correctscriptnonce'" },
    { "script-src 'sha256-siVR8vAcqP06h2ppeNwqgjr0yZ6yned4X2VF84j4GmI='",
      "script-src 'sha256-siVR8vAcqP06h2ppeNwqgjr0yZ6yned4X2VF84j4GmI='" }
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestKeywords ========================

nsresult TestKeywords() {

  static const PolicyTest policies[] =
  {
    { "script-src 'self'",
      "script-src http://www.selfuri.com" },
    { "script-src 'unsafe-inline'",
      "script-src 'unsafe-inline'" },
    { "script-src 'unsafe-eval'",
      "script-src 'unsafe-eval'" },
    { "script-src 'unsafe-inline' 'unsafe-eval'",
      "script-src 'unsafe-inline' 'unsafe-eval'" },
    { "script-src 'none'",
      "script-src 'none'" },
    { "img-src 'none'; script-src 'unsafe-eval' 'unsafe-inline'; default-src 'self'",
      "img-src 'none'; script-src 'unsafe-eval' 'unsafe-inline'; default-src http://www.selfuri.com" },
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestIgnoreUpperLowerCasePolicies ========================

nsresult TestIgnoreUpperLowerCasePolicies() {

  static const PolicyTest policies[] =
  {
    { "script-src 'SELF'",
      "script-src http://www.selfuri.com" },
    { "sCriPt-src 'Unsafe-Inline'",
      "script-src 'unsafe-inline'" },
    { "SCRIPT-src 'unsafe-eval'",
      "script-src 'unsafe-eval'" },
    { "default-SRC 'unsafe-inline' 'unsafe-eval'",
      "default-src 'unsafe-inline' 'unsafe-eval'" },
    { "script-src 'NoNe'",
      "script-src 'none'" },
    { "img-sRc 'noNe'; scrIpt-src 'unsafe-EVAL' 'UNSAFE-inline'; deFAULT-src 'Self'",
      "img-src 'none'; script-src 'unsafe-eval' 'unsafe-inline'; default-src http://www.selfuri.com" },
    { "default-src HTTP://www.example.com",
      "default-src http://www.example.com" },
    { "default-src HTTP://WWW.EXAMPLE.COM",
      "default-src http://www.example.com" },
    { "default-src HTTPS://*.example.COM",
      "default-src https://*.example.com" },
    { "script-src 'none' test.com;",
      "script-src http://test.com" },
    { "script-src 'NoNCE-correctscriptnonce'",
      "script-src 'nonce-correctscriptnonce'" },
    { "script-src 'NoncE-NONCENEEDSTOBEUPPERCASE'",
      "script-src 'nonce-NONCENEEDSTOBEUPPERCASE'" },
    { "script-src 'SHA256-siVR8vAcqP06h2ppeNwqgjr0yZ6yned4X2VF84j4GmI='",
      "script-src 'sha256-siVR8vAcqP06h2ppeNwqgjr0yZ6yned4X2VF84j4GmI='" }
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestPaths ========================

nsresult TestPaths() {

  static const PolicyTest policies[] =
  {
    { "script-src http://www.example.com",
      "script-src http://www.example.com" },
    { "script-src http://www.example.com/",
      "script-src http://www.example.com/" },
    { "script-src http://www.example.com/path-1",
      "script-src http://www.example.com/path-1" },
    { "script-src http://www.example.com/path-1/",
      "script-src http://www.example.com/path-1/" },
    { "script-src http://www.example.com/path-1/path_2",
      "script-src http://www.example.com/path-1/path_2" },
    { "script-src http://www.example.com/path-1/path_2/",
      "script-src http://www.example.com/path-1/path_2/" },
    { "script-src http://www.example.com/path-1/path_2/file.js",
      "script-src http://www.example.com/path-1/path_2/file.js" },
    { "script-src http://www.example.com/path-1/path_2/file_1.js",
      "script-src http://www.example.com/path-1/path_2/file_1.js" },
    { "script-src http://www.example.com/path-1/path_2/file-2.js",
      "script-src http://www.example.com/path-1/path_2/file-2.js" },
    { "script-src http://www.example.com/path-1/path_2/f.js",
      "script-src http://www.example.com/path-1/path_2/f.js" },
    { "script-src http://www.example.com:88",
      "script-src http://www.example.com:88" },
    { "script-src http://www.example.com:88/",
      "script-src http://www.example.com:88/" },
    { "script-src http://www.example.com:88/path-1",
      "script-src http://www.example.com:88/path-1" },
    { "script-src http://www.example.com:88/path-1/",
      "script-src http://www.example.com:88/path-1/" },
    { "script-src http://www.example.com:88/path-1/path_2",
      "script-src http://www.example.com:88/path-1/path_2" },
    { "script-src http://www.example.com:88/path-1/path_2/",
      "script-src http://www.example.com:88/path-1/path_2/" },
    { "script-src http://www.example.com:88/path-1/path_2/file.js",
      "script-src http://www.example.com:88/path-1/path_2/file.js" },
    { "script-src http://www.example.com:*",
      "script-src http://www.example.com:*" },
    { "script-src http://www.example.com:*/",
      "script-src http://www.example.com:*/" },
    { "script-src http://www.example.com:*/path-1",
      "script-src http://www.example.com:*/path-1" },
    { "script-src http://www.example.com:*/path-1/",
      "script-src http://www.example.com:*/path-1/" },
    { "script-src http://www.example.com:*/path-1/path_2",
      "script-src http://www.example.com:*/path-1/path_2" },
    { "script-src http://www.example.com:*/path-1/path_2/",
      "script-src http://www.example.com:*/path-1/path_2/" },
    { "script-src http://www.example.com:*/path-1/path_2/file.js",
      "script-src http://www.example.com:*/path-1/path_2/file.js" },
    { "script-src http://www.example.com#foo",
      "script-src http://www.example.com" },
    { "script-src http://www.example.com?foo=bar",
      "script-src http://www.example.com" },
    { "script-src http://www.example.com:8888#foo",
      "script-src http://www.example.com:8888" },
    { "script-src http://www.example.com:8888?foo",
      "script-src http://www.example.com:8888" },
    { "script-src http://www.example.com/#foo",
      "script-src http://www.example.com/" },
    { "script-src http://www.example.com/?foo",
      "script-src http://www.example.com/" },
    { "script-src http://www.example.com/path-1/file.js#foo",
      "script-src http://www.example.com/path-1/file.js" },
    { "script-src http://www.example.com/path-1/file.js?foo",
      "script-src http://www.example.com/path-1/file.js" },
    { "script-src http://www.example.com/path-1/file.js?foo#bar",
      "script-src http://www.example.com/path-1/file.js" },
    { "report-uri http://www.example.com/",
      "report-uri http://www.example.com/" },
    { "report-uri http://www.example.com:8888/asdf",
      "report-uri http://www.example.com:8888/asdf" },
    { "report-uri http://www.example.com:8888/path_1/path_2",
      "report-uri http://www.example.com:8888/path_1/path_2" },
    { "report-uri http://www.example.com:8888/path_1/path_2/report.sjs&301",
      "report-uri http://www.example.com:8888/path_1/path_2/report.sjs&301" },
    { "report-uri /examplepath",
      "report-uri http://www.selfuri.com/examplepath" },
    { "connect-src http://www.example.com/foo%3Bsessionid=12%2C34",
      "connect-src http://www.example.com/foo;sessionid=12,34" },
    { "connect-src http://www.example.com/foo%3bsessionid=12%2c34",
      "connect-src http://www.example.com/foo;sessionid=12,34" },
    { "connect-src http://test.com/pathIncludingAz19-._~!$&'()*+=:@",
      "connect-src http://test.com/pathIncludingAz19-._~!$&'()*+=:@" },
    { "script-src http://www.example.com:88/.js",
      "script-src http://www.example.com:88/.js" },
    { "script-src https://foo.com/_abc/abc_/_/_a_b_c_",
      "script-src https://foo.com/_abc/abc_/_/_a_b_c_" }
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestSimplePolicies ========================

nsresult TestSimplePolicies() {

  static const PolicyTest policies[] =
  {
    { "default-src *",
      "default-src *" },
    { "default-src https:",
      "default-src https:" },
    { "default-src https://*",
      "default-src https://*" },
    { "default-src *:*",
      "default-src http://*:*" },
    { "default-src *:80",
      "default-src http://*:80" },
    { "default-src http://*:80",
      "default-src http://*:80" },
    { "default-src javascript:",
      "default-src javascript:" },
    { "default-src data:",
      "default-src data:" },
    { "script-src 'unsafe-eval' 'unsafe-inline' http://www.example.com",
      "script-src 'unsafe-eval' 'unsafe-inline' http://www.example.com" },
    { "object-src 'self'",
      "object-src http://www.selfuri.com" },
    { "style-src http://www.example.com 'self'",
      "style-src http://www.example.com http://www.selfuri.com" },
    { "media-src http://www.example.com http://www.test.com",
      "media-src http://www.example.com http://www.test.com" },
    { "connect-src http://www.test.com example.com *.other.com;",
      "connect-src http://www.test.com http://example.com http://*.other.com"},
    { "connect-src example.com *.other.com",
      "connect-src http://example.com http://*.other.com"},
    { "style-src *.other.com example.com",
      "style-src http://*.other.com http://example.com"},
    { "default-src 'self'; img-src *;",
      "default-src http://www.selfuri.com; img-src *" },
    { "object-src media1.example.com media2.example.com *.cdn.example.com;",
      "object-src http://media1.example.com http://media2.example.com http://*.cdn.example.com" },
    { "script-src trustedscripts.example.com",
      "script-src http://trustedscripts.example.com" },
    { "script-src 'self' ; default-src trustedscripts.example.com",
      "script-src http://www.selfuri.com; default-src http://trustedscripts.example.com" },
    { "default-src 'none'; report-uri http://localhost:49938/test",
      "default-src 'none'; report-uri http://localhost:49938/test" },
    { "default-src app://{app-host-is-uid}",
      "default-src app://{app-host-is-uid}" },
    { "   ;   default-src abc",
      "default-src http://abc" },
    { " ; ; ; ;     default-src            abc    ; ; ; ;",
      "default-src http://abc" },
    { "script-src 'none' 'none' 'none';",
      "script-src 'none'" },
    { "script-src http://www.example.com/path-1//",
      "script-src http://www.example.com/path-1//" },
    { "script-src http://www.example.com/path-1//path_2",
      "script-src http://www.example.com/path-1//path_2" },
    { "default-src 127.0.0.1",
      "default-src http://127.0.0.1" },
    { "default-src 127.0.0.1:*",
      "default-src http://127.0.0.1:*" },
    { "default-src -; ",
      "default-src http://-" },
    { "script-src 1",
      "script-src http://1" }
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestPoliciesWithInvalidSrc ========================

nsresult TestPoliciesWithInvalidSrc() {

  static const PolicyTest policies[] =
  {
    { "script-src 'self'; SCRIPT-SRC http://www.example.com",
      "script-src http://www.selfuri.com" },
    { "script-src 'none' test.com; script-src example.com",
      "script-src http://test.com" },
    { "default-src **",
      "default-src 'none'" },
    { "default-src 'self",
      "default-src 'none'" },
    { "default-src 'unsafe-inlin' ",
      "default-src 'none'" },
    { "default-src */",
      "default-src 'none'" },
    { "default-src",
      "default-src 'none'" },
    { "default-src 'unsafe-inlin' ",
      "default-src 'none'" },
    { "default-src :88",
      "default-src 'none'" },
    { "script-src abc::::::88",
      "script-src 'none'" },
    { "script-src *.*:*",
      "script-src 'none'" },
    { "img-src *::88",
      "img-src 'none'" },
    { "object-src http://localhost:",
      "object-src 'none'" },
    { "script-src test..com",
      "script-src 'none'" },
    { "script-src sub1.sub2.example+",
      "script-src 'none'" },
    { "script-src http://www.example.com//",
      "script-src 'none'" },
    { "script-src http://www.example.com:88path-1/",
      "script-src 'none'" },
    { "script-src http://www.example.com:88//",
      "script-src 'none'" },
    { "script-src http://www.example.com:88//path-1",
      "script-src 'none'" },
    { "script-src http://www.example.com:88//path-1",
      "script-src 'none'" },
    { "script-src http://www.example.com:88.js",
      "script-src 'none'" },
    { "script-src http://www.example.com:*.js",
      "script-src 'none'" },
    { "script-src http://www.example.com:*.",
      "script-src 'none'" },
    { "connect-src http://www.example.com/foo%zz;",
      "connect-src 'none'" },
    { "script-src https://foo.com/%$",
      "script-src 'none'" },
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestBadPolicies ========================

nsresult TestBadPolicies() {

  static const PolicyTest policies[] =
  {
    { "script-sr 'self", "" },
    { "", "" },
    { "; ; ; ; ; ; ;", "" },
    { "defaut-src asdf", "" },
    { "default-src: aaa", "" },
    { "asdf http://test.com", ""},
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 0);
}

// ============================= TestGoodGeneratedPolicies ========================

nsresult TestGoodGeneratedPolicies() {

  static const PolicyTest policies[] =
  {
    { "default-src 'self'; img-src *",
      "default-src http://www.selfuri.com; img-src *" },
    { "report-uri /policy",
      "report-uri http://www.selfuri.com/policy"},
    { "img-src *",
      "img-src *" },
    { "media-src foo.bar",
      "media-src http://foo.bar" },
    { "frame-src *.bar",
      "frame-src http://*.bar" },
    { "font-src com",
      "font-src http://com" },
    { "connect-src f00b4r.com",
      "connect-src http://f00b4r.com" },
    { "default-src {app-url-is-uid}",
      "default-src http://{app-url-is-uid}" },
    { "script-src *.a.b.c",
      "script-src http://*.a.b.c" },
    { "object-src *.b.c",
      "object-src http://*.b.c" },
    { "style-src a.b.c",
      "style-src http://a.b.c" },
    { "img-src a.com",
      "img-src http://a.com" },
    { "media-src http://abc.com",
      "media-src http://abc.com" },
    { "frame-src a2-c.com",
      "frame-src http://a2-c.com" },
    { "font-src https://a.com",
      "font-src https://a.com" },
    { "connect-src *.a.com",
      "connect-src http://*.a.com" },
    { "default-src a.com:23",
      "default-src http://a.com:23" },
    { "script-src https://a.com:200",
      "script-src https://a.com:200" },
    { "object-src data:",
      "object-src data:" },
    { "style-src javascript:",
      "style-src javascript:" },
    { "img-src {app-host-is-uid}",
      "img-src http://{app-host-is-uid}" },
    { "media-src app://{app-host-is-uid}",
      "media-src app://{app-host-is-uid}" },
    { "frame-src https://foobar.com:443",
      "frame-src https://foobar.com:443" },
    { "font-src https://a.com:443",
      "font-src https://a.com:443" },
    { "connect-src http://a.com:80",
      "connect-src http://a.com:80" },
    { "default-src http://foobar.com",
      "default-src http://foobar.com" },
    { "script-src https://foobar.com",
      "script-src https://foobar.com" },
    { "object-src https://{app-host-is-uid}",
      "object-src https://{app-host-is-uid}" },
    { "style-src 'none'",
      "style-src 'none'" },
    { "img-src foo.bar:21 https://ras.bar",
      "img-src http://foo.bar:21 https://ras.bar" },
    { "media-src http://foo.bar:21 https://ras.bar:443",
      "media-src http://foo.bar:21 https://ras.bar:443" },
    { "frame-src http://self.com:80",
      "frame-src http://self.com:80" },
    { "font-src http://self.com",
      "font-src http://self.com" },
    { "connect-src https://foo.com http://bar.com:88",
      "connect-src https://foo.com http://bar.com:88" },
    { "default-src * https://bar.com 'none'",
      "default-src * https://bar.com" },
    { "script-src *.foo.com",
      "script-src http://*.foo.com" },
    { "object-src http://b.com",
      "object-src http://b.com" },
    { "style-src http://bar.com:88",
      "style-src http://bar.com:88" },
    { "img-src https://bar.com:88",
      "img-src https://bar.com:88" },
    { "media-src http://bar.com:443",
      "media-src http://bar.com:443" },
    { "frame-src https://foo.com:88",
      "frame-src https://foo.com:88" },
    { "font-src http://foo.com",
      "font-src http://foo.com" },
    { "connect-src http://x.com:23",
      "connect-src http://x.com:23" },
    { "default-src http://barbaz.com",
      "default-src http://barbaz.com" },
    { "script-src http://somerandom.foo.com",
      "script-src http://somerandom.foo.com" },
    { "default-src *",
      "default-src *" },
    { "style-src http://bar.com:22",
      "style-src http://bar.com:22" },
    { "img-src https://foo.com:443",
      "img-src https://foo.com:443" },
    { "script-src https://foo.com; ",
      "script-src https://foo.com" },
    { "img-src bar.com:*",
      "img-src http://bar.com:*" },
    { "font-src https://foo.com:400",
      "font-src https://foo.com:400" },
    { "connect-src http://bar.com:400",
      "connect-src http://bar.com:400" },
    { "default-src http://evil.com",
      "default-src http://evil.com" },
    { "script-src https://evil.com:100",
      "script-src https://evil.com:100" },
    { "default-src bar.com; script-src https://foo.com",
      "default-src http://bar.com; script-src https://foo.com" },
    { "default-src 'self'; script-src 'self' https://*:*",
      "default-src http://www.selfuri.com; script-src http://www.selfuri.com https://*:*" },
    { "img-src http://self.com:34",
      "img-src http://self.com:34" },
    { "media-src http://subd.self.com:34",
      "media-src http://subd.self.com:34" },
    { "default-src 'none'",
      "default-src 'none'" },
    { "connect-src http://self",
      "connect-src http://self" },
    { "default-src http://foo",
      "default-src http://foo" },
    { "script-src http://foo:80",
      "script-src http://foo:80" },
    { "object-src http://bar",
      "object-src http://bar" },
    { "style-src http://three:80",
      "style-src http://three:80" },
    { "img-src https://foo:400",
      "img-src https://foo:400" },
    { "media-src https://self:34",
      "media-src https://self:34" },
    { "frame-src https://bar",
      "frame-src https://bar" },
    { "font-src http://three:81",
      "font-src http://three:81" },
    { "connect-src https://three:81",
      "connect-src https://three:81" },
    { "script-src http://self.com:80/foo",
      "script-src http://self.com:80/foo" },
    { "object-src http://self.com/foo",
      "object-src http://self.com/foo" },
    { "report-uri /report.py",
      "report-uri http://www.selfuri.com/report.py"},
    { "img-src http://foo.org:34/report.py",
      "img-src http://foo.org:34/report.py" },
    { "media-src foo/bar/report.py",
      "media-src http://foo/bar/report.py" },
    { "report-uri /",
      "report-uri http://www.selfuri.com/"},
    { "font-src https://self.com/report.py",
      "font-src https://self.com/report.py" },
    { "connect-src https://foo.com/report.py",
      "connect-src https://foo.com/report.py" },
    { "default-src *; report-uri  http://www.reporturi.com/",
      "default-src *; report-uri http://www.reporturi.com/" },
    { "default-src http://first.com",
      "default-src http://first.com" },
    { "script-src http://second.com",
      "script-src http://second.com" },
    { "object-src http://third.com",
      "object-src http://third.com" },
    { "style-src https://foobar.com:4443",
      "style-src https://foobar.com:4443" },
    { "img-src http://foobar.com:4443",
      "img-src http://foobar.com:4443" },
    { "media-src bar.com",
      "media-src http://bar.com" },
    { "frame-src http://bar.com",
      "frame-src http://bar.com" },
    { "font-src http://self.com/",
      "font-src http://self.com/" },
    { "script-src 'self'",
      "script-src http://www.selfuri.com" },
    { "default-src http://self.com/foo.png",
      "default-src http://self.com/foo.png" },
    { "script-src http://self.com/foo.js",
      "script-src http://self.com/foo.js" },
    { "object-src http://bar.com/foo.js",
      "object-src http://bar.com/foo.js" },
    { "style-src http://FOO.COM",
      "style-src http://foo.com" },
    { "img-src HTTP",
      "img-src http://http" },
    { "media-src http",
      "media-src http://http" },
    { "frame-src 'SELF'",
      "frame-src http://www.selfuri.com" },
    { "DEFAULT-src 'self';",
      "default-src http://www.selfuri.com" },
    { "default-src 'self' http://FOO.COM",
      "default-src http://www.selfuri.com http://foo.com" },
    { "default-src 'self' HTTP://foo.com",
      "default-src http://www.selfuri.com http://foo.com" },
    { "default-src 'NONE'",
      "default-src 'none'" },
    { "script-src policy-uri ",
      "script-src http://policy-uri" },
    { "img-src 'self'; ",
      "img-src http://www.selfuri.com" },
    { "frame-ancestors foo-bar.com",
      "frame-ancestors http://foo-bar.com" },
    { "frame-ancestors http://a.com",
      "frame-ancestors http://a.com" },
    { "frame-ancestors 'self'",
      "frame-ancestors http://www.selfuri.com" },
    { "frame-ancestors http://self.com:88",
      "frame-ancestors http://self.com:88" },
    { "frame-ancestors http://a.b.c.d.e.f.g.h.i.j.k.l.x.com",
      "frame-ancestors http://a.b.c.d.e.f.g.h.i.j.k.l.x.com" },
    { "frame-ancestors https://self.com:34",
      "frame-ancestors https://self.com:34" },
    { "default-src 'none'; frame-ancestors 'self'",
      "default-src 'none'; frame-ancestors http://www.selfuri.com" },
    { "frame-ancestors http://self:80",
      "frame-ancestors http://self:80" },
    { "frame-ancestors http://self.com/bar",
      "frame-ancestors http://self.com/bar" },
    { "default-src 'self'; frame-ancestors 'self'",
      "default-src http://www.selfuri.com; frame-ancestors http://www.selfuri.com" },
    { "frame-ancestors http://bar.com/foo.png",
      "frame-ancestors http://bar.com/foo.png" },
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestBadGeneratedPolicies ========================

nsresult TestBadGeneratedPolicies() {

  static const PolicyTest policies[] =
  {
    { "foo.*.bar", ""},
    { "foo!bar.com", ""},
    { "x.*.a.com", ""},
    { "a#2-c.com", ""},
    { "http://foo.com:bar.com:23", ""},
    { "f!oo.bar", ""},
    { "ht!ps://f-oo.bar", ""},
    { "https://f-oo.bar:3f", ""},
    { "**", ""},
    { "*a", ""},
    { "http://username:password@self.com/foo", ""},
    { "http://other:pass1@self.com/foo", ""},
    { "http://user1:pass1@self.com/foo", ""},
    { "http://username:password@self.com/bar", ""},
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 0);
}

// ============ TestGoodGeneratedPoliciesForPathHandling ============

nsresult TestGoodGeneratedPoliciesForPathHandling() {
  // Once bug 808292 (Implement path-level host-source matching to CSP)
  // lands we have to update the expected output to include the parsed path

  static const PolicyTest policies[] =
  {
    { "img-src http://test1.example.com",
      "img-src http://test1.example.com" },
    { "img-src http://test1.example.com/",
      "img-src http://test1.example.com/" },
    { "img-src http://test1.example.com/path-1",
      "img-src http://test1.example.com/path-1" },
    { "img-src http://test1.example.com/path-1/",
      "img-src http://test1.example.com/path-1/" },
    { "img-src http://test1.example.com/path-1/path_2/",
      "img-src http://test1.example.com/path-1/path_2/" },
    { "img-src http://test1.example.com/path-1/path_2/file.js",
      "img-src http://test1.example.com/path-1/path_2/file.js" },
    { "img-src http://test1.example.com/path-1/path_2/file_1.js",
      "img-src http://test1.example.com/path-1/path_2/file_1.js" },
    { "img-src http://test1.example.com/path-1/path_2/file-2.js",
      "img-src http://test1.example.com/path-1/path_2/file-2.js" },
    { "img-src http://test1.example.com/path-1/path_2/f.js",
      "img-src http://test1.example.com/path-1/path_2/f.js" },
    { "img-src http://test1.example.com/path-1/path_2/f.oo.js",
      "img-src http://test1.example.com/path-1/path_2/f.oo.js" },
    { "img-src test1.example.com",
      "img-src http://test1.example.com" },
    { "img-src test1.example.com/",
      "img-src http://test1.example.com/" },
    { "img-src test1.example.com/path-1",
      "img-src http://test1.example.com/path-1" },
    { "img-src test1.example.com/path-1/",
      "img-src http://test1.example.com/path-1/" },
    { "img-src test1.example.com/path-1/path_2/",
      "img-src http://test1.example.com/path-1/path_2/" },
    { "img-src test1.example.com/path-1/path_2/file.js",
      "img-src http://test1.example.com/path-1/path_2/file.js" },
    { "img-src test1.example.com/path-1/path_2/file_1.js",
      "img-src http://test1.example.com/path-1/path_2/file_1.js" },
    { "img-src test1.example.com/path-1/path_2/file-2.js",
      "img-src http://test1.example.com/path-1/path_2/file-2.js" },
    { "img-src test1.example.com/path-1/path_2/f.js",
      "img-src http://test1.example.com/path-1/path_2/f.js" },
    { "img-src test1.example.com/path-1/path_2/f.oo.js",
      "img-src http://test1.example.com/path-1/path_2/f.oo.js" },
    { "img-src *.example.com",
      "img-src http://*.example.com" },
    { "img-src *.example.com/",
      "img-src http://*.example.com/" },
    { "img-src *.example.com/path-1",
      "img-src http://*.example.com/path-1" },
    { "img-src *.example.com/path-1/",
      "img-src http://*.example.com/path-1/" },
    { "img-src *.example.com/path-1/path_2/",
      "img-src http://*.example.com/path-1/path_2/" },
    { "img-src *.example.com/path-1/path_2/file.js",
      "img-src http://*.example.com/path-1/path_2/file.js" },
    { "img-src *.example.com/path-1/path_2/file_1.js",
      "img-src http://*.example.com/path-1/path_2/file_1.js" },
    { "img-src *.example.com/path-1/path_2/file-2.js",
      "img-src http://*.example.com/path-1/path_2/file-2.js" },
    { "img-src *.example.com/path-1/path_2/f.js",
      "img-src http://*.example.com/path-1/path_2/f.js" },
    { "img-src *.example.com/path-1/path_2/f.oo.js",
      "img-src http://*.example.com/path-1/path_2/f.oo.js" },
    { "img-src test1.example.com:80",
      "img-src http://test1.example.com:80" },
    { "img-src test1.example.com:80/",
      "img-src http://test1.example.com:80/" },
    { "img-src test1.example.com:80/path-1",
      "img-src http://test1.example.com:80/path-1" },
    { "img-src test1.example.com:80/path-1/",
      "img-src http://test1.example.com:80/path-1/" },
    { "img-src test1.example.com:80/path-1/path_2",
      "img-src http://test1.example.com:80/path-1/path_2" },
    { "img-src test1.example.com:80/path-1/path_2/",
      "img-src http://test1.example.com:80/path-1/path_2/" },
    { "img-src test1.example.com:80/path-1/path_2/file.js",
      "img-src http://test1.example.com:80/path-1/path_2/file.js" },
    { "img-src test1.example.com:80/path-1/path_2/f.ile.js",
      "img-src http://test1.example.com:80/path-1/path_2/f.ile.js" },
    { "img-src test1.example.com:*",
      "img-src http://test1.example.com:*" },
    { "img-src test1.example.com:*/",
      "img-src http://test1.example.com:*/" },
    { "img-src test1.example.com:*/path-1",
      "img-src http://test1.example.com:*/path-1" },
    { "img-src test1.example.com:*/path-1/",
      "img-src http://test1.example.com:*/path-1/" },
    { "img-src test1.example.com:*/path-1/path_2",
      "img-src http://test1.example.com:*/path-1/path_2" },
    { "img-src test1.example.com:*/path-1/path_2/",
      "img-src http://test1.example.com:*/path-1/path_2/" },
    { "img-src test1.example.com:*/path-1/path_2/file.js",
      "img-src http://test1.example.com:*/path-1/path_2/file.js" },
    { "img-src test1.example.com:*/path-1/path_2/f.ile.js",
      "img-src http://test1.example.com:*/path-1/path_2/f.ile.js" },
    { "img-src http://test1.example.com/abc//",
      "img-src http://test1.example.com/abc//" },
    { "img-src https://test1.example.com/abc/def//",
      "img-src https://test1.example.com/abc/def//" },
    { "img-src https://test1.example.com/abc/def/ghi//",
      "img-src https://test1.example.com/abc/def/ghi//" },
    { "img-src http://test1.example.com:80/abc//",
      "img-src http://test1.example.com:80/abc//" },
    { "img-src https://test1.example.com:80/abc/def//",
      "img-src https://test1.example.com:80/abc/def//" },
    { "img-src https://test1.example.com:80/abc/def/ghi//",
      "img-src https://test1.example.com:80/abc/def/ghi//" },
    { "img-src https://test1.example.com/abc////////////def/",
      "img-src https://test1.example.com/abc////////////def/" },
    { "img-src https://test1.example.com/abc////////////",
      "img-src https://test1.example.com/abc////////////" },
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============ TestBadGeneratedPoliciesForPathHandling ============

nsresult TestBadGeneratedPoliciesForPathHandling() {

  static const PolicyTest policies[] =
  {
    { "img-src test1.example.com:88path-1/",
      "img-src 'none'" },
    { "img-src test1.example.com:80.js",
      "img-src 'none'" },
    { "img-src test1.example.com:*.js",
      "img-src 'none'" },
    { "img-src test1.example.com:*.",
      "img-src 'none'" },
    { "img-src http://test1.example.com//",
      "img-src 'none'" },
    { "img-src http://test1.example.com:80//",
      "img-src 'none'" },
    { "img-src http://test1.example.com:80abc",
      "img-src 'none'" },
  };

  uint32_t policyCount = sizeof(policies) / sizeof(PolicyTest);
  return runTestSuite(policies, policyCount, 1);
}

// ============================= TestFuzzyPolicies ========================

// Use a policy, eliminate one character at a time,
// and feed it as input to the parser.

nsresult TestShorteningPolicies() {

  char pol[] = "default-src http://www.sub1.sub2.example.com:88/path1/path2/ 'unsafe-inline' 'none'";
  uint32_t len = static_cast<uint32_t>(sizeof(pol));

  PolicyTest testPol[1];
  memset(&testPol[0].policy, '\0', kMaxPolicyLength * sizeof(char));
  nsresult rv = NS_OK;

  while (--len) {
    memset(&testPol[0].policy, '\0', kMaxPolicyLength * sizeof(char));
    memcpy(&testPol[0].policy, &pol, len * sizeof(char));
    rv = runTestSuite(testPol, 1, kFuzzyExpectedPolicyCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

// ============================= TestFuzzyPolicies ========================

// We generate kFuzzyRuns inputs by (pseudo) randomly picking from the 128
// ASCII characters; feed them to the parser and verfy that the parser
// handles the input gracefully.
//
// Please note, that by using srand(0) we get deterministic results!

#if RUN_OFFLINE_TESTS

nsresult TestFuzzyPolicies() {

  // init srand with 0 so we get same results
  srand(0);

  PolicyTest testPol[1];
  memset(&testPol[0].policy, '\0', kMaxPolicyLength);
  nsresult rv = NS_OK;

  for (uint32_t index = 0; index < kFuzzyRuns; index++) {
    // randomly select the length of the next policy
    uint32_t polLength = rand() % kMaxPolicyLength;
    // reset memory of the policy string
    memset(&testPol[0].policy, '\0', kMaxPolicyLength * sizeof(char));

    for (uint32_t i = 0; i < polLength; i++) {
      // fill the policy array with random ASCII chars
      testPol[0].policy[i] = static_cast<char>(rand() % 128);
    }
    rv = runTestSuite(testPol, 1, kFuzzyExpectedPolicyCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

#endif
// ============================= TestFuzzyPoliciesIncDir ========================

// In a similar fashion as in TestFuzzyPolicies, we again (pseudo) randomly
// generate input for the parser, but this time also include a valid directive
// followed by the random input.

#if RUN_OFFLINE_TESTS

nsresult TestFuzzyPoliciesIncDir() {

  // init srand with 0 so we get same results
  srand(0);

  PolicyTest testPol[1];
  memset(&testPol[0].policy, '\0', kMaxPolicyLength);
  nsresult rv = NS_OK;

  char defaultSrc[] = "default-src ";
  int defaultSrcLen = sizeof(defaultSrc) - 1;
  // copy default-src into the policy array
  memcpy(&testPol[0].policy, &defaultSrc, (defaultSrcLen * sizeof(char)));

  for (uint32_t index = 0; index < kFuzzyRuns; index++) {
    // randomly select the length of the next policy
    uint32_t polLength = rand() % (kMaxPolicyLength - defaultSrcLen);
    // reset memory of the policy string, but leave default-src.
    memset((&(testPol[0].policy) + (defaultSrcLen * sizeof(char))),
           '\0', (kMaxPolicyLength - defaultSrcLen) * sizeof(char));

    // do not start at index 0 so we do not overwrite 'default-src'
    for (uint32_t i = defaultSrcLen; i < polLength; i++) {
      // fill the policy array with random ASCII chars
      testPol[0].policy[i] = static_cast<char>(rand() % 128);
    }
    rv = runTestSuite(testPol, 1, kFuzzyExpectedPolicyCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

#endif

// ============================= TestFuzzyPoliciesIncDirLimASCII ==============

// Same as TestFuzzyPoliciesIncDir() but using limited ASCII,
// which represents more likely input.

#if RUN_OFFLINE_TESTS

nsresult TestFuzzyPoliciesIncDirLimASCII() {

  char input[] = "1234567890" \
                 "abcdefghijklmnopqrstuvwxyz" \
                 "ABCDEFGHIJKLMNOPQRSTUVWZYZ" \
                 "!@#^&*()-+_=";

  // init srand with 0 so we get same results
  srand(0);

  PolicyTest testPol[1];
  memset(&testPol[0].policy, '\0', kMaxPolicyLength);
  nsresult rv = NS_OK;

  char defaultSrc[] = "default-src ";
  int defaultSrcLen = sizeof(defaultSrc) - 1;
  // copy default-src into the policy array
  memcpy(&testPol[0].policy, &defaultSrc, (defaultSrcLen * sizeof(char)));

  for (uint32_t index = 0; index < kFuzzyRuns; index++) {
    // randomly select the length of the next policy
    uint32_t polLength = rand() % (kMaxPolicyLength - defaultSrcLen);
    // reset memory of the policy string, but leave default-src.
    memset((&(testPol[0].policy) + (defaultSrcLen * sizeof(char))),
           '\0', (kMaxPolicyLength - defaultSrcLen) * sizeof(char));

    // do not start at index 0 so we do not overwrite 'default-src'
    for (uint32_t i = defaultSrcLen; i < polLength; i++) {
      // fill the policy array with chars from the pre-defined input
      uint32_t inputIndex = rand() % sizeof(input);
      testPol[0].policy[i] = input[inputIndex];
    }
    rv = runTestSuite(testPol, 1, kFuzzyExpectedPolicyCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}
#endif

// ============================= Run the tests ========================

int main(int argc, char** argv) {

  ScopedXPCOM xpcom("ContentSecurityPolicyParser");
  if (xpcom.failed()) {
    return 1;
  }

  if (NS_FAILED(TestDirectives()))                           { return 1; }
  if (NS_FAILED(TestKeywords()))                             { return 1; }
  if (NS_FAILED(TestIgnoreUpperLowerCasePolicies()))         { return 1; }
  if (NS_FAILED(TestPaths()))                                { return 1; }
  if (NS_FAILED(TestSimplePolicies()))                       { return 1; }
  if (NS_FAILED(TestPoliciesWithInvalidSrc()))               { return 1; }
  if (NS_FAILED(TestBadPolicies()))                          { return 1; }
  if (NS_FAILED(TestGoodGeneratedPolicies()))                { return 1; }
  if (NS_FAILED(TestBadGeneratedPolicies()))                 { return 1; }
  if (NS_FAILED(TestGoodGeneratedPoliciesForPathHandling())) { return 1; }
  if (NS_FAILED(TestBadGeneratedPoliciesForPathHandling()))  { return 1; }
  if (NS_FAILED(TestShorteningPolicies()))                   { return 1; }

#if RUN_OFFLINE_TESTS
  if (NS_FAILED(TestFuzzyPolicies()))                        { return 1; }
  if (NS_FAILED(TestFuzzyPoliciesIncDir()))                  { return 1; }
  if (NS_FAILED(TestFuzzyPoliciesIncDirLimASCII()))          { return 1; }
#endif

  return 0;
}
