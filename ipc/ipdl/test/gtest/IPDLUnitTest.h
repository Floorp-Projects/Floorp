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

#define IPDL_TEST_CLASS_NAME_(actorname, ...) IPDL_TEST_##actorname##__VA_ARGS__

#define IPDL_TEST_HEAD_(actorname, ...)                                \
  class IPDL_TEST_CLASS_NAME_(actorname, ##__VA_ARGS__)                \
      : public ::mozilla::_ipdltest::IPDLTestHelper {                  \
   public:                                                             \
    IPDL_TEST_CLASS_NAME_(actorname, ##__VA_ARGS__)                    \
    () : mActor(new actorname##Parent) {}                              \
                                                                       \
   private:                                                            \
    void TestBody() override;                                          \
    const char* GetName() override { return sName; };                  \
    actorname##Parent* GetActor() override { return mActor; };         \
                                                                       \
    RefPtr<actorname##Parent> mActor;                                  \
    static const char* sName;                                          \
  };                                                                   \
  const char* IPDL_TEST_CLASS_NAME_(actorname, ##__VA_ARGS__)::sName = \
      ::mozilla::_ipdltest::RegisterAllocChildActor(                   \
          #actorname, []() -> ::mozilla::ipc::IToplevelProtocol* {     \
            return new actorname##Child;                               \
          });

#define IPDL_TEST_DECL_CROSSPROCESS_(actorname, ...)      \
  TEST(IPDLTest_CrossProcess, actorname##__VA_ARGS__)     \
  {                                                       \
    IPDL_TEST_CLASS_NAME_(actorname, ##__VA_ARGS__) test; \
    test.TestWrapper(true);                               \
  }

#define IPDL_TEST_DECL_CROSSTHREAD_(actorname, ...)       \
  TEST(IPDLTest_CrossThread, actorname##__VA_ARGS__)      \
  {                                                       \
    IPDL_TEST_CLASS_NAME_(actorname, ##__VA_ARGS__) test; \
    test.TestWrapper(false);                              \
  }

#define IPDL_TEST_DECL_ANY_(actorname, ...)              \
  IPDL_TEST_DECL_CROSSPROCESS_(actorname, ##__VA_ARGS__) \
  IPDL_TEST_DECL_CROSSTHREAD_(actorname, ##__VA_ARGS__)

#define IPDL_TEST_BODY_SEGUE_(actorname, ...) \
  void IPDL_TEST_CLASS_NAME_(actorname, ##__VA_ARGS__)::TestBody()

// Declare an IPDL unit test. The `mode` should be set to either `CROSSTHREAD`,
// `CROSSPROCESS`, or `ANY` to run in the selected configuration(s). The actor
// `actorname` will be instantiated by default-constructing the parent & child
// actors in the relevant processes, and the test block will be executed, with
// `mActor` being a pointer to the parent actor. The test is asynchronous, and
// will end when the IPDL connection between the created actors is destroyed.
// `aCrossProcess` will indicate whether the test is running cross-process
// (true) or cross-thread (false).
//
// You can optionally pass a single additional argument which provides a
// `testname`, which will be appended to `actorname` as the full GTest test
// name. It should be of the form usually passed to the `TEST` gtest macro (an
// identifier).
//
// GTest assertions fired in the child process will be relayed to the parent
// process, and should generally function correctly.
#define IPDL_TEST_ON(mode, actorname, ...)           \
  IPDL_TEST_HEAD_(actorname, ##__VA_ARGS__)          \
  IPDL_TEST_DECL_##mode##_(actorname, ##__VA_ARGS__) \
      IPDL_TEST_BODY_SEGUE_(actorname, ##__VA_ARGS__)

// Declare an IPDL unit test that will run in both cross-thread and
// cross-process configurations. See the documentation of IPDL_TEST_ON for more
// info.
#define IPDL_TEST(actorname, ...) IPDL_TEST_ON(ANY, actorname, ##__VA_ARGS__)

}  // namespace mozilla::_ipdltest

#endif  // mozilla__ipdltest_IPDLUnitTest_h
