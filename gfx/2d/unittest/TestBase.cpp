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
