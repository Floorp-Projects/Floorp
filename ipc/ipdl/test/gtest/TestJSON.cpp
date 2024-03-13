/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test sending JSON(-like) objects over IPC.
 */

#include "gtest/gtest.h"

#include "mozilla/_ipdltest/IPDLUnitTest.h"
#include "mozilla/_ipdltest/PTestJSONChild.h"
#include "mozilla/_ipdltest/PTestJSONParent.h"
#include "mozilla/_ipdltest/PTestJSONHandleChild.h"
#include "mozilla/_ipdltest/PTestJSONHandleParent.h"

using namespace mozilla::ipc;

namespace mozilla::_ipdltest {

static nsString String(const char* const str) {
  return NS_ConvertUTF8toUTF16(str);
}

static void Array123(nsTArray<JSONVariant>& a123) {
  a123.AppendElement(1);
  a123.AppendElement(2);
  a123.AppendElement(3);

  ASSERT_EQ(a123, a123) << "operator== is broken";
}

template <class HandleT>
JSONVariant MakeTestVariant(HandleT* handle) {
  // In JS syntax:
  //
  //   return [
  //     undefined, null, true, 1.25, "test string",
  //     handle,
  //     [ 1, 2, 3 ],
  //     { "undefined" : undefined,
  //       "null"      : null,
  //       "true"      : true,
  //       "1.25"      : 1.25,
  //       "string"    : "string"
  //       "handle"    : handle,
  //       "array"     : [ 1, 2, 3 ]
  //     }
  //   ]
  //
  nsTArray<JSONVariant> outer;

  outer.AppendElement(void_t());
  outer.AppendElement(null_t());
  outer.AppendElement(true);
  outer.AppendElement(1.25);
  outer.AppendElement(String("test string"));

  outer.AppendElement(handle);

  nsTArray<JSONVariant> tmp;
  Array123(tmp);
  outer.AppendElement(tmp);

  nsTArray<KeyValue> obj;
  obj.AppendElement(KeyValue(String("undefined"), void_t()));
  obj.AppendElement(KeyValue(String("null"), null_t()));
  obj.AppendElement(KeyValue(String("true"), true));
  obj.AppendElement(KeyValue(String("1.25"), 1.25));
  obj.AppendElement(KeyValue(String("string"), String("value")));
  obj.AppendElement(KeyValue(String("handle"), handle));
  nsTArray<JSONVariant> tmp2;
  Array123(tmp2);
  obj.AppendElement(KeyValue(String("array"), tmp2));

  outer.AppendElement(obj);

  EXPECT_EQ(outer, outer) << "operator== is broken";

  return JSONVariant(outer);
}

//-----------------------------------------------------------------------------
// parent

class TestJSONHandleParent : public PTestJSONHandleParent {
  NS_INLINE_DECL_REFCOUNTING(TestJSONHandleParent, override)
 private:
  ~TestJSONHandleParent() = default;
};

class TestJSONHandleChild : public PTestJSONHandleChild {
  NS_INLINE_DECL_REFCOUNTING(TestJSONHandleChild, override)
 private:
  ~TestJSONHandleChild() = default;
};

class TestJSONParent : public PTestJSONParent {
  NS_INLINE_DECL_REFCOUNTING(TestJSONParent, override)
 private:
  IPCResult RecvTest(const JSONVariant& i, JSONVariant* o) final override {
    EXPECT_EQ(i, MakeTestVariant(mKid.get())) << "inparam mangled en route";

    *o = i;

    EXPECT_EQ(i, *o) << "operator== and/or copy assignment are broken";

    return IPC_OK();
  }

  already_AddRefed<PTestJSONHandleParent> AllocPTestJSONHandleParent()
      final override {
    auto handle = MakeRefPtr<TestJSONHandleParent>();
    EXPECT_FALSE(mKid);
    mKid = handle;
    return handle.forget();
  }

  ~TestJSONParent() = default;

  RefPtr<PTestJSONHandleParent> mKid;
};

//-----------------------------------------------------------------------------
// child

class TestJSONChild : public PTestJSONChild {
  NS_INLINE_DECL_REFCOUNTING(TestJSONChild, override)
 private:
  IPCResult RecvStart() final override {
    mKid = MakeRefPtr<TestJSONHandleChild>();
    EXPECT_TRUE(SendPTestJSONHandleConstructor(mKid)) << "sending Handle ctor";

    JSONVariant i(MakeTestVariant(mKid.get()));
    EXPECT_EQ(i, i) << "operator== is broken";
    EXPECT_EQ(i, MakeTestVariant(mKid.get())) << "copy ctor is broken";

    JSONVariant o;
    EXPECT_TRUE(SendTest(i, &o)) << "sending Test";

    EXPECT_EQ(i, o) << "round-trip mangled input data";
    EXPECT_EQ(o, MakeTestVariant(mKid.get())) << "outparam mangled en route";

    Close();
    return IPC_OK();
  }

  ~TestJSONChild() = default;

  RefPtr<PTestJSONHandleChild> mKid;
};

IPDL_TEST(TestJSON) { EXPECT_TRUE(mActor->SendStart()) << "sending Start"; }

}  // namespace mozilla::_ipdltest
