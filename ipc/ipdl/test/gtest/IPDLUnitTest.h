/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTest_h
#define mozilla__ipdltest_IPDLUnitTest_h

#include "mozilla/_ipdltest/IPDLUnitTestChild.h"
#include "mozilla/_ipdltest/IPDLUnitTestParent.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla::_ipdltest {

// Should be called from static constructors to register a child actor
// constructor so that it can be called from the child process.
const char* RegisterAllocChildActor(
    const char* aName, mozilla::ipc::IToplevelProtocol* (*aFunc)());

// Internal helper type used to declare IPDL tests.
class IPDLTestHelper {
 public:
  void TestWrapper(bool aCrossProcess);
  virtual const char* GetName() = 0;
  virtual ipc::IToplevelProtocol* GetActor() = 0;
  virtual void TestBody() = 0;
};

#define IPDL_TEST_CLASS_NAME_(actorname) IPDL_TEST_##actorname

#define IPDL_TEST_HEAD_(actorname)                                        \
  class IPDL_TEST_CLASS_NAME_(actorname)                                  \
      : public ::mozilla::_ipdltest::IPDLTestHelper {                     \
   public:                                                                \
    IPDL_TEST_CLASS_NAME_(actorname)() : mActor(new actorname##Parent) {} \
                                                                          \
   private:                                                               \
    void TestBody() override;                                             \
    const char* GetName() override { return sName; };                     \
    actorname##Parent* GetActor() override { return mActor; };            \
                                                                          \
    RefPtr<actorname##Parent> mActor;                                     \
    static const char* sName;                                             \
  };                                                                      \
  const char* IPDL_TEST_CLASS_NAME_(actorname)::sName =                   \
      ::mozilla::_ipdltest::RegisterAllocChildActor(                      \
          #actorname, []() -> ::mozilla::ipc::IToplevelProtocol* {        \
            return new actorname##Child;                                  \
          });

#define IPDL_TEST_DECL_(testgroup, actorname, crossprocess) \
  TEST(testgroup, actorname)                                \
  {                                                         \
    IPDL_TEST_CLASS_NAME_(actorname) test;                  \
    test.TestWrapper(crossprocess);                         \
  }

#define IPDL_TEST_BODY_SEGUE_(actorname) \
  void IPDL_TEST_CLASS_NAME_(actorname)::TestBody()

// Declare a basic IPDL unit test which will be run in both both cross-thread
// and cross-process configurations. The actor `actorname` will be instantiated
// by default-constructing the parent & child actors in the relevant processes,
// and the test block will be executed, with `mActor` being a pointer to the
// parent actor. The test is asynchronous, and will end when the IPDL connection
// between the created actors is destroyed.
//
// GTest assertions fired in the child process will be relayed to the parent
// process, and should generally function correctly.
#define IPDL_TEST(actorname)                              \
  IPDL_TEST_HEAD_(actorname)                              \
  IPDL_TEST_DECL_(IPDLTest_CrossProcess, actorname, true) \
  IPDL_TEST_DECL_(IPDLTest_CrossThread, actorname, false) \
  IPDL_TEST_BODY_SEGUE_(actorname)

}  // namespace mozilla::_ipdltest

#endif  // mozilla__ipdltest_IPDLUnitTest_h
