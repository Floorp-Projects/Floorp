/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include <string>
#include <vector>

#ifdef _MSC_VER
// On MSVC otherwise our generic member pointer trick doesn't work.
#pragma pointers_to_members(full_generality, single_inheritance)
#endif

#define VERIFY(arg) if (!(arg)) { \
  LogMessage("VERIFY FAILED: "#arg"\n"); \
  mTestFailed = true; \
  }

#define REGISTER_TEST(className, testName) \
  mTests.push_back(Test(static_cast<TestCall>(&className::testName), #testName, this))

class TestBase
{
public:
  TestBase() {}

  typedef void (TestBase::*TestCall)();

  int RunTests(int *aFailures);

protected:
  static void LogMessage(std::string aMessage);

  struct Test {
    Test(TestCall aCall, std::string aName, void *aImplPointer)
      : funcCall(aCall)
      , name(aName)
      , implPointer(aImplPointer)
    {
    }
    TestCall funcCall;
    std::string name;
    void *implPointer;
  };
  std::vector<Test> mTests;

  bool mTestFailed;

private:
  // This doesn't really work with our generic member pointer trick.
  TestBase(const TestBase &aOther);
};
