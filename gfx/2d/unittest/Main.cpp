/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SanityChecks.h"
#include "TestPoint.h"
#include "TestScaling.h"
#include "TestBugs.h"
#ifdef WIN32
#include "TestDrawTargetD2D.h"
#endif

#include <string>
#include <sstream>

struct TestObject {
  TestBase *test;
  std::string name;
};


using namespace std;

int
main()
{
  TestObject tests[] = 
  {
    { new SanityChecks(), "Sanity Checks" },
  #ifdef WIN32
    { new TestDrawTargetD2D(), "DrawTarget (D2D)" },
  #endif
    { new TestPoint(), "Point Tests" },
    { new TestScaling(), "Scaling Tests" }
    { new TestBugs(), "Bug Tests" }
  };

  int totalFailures = 0;
  int totalTests = 0;
  stringstream message;
  printf("------ STARTING RUNNING TESTS ------\n");
  for (int i = 0; i < sizeof(tests) / sizeof(TestObject); i++) {
    message << "--- RUNNING TESTS: " << tests[i].name << " ---\n";
    printf(message.str().c_str());
    message.str("");
    int failures = 0;
    totalTests += tests[i].test->RunTests(&failures);
    totalFailures += failures;
    // Done with this test!
    delete tests[i].test;
  }
  message << "------ FINISHED RUNNING TESTS ------\nTests run: " << totalTests << " - Passes: " << totalTests - totalFailures << " - Failures: " << totalFailures << "\n";
  printf(message.str().c_str());
  return totalFailures;
}
