/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TRANSFRMX_TXMOZILLAXSLTPROCESSOR_H
#define TRANSFRMX_TXMOZILLAXSLTPROCESSOR_H

#include "nsAutoPtr.h"
#include "nsStubMutationObserver.h"
#include "nsIDocumentTransformer.h"
#include "txExpandedNameMap.h"
#include "txNamespaceMap.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/XSLTProcessorBinding.h"

class nsINode;
class nsIURI;
class txStylesheet;
class txResultRecycler;
class txIGlobalParameter;

namespace mozilla {
namespace dom {

class DocGroup;
class Document;
class DocumentFragment;
class GlobalObject;

}  // namespace dom
}  // namespace mozilla

#define XSLT_MSGS_URL "chrome://global/locale/xslt/xslt.properties"

/**
 * txMozillaXSLTProcessor is a front-end to the XSLT Processor.
 */
class txMozillaXSLTProcessor final : public nsIDocumentTransformer,
                                     public nsStubMutationObserver,
                                     public nsWrapperCache {
 public:
  /**
   * Creates a new txMozillaXSLTProcessor
   */
  txMozillaXSLTProcessor();

  // nsISupports interface
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(txMozillaXSLTProcessor,
                                                         nsIDocumentTransformer)

  // nsIDocumentTransformer interface
  NS_IMETHOD SetTransformObserver(nsITransformObserver* aObserver) override;
  NS_IMETHOD LoadStyleSheet(nsIURI* aUri,
                            mozilla::dom::Document* aLoaderDocument) override;
  NS_IMETHOD SetSourceContentModel(nsINode* aSource) override;
  NS_IMETHOD CancelLoads() override { return NS_OK; }
  NS_IMETHOD AddXSLTParamNamespace(const nsString& aPrefix,
                                   const nsString& aNamespace) override;
  NS_IMETHOD AddXSLTParam(const nsString& aName, const nsString& aNamespace,
                          const nsString& aSelect, const nsString& aValue,
                          nsINode* aContext) override;

  // nsIMutationObserver interface
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTREMOVED
  NS_DECL_NSIMUTATIONOBSERVER_NODEWILLBEDESTROYED

  // nsWrapperCache
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL
  nsISupports* GetParentObject() const { return mOwner; }

  mozilla::dom::DocGroup* GetDocGroup() const;

  static already_AddRefed<txMozillaXSLTProcessor> Constructor(
      const mozilla::dom::GlobalObject& aGlobal, mozilla::ErrorResult& aRv);

  void ImportStylesheet(nsINode& stylesheet, mozilla::ErrorResult& aRv);
  already_AddRefed<mozilla::dom::DocumentFragment> TransformToFragment(
      nsINode& source, mozilla::dom::Document& docVal,
      mozilla::ErrorResult& aRv);
  already_AddRefed<mozilla::dom::Document> TransformToDocument(
      nsINode& source, mozilla::ErrorResult& aRv);

  void SetParameter(JSContext* aCx, const nsAString& aNamespaceURI,
                    const nsAString& aLocalName, JS::Handle<JS::Value> aValue,
                    mozilla::ErrorResult& aRv);
  already_AddRefed<nsIVariant> GetParameter(const nsAString& aNamespaceURI,
                                            const nsAString& aLocalName,
                                            mozilla::ErrorResult& aRv);
  void RemoveParameter(const nsAString& aNamespaceURI,
                       const nsAString& aLocalName, mozilla::ErrorResult& aRv);
  void ClearParameters();
  void Reset();

  uint32_t Flags(mozilla::dom::SystemCallerGuarantee);
  void SetFlags(uint32_t aFlags, mozilla::dom::SystemCallerGuarantee);

  nsresult setStylesheet(txStylesheet* aStylesheet);
  void reportError(nsresult aResult, const char16_t* aErrorText,
                   const char16_t* aSourceText);

  nsINode* GetSourceContentModel() { return mSource; }

  nsresult TransformToDoc(mozilla::dom::Document** aResult,
                          bool aCreateDataDocument);

  bool IsLoadDisabled() {
    return (mFlags & mozilla::dom::XSLTProcessor_Binding::DISABLE_ALL_LOADS) !=
           0;
  }

  static nsresult Startup();
  static void Shutdown();

 private:
  explicit txMozillaXSLTProcessor(nsISupports* aOwner);
  /**
   * Default destructor for txMozillaXSLTProcessor
   */
  ~txMozillaXSLTProcessor();

  nsresult DoTransform();
  void notifyError();
  nsresult ensureStylesheet();

  // Helper method for the WebIDL SetParameter.
  nsresult SetParameter(const nsAString& aNamespaceURI,
                        const nsAString& aLocalName, nsIVariant* aValue);

  nsCOMPtr<nsISupports> mOwner;

  RefPtr<txStylesheet> mStylesheet;
  mozilla::dom::Document* mStylesheetDocument;  // weak
  nsCOMPtr<nsIContent> mEmbeddedStylesheetRoot;

  nsCOMPtr<nsINode> mSource;
  nsresult mTransformResult;
  nsresult mCompileResult;
  nsString mErrorText, mSourceText;
  nsCOMPtr<nsITransformObserver> mObserver;
  txOwningExpandedNameMap<txIGlobalParameter> mVariables;
  txNamespaceMap mParamNamespaceMap;
  RefPtr<txResultRecycler> mRecycler;

  uint32_t mFlags;
};

extern nsresult TX_LoadSheet(nsIURI* aUri, txMozillaXSLTProcessor* aProcessor,
                             mozilla::dom::Document* aLoaderDocument,
                             mozilla::dom::ReferrerPolicy aReferrerPolicy);

extern nsresult TX_CompileStylesheet(nsINode* aNode,
                                     txMozillaXSLTProcessor* aProcessor,
                                     txStylesheet** aStylesheet);

#endif
