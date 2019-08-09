/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULPrototypeDocument_h__
#define nsXULPrototypeDocument_h__

#include "js/TracingAPI.h"
#include "mozilla/Attributes.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "nsISerializable.h"
#include "nsCycleCollectionParticipant.h"
#include <functional>
#include "mozilla/dom/Element.h"

class nsAtom;
class nsIPrincipal;
class nsIURI;
class nsNodeInfoManager;
class nsXULPrototypeElement;
class nsXULPrototypePI;

/**
 * A "prototype" document that stores shared document information
 * for the XUL cache.
 * Among other things, stores the tree of nsXULPrototype*
 * objects, from which the real DOM tree is built later in
 * PrototypeDocumentContentSink::ResumeWalk.
 */
class nsXULPrototypeDocument final : public nsISerializable {
 public:
  static nsresult Create(nsIURI* aURI, nsXULPrototypeDocument** aResult);

  typedef std::function<void()> Callback;

  // nsISupports interface
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsISerializable interface
  NS_DECL_NSISERIALIZABLE

  nsresult InitPrincipal(nsIURI* aURI, nsIPrincipal* aPrincipal);
  nsIURI* GetURI();

  /**
   * Get/set the root nsXULPrototypeElement of the document.
   */
  nsXULPrototypeElement* GetRootElement();
  void SetRootElement(nsXULPrototypeElement* aElement);

  /**
   * Add a processing instruction to the prolog. Note that only
   * PI nodes are currently stored in a XUL prototype document's
   * prolog and that they're handled separately from the rest of
   * prototype node tree.
   *
   * @param aPI an already adrefed PI proto to add. This method takes
   *            ownership of the passed PI.
   */
  nsresult AddProcessingInstruction(nsXULPrototypePI* aPI);
  /**
   * @note GetProcessingInstructions retains the ownership (the PI
   *       protos only get deleted when the proto document is deleted)
   */
  const nsTArray<RefPtr<nsXULPrototypePI> >& GetProcessingInstructions() const;

  nsIPrincipal* DocumentPrincipal();
  void SetDocumentPrincipal(nsIPrincipal* aPrincipal);

  /**
   * If current prototype document has not yet finished loading,
   * appends aDocument to the list of documents to notify (via
   * PrototypeDocumentContentSink::OnPrototypeLoadDone()) and
   * sets aLoaded to false. Otherwise sets aLoaded to true.
   */
  nsresult AwaitLoadDone(Callback&& aCallback, bool* aResult);

  /**
   * Notifies each document registered via AwaitLoadDone on this
   * prototype document that the prototype has finished loading.
   * The notification is performed by calling
   * PrototypeDocumentContentSink::OnPrototypeLoadDone on the
   * registered documents.
   */
  nsresult NotifyLoadDone();

  nsNodeInfoManager* GetNodeInfoManager();

  void MarkInCCGeneration(uint32_t aCCGeneration);

  NS_DECL_CYCLE_COLLECTION_CLASS(nsXULPrototypeDocument)

  void TraceProtos(JSTracer* aTrc);

  bool WasL10nCached() { return mWasL10nCached; };

  void SetIsL10nCached();
  void RebuildPrototypeFromElement(nsXULPrototypeElement* aPrototype,
                                   mozilla::dom::Element* aElement, bool aDeep);
  void RebuildL10nPrototype(mozilla::dom::Element* aElement, bool aDeep);

 protected:
  nsCOMPtr<nsIURI> mURI;
  RefPtr<nsXULPrototypeElement> mRoot;
  nsTArray<RefPtr<nsXULPrototypePI> > mProcessingInstructions;

  bool mLoaded;
  nsTArray<Callback> mPrototypeWaiters;

  RefPtr<nsNodeInfoManager> mNodeInfoManager;

  uint32_t mCCGeneration;
  uint32_t mGCNumber;

  nsXULPrototypeDocument();
  virtual ~nsXULPrototypeDocument();
  nsresult Init();

  friend NS_IMETHODIMP NS_NewXULPrototypeDocument(
      nsXULPrototypeDocument** aResult);

  static uint32_t gRefCnt;
  bool mWasL10nCached;
};

#endif  // nsXULPrototypeDocument_h__
