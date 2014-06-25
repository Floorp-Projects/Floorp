/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMImplementation_h
#define mozilla_dom_DOMImplementation_h

#include "nsIDOMDOMImplementation.h"
#include "nsWrapperCache.h"

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIURI.h"
#include "nsIWeakReferenceUtils.h"
#include "nsString.h"

class nsIDOMDocument;

namespace mozilla {
namespace dom {
class DocumentType;

class DOMImplementation MOZ_FINAL : public nsIDOMDOMImplementation
                                  , public nsWrapperCache
{
  ~DOMImplementation()
  {
  }

public:
  DOMImplementation(nsIDocument* aOwner,
                    nsIGlobalObject* aScriptObject,
                    nsIURI* aDocumentURI,
                    nsIURI* aBaseURI)
    : mOwner(aOwner)
    , mScriptObject(do_GetWeakReference(aScriptObject))
    , mDocumentURI(aDocumentURI)
    , mBaseURI(aBaseURI)
  {
    MOZ_ASSERT(aOwner);
    SetIsDOMBinding();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMImplementation)

  nsIDocument* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // nsIDOMDOMImplementation
  NS_DECL_NSIDOMDOMIMPLEMENTATION

  bool HasFeature(const nsAString& aFeature, const nsAString& aVersion);

  already_AddRefed<DocumentType>
  CreateDocumentType(const nsAString& aQualifiedName,
                     const nsAString& aPublicId,
                     const nsAString& aSystemId,
                     ErrorResult& aRv);

  already_AddRefed<nsIDocument>
  CreateDocument(const nsAString& aNamespaceURI,
                 const nsAString& aQualifiedName,
                 nsIDOMDocumentType* aDoctype,
                 ErrorResult& aRv);

  already_AddRefed<nsIDocument>
  CreateHTMLDocument(const Optional<nsAString>& aTitle, ErrorResult& aRv);

private:
  nsresult CreateDocument(const nsAString& aNamespaceURI,
                          const nsAString& aQualifiedName,
                          nsIDOMDocumentType* aDoctype,
                          nsIDocument** aDocument,
                          nsIDOMDocument** aDOMDocument);
  nsresult CreateHTMLDocument(const nsAString& aTitle,
                              nsIDocument** aDocument,
                              nsIDOMDocument** aDOMDocument);

  nsCOMPtr<nsIDocument> mOwner;
  nsWeakPtr mScriptObject;
  nsCOMPtr<nsIURI> mDocumentURI;
  nsCOMPtr<nsIURI> mBaseURI;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DOMImplementation_h
