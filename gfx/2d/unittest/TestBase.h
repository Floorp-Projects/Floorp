/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
