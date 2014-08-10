/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"

struct JSContext;

namespace mozilla {
namespace dom {

class Animation MOZ_FINAL : public nsWrapperCache
{
public:
  explicit Animation(nsIDocument* aDocument)
    : mDocument(aDocument)
  {
    SetIsDOMBinding();
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(Animation)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(Animation)

  nsIDocument* GetParentObject() const { return mDocument; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

protected:
  virtual ~Animation() { }

  // We use a document for a parent object since the other likely candidate,
  // the target element, can be empty.
  nsRefPtr<nsIDocument> mDocument;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Animation_h
