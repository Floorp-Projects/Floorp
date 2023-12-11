/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/StringifyUtils.h"

#include "gtest/gtest.h"

namespace mozilla::dom::quota {

class B;
class A : public Stringifyable {
 public:
  A() : mB(nullptr) {}
  virtual void DoStringify(nsACString& aData) override;
  void setB(B* aB) { mB = aB; }

 private:
  B* mB;
};

class B : public Stringifyable {
 public:
  B() : mA(nullptr) {}
  virtual void DoStringify(nsACString& aData) override;
  void setA(A* aA) { mA = aA; }

 private:
  A* mA;
};

void A::DoStringify(nsACString& aData) {
  aData.Append("Class A content."_ns);
  mB->Stringify(aData);
}

void B::DoStringify(nsACString& aData) {
  aData.Append("Class B content.");
  mA->Stringify(aData);
}

TEST(DOM_Quota_StringifyUtils, Nested)
{
  A a1;
  A a2;
  B b;
  a1.setB(&b);
  b.setA(&a2);
  a2.setB(&b);
  nsCString msg;
  a1.Stringify(msg);

  EXPECT_EQ(
      0, strcmp("{Class A content.{Class B content.{Class A content.(...)}}}",
                msg.get()));
}

}  // namespace mozilla::dom::quota
