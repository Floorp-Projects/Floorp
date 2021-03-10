/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleDocument_h_
#define mozilla_a11y_xpcAccessibleDocument_h_

#include "nsIAccessibleDocument.h"

#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "xpcAccessibleApplication.h"
#include "xpcAccessibleHyperText.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around DocAccessible class.
 */
class xpcAccessibleDocument : public xpcAccessibleHyperText,
                              public nsIAccessibleDocument {
 public:
  explicit xpcAccessibleDocument(DocAccessible* aIntl)
      : xpcAccessibleHyperText(aIntl),
        mCache(kDefaultCacheLength),
        mRemote(false) {}

  xpcAccessibleDocument(RemoteAccessible* aProxy, uint32_t aInterfaces)
      : xpcAccessibleHyperText(aProxy, aInterfaces),
        mCache(kDefaultCacheLength),
        mRemote(true) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleDocument
  NS_IMETHOD GetURL(nsAString& aURL) final;
  NS_IMETHOD GetTitle(nsAString& aTitle) final;
  NS_IMETHOD GetMimeType(nsAString& aType) final;
  NS_IMETHOD GetDocType(nsAString& aType) final;
  NS_IMETHOD GetDOMDocument(dom::Document** aDOMDocument) final;
  NS_IMETHOD GetWindow(mozIDOMWindowProxy** aDOMWindow) final;
  NS_IMETHOD GetParentDocument(nsIAccessibleDocument** aDocument) final;
  NS_IMETHOD GetChildDocumentCount(uint32_t* aCount) final;
  NS_IMETHOD GetChildDocumentAt(uint32_t aIndex,
                                nsIAccessibleDocument** aDocument) final;
  NS_IMETHOD GetVirtualCursor(nsIAccessiblePivot** aVirtualCursor) final;

  /**
   * Return XPCOM wrapper for the internal accessible.
   */
  xpcAccessibleGeneric* GetAccessible(Accessible* aAccessible);

  virtual void Shutdown() override;

 protected:
  virtual ~xpcAccessibleDocument() {}

 private:
  DocAccessible* Intl() {
    if (LocalAccessible* acc = mIntl->AsLocal()) {
      return acc->AsDoc();
    }

    return nullptr;
  }

  void NotifyOfShutdown(Accessible* aAccessible) {
    xpcAccessibleGeneric* xpcAcc = mCache.Get(aAccessible);
    if (xpcAcc) {
      xpcAcc->Shutdown();
    }

    mCache.Remove(aAccessible);
    if (mCache.Count() == 0 && mRefCnt == 1) {
      if (mIntl->IsLocal()) {
        GetAccService()->RemoveFromXPCDocumentCache(mIntl->AsLocal()->AsDoc());
      } else {
        GetAccService()->RemoveFromRemoteXPCDocumentCache(
            mIntl->AsRemote()->AsDoc());
      }
    }
  }

  friend class DocManager;
  friend class DocAccessible;
  friend class RemoteAccessible;
  friend class RemoteAccessibleBase<RemoteAccessible>;
  friend class xpcAccessibleGeneric;

  xpcAccessibleDocument(const xpcAccessibleDocument&) = delete;
  xpcAccessibleDocument& operator=(const xpcAccessibleDocument&) = delete;

  nsTHashMap<nsPtrHashKey<const void>, xpcAccessibleGeneric*> mCache;
  bool mRemote;
};

inline xpcAccessibleGeneric* ToXPC(Accessible* aAccessible) {
  if (!aAccessible) return nullptr;

  if (aAccessible->IsApplication()) return XPCApplicationAcc();

  xpcAccessibleDocument* xpcDoc =
      aAccessible->IsLocal()
          ? GetAccService()->GetXPCDocument(aAccessible->AsLocal()->Document())
          : GetAccService()->GetXPCDocument(
                aAccessible->AsRemote()->Document());
  return xpcDoc ? xpcDoc->GetAccessible(aAccessible) : nullptr;
}

inline xpcAccessibleHyperText* ToXPCText(HyperTextAccessible* aAccessible) {
  if (!aAccessible) return nullptr;

  xpcAccessibleDocument* xpcDoc =
      GetAccService()->GetXPCDocument(aAccessible->Document());
  return static_cast<xpcAccessibleHyperText*>(
      xpcDoc->GetAccessible(aAccessible));
}

inline xpcAccessibleDocument* ToXPCDocument(DocAccessible* aAccessible) {
  return GetAccService()->GetXPCDocument(aAccessible);
}

inline xpcAccessibleDocument* ToXPCDocument(DocAccessibleParent* aAccessible) {
  return GetAccService()->GetXPCDocument(aAccessible);
}

}  // namespace a11y
}  // namespace mozilla

#endif
