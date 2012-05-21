/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestBase.h"

#include <sstream>

using namespace std;

int
TestBase::RunTests(int *aFailures)
{
  int testsRun = 0;
  *aFailures = 0;

  for(unsigned int i = 0; i < mTests.size(); i++) {
    stringstream stream;
    stream << "Test (" << mTests[i].name << "): ";
    LogMessage(stream.str());
    stream.str("");

    mTestFailed = false;

    // Don't try this at home! We know these are actually pointers to members
    // of child clases, so we reinterpret cast those child class pointers to
    // TestBase and then call the functions. Because the compiler believes
    // these function calls are members of TestBase.
    ((*reinterpret_cast<TestBase*>((mTests[i].implPointer))).*(mTests[i].funcCall))();

    if (!mTestFailed) {
      LogMessage("PASSED\n");
    } else {
      LogMessage("FAILED\n");
      (*aFailures)++;
    }
    testsRun++;
  }

  return testsRun;
}

void
TestBase::LogMessage(string aMessage)
{
  printf(aMessage.c_str());
}
