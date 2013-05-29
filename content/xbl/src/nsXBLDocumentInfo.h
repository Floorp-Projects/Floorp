/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLDocumentInfo_h__
#define nsXBLDocumentInfo_h__

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsWeakReference.h"
#include "nsIDocument.h"
#include "nsCycleCollectionParticipant.h"

class nsXBLPrototypeBinding;
class nsObjectHashtable;
class nsXBLDocGlobalObject;

class nsXBLDocumentInfo : public nsIScriptGlobalObjectOwner,
                          public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  nsXBLDocumentInfo(nsIDocument* aDocument);
  virtual ~nsXBLDocumentInfo();

  already_AddRefed<nsIDocument> GetDocument()
    { nsCOMPtr<nsIDocument> copy = mDocument; return copy.forget(); }

  bool GetScriptAccess() { return mScriptAccess; }

  nsIURI* DocumentURI() { return mDocument->GetDocumentURI(); }

  nsXBLPrototypeBinding* GetPrototypeBinding(const nsACString& aRef);
  nsresult SetPrototypeBinding(const nsACString& aRef,
                               nsXBLPrototypeBinding* aBinding);

  // This removes the binding without deleting it
  void RemovePrototypeBinding(const nsACString& aRef);

  nsresult WritePrototypeBindings();

  void SetFirstPrototypeBinding(nsXBLPrototypeBinding* aBinding);
  
  void FlushSkinStylesheets();

  bool IsChrome() { return mIsChrome; }

  // nsIScriptGlobalObjectOwner methods
  virtual nsIScriptGlobalObject* GetScriptGlobalObject() MOZ_OVERRIDE;

  void MarkInCCGeneration(uint32_t aGeneration);

  static nsresult ReadPrototypeBindings(nsIURI* aURI, nsXBLDocumentInfo** aDocInfo);

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsXBLDocumentInfo,
                                                         nsIScriptGlobalObjectOwner)

private:
  nsCOMPtr<nsIDocument> mDocument;
  bool mScriptAccess;
  bool mIsChrome;
  // the binding table owns each nsXBLPrototypeBinding
  nsObjectHashtable* mBindingTable;
  // non-owning pointer to the first binding in the table
  nsXBLPrototypeBinding* mFirstBinding;

  nsRefPtr<nsXBLDocGlobalObject> mGlobalObject;
};

#endif
