/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/COMPtrTypes.h"
#include "mozilla/a11y/DocAccessibleChildBase.h"
#include "mozilla/mscom/Ptr.h"

namespace mozilla {
namespace a11y {

/*
 * These objects handle content side communication for an accessible document,
 * and their lifetime is the same as the document they represent.
 */
class DocAccessibleChild : public DocAccessibleChildBase
{
public:
  explicit DocAccessibleChild(DocAccessible* aDoc);
  ~DocAccessibleChild();

  void SendCOMProxy(const IAccessibleHolder& aProxy);
  IAccessible* GetParentIAccessible() const { return mParentProxy.get(); }

private:
  mscom::ProxyUniquePtr<IAccessible> mParentProxy;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_DocAccessibleChild_h
