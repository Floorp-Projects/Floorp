/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsStructuredCloneContainer_h__
#define nsStructuredCloneContainer_h__

#include "nsIStructuredCloneContainer.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"

#define NS_STRUCTUREDCLONECONTAINER_CONTRACTID \
  "@mozilla.org/docshell/structured-clone-container;1"
#define NS_STRUCTUREDCLONECONTAINER_CID \
{ /* 38bd0634-0fd4-46f0-b85f-13ced889eeec */       \
  0x38bd0634,                                      \
  0x0fd4,                                          \
  0x46f0,                                          \
  {0xb8, 0x5f, 0x13, 0xce, 0xd8, 0x89, 0xee, 0xec} \
}

class nsStructuredCloneContainer final
  : public nsIStructuredCloneContainer
  , public mozilla::dom::ipc::StructuredCloneData
{
  public:
    nsStructuredCloneContainer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSISTRUCTUREDCLONECONTAINER

  private:
    ~nsStructuredCloneContainer();

    uint32_t mVersion;
};

#endif
