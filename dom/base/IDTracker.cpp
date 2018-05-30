/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDTracker.h"

#include "mozilla/Encoding.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsBindingManager.h"
#include "nsEscape.h"
#include "nsXBLPrototypeBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

void
IDTracker::Reset(nsIContent* aFromContent, nsIURI* aURI,
                 bool aWatch, bool aReferenceImage)
{
  MOZ_ASSERT(aFromContent, "Reset() expects non-null content pointer");

  Unlink();

  if (!aURI)
    return;

  nsAutoCString refPart;
  aURI->GetRef(refPart);
  // Unescape %-escapes in the reference. The result will be in the
  // document charset, hopefully...
  NS_UnescapeURL(refPart);

  // Get the current document
  nsIDocument *doc = aFromContent->OwnerDoc();
  if (!doc) {
    return;
  }

  auto encoding = doc->GetDocumentCharacterSet();
  nsAutoString ref;
  nsresult rv = encoding->DecodeWithoutBOMHandling(refPart, ref);
  if (NS_FAILED(rv) || ref.IsEmpty()) {
    return;
  }
  rv = NS_OK;

  nsIContent* bindingParent = aFromContent->GetBindingParent();
  if (bindingParent) {
    nsXBLBinding* binding = bindingParent->GetXBLBinding();
    if (!binding) {
      // This happens, for example, if aFromContent is part of the content
      // inserted by a call to nsIDocument::InsertAnonymousContent, which we
      // also want to handle.  (It also happens for <use>'s anonymous
      // content etc.)
      Element* anonRoot =
        doc->GetAnonRootIfInAnonymousContentContainer(aFromContent);
      if (anonRoot) {
        mElement = nsContentUtils::MatchElementId(anonRoot, ref);
        // We don't have watching working yet for anonymous content, so bail out here.
        return;
      }
    } else {
      bool isEqualExceptRef;
      rv = aURI->EqualsExceptRef(binding->PrototypeBinding()->DocURI(),
                                 &isEqualExceptRef);
      if (NS_SUCCEEDED(rv) && isEqualExceptRef) {
        // XXX sXBL/XBL2 issue
        // Our content is an anonymous XBL element from a binding inside the
        // same document that the referenced URI points to. In order to avoid
        // the risk of ID collisions we restrict ourselves to anonymous
        // elements from this binding; specifically, URIs that are relative to
        // the binding document should resolve to the copy of the target
        // element that has been inserted into the bound document.
        // If the URI points to a different document we don't need this
        // restriction.
        nsINodeList* anonymousChildren =
          doc->BindingManager()->GetAnonymousNodesFor(bindingParent);

        if (anonymousChildren) {
          uint32_t length = anonymousChildren->Length();
          for (uint32_t i = 0; i < length && !mElement; ++i) {
            mElement =
              nsContentUtils::MatchElementId(anonymousChildren->Item(i), ref);
          }
        }

        // We don't have watching working yet for XBL, so bail out here.
        return;
      }
    }
  }

  bool isEqualExceptRef;
  rv = aURI->EqualsExceptRef(doc->GetDocumentURI(), &isEqualExceptRef);
  if (NS_FAILED(rv) || !isEqualExceptRef) {
    RefPtr<nsIDocument::ExternalResourceLoad> load;
    doc = doc->RequestExternalResource(aURI, aFromContent,
                                       getter_AddRefs(load));
    if (!doc) {
      if (!load || !aWatch) {
        // Nothing will ever happen here
        return;
      }

      DocumentLoadNotification* observer =
        new DocumentLoadNotification(this, ref);
      mPendingNotification = observer;
      if (observer) {
        load->AddObserver(observer);
      }
      // Keep going so we set up our watching stuff a bit
    }
  }

  if (aWatch) {
    RefPtr<nsAtom> atom = NS_Atomize(ref);
    if (!atom)
      return;
    atom.swap(mWatchID);
  }

  mReferencingImage = aReferenceImage;

  HaveNewDocument(doc, aWatch, ref);
}

void
IDTracker::ResetWithID(nsIContent* aFromContent, const nsString& aID,
                       bool aWatch)
{
  nsIDocument *doc = aFromContent->OwnerDoc();
  if (!doc)
    return;

  // XXX Need to take care of XBL/XBL2

  if (aWatch) {
    RefPtr<nsAtom> atom = NS_Atomize(aID);
    if (!atom)
      return;
    atom.swap(mWatchID);
  }

  mReferencingImage = false;

  HaveNewDocument(doc, aWatch, aID);
}

void
IDTracker::HaveNewDocument(nsIDocument* aDocument, bool aWatch,
                           const nsString& aRef)
{
  if (aWatch) {
    mWatchDocument = aDocument;
    if (mWatchDocument) {
      mElement = mWatchDocument->AddIDTargetObserver(mWatchID, Observe, this,
                                                     mReferencingImage);
    }
    return;
  }

  if (!aDocument) {
    return;
  }

  Element *e = mReferencingImage ? aDocument->LookupImageElement(aRef) :
                                   aDocument->GetElementById(aRef);
  if (e) {
    mElement = e;
  }
}

void
IDTracker::Traverse(nsCycleCollectionTraversalCallback* aCB)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mWatchDocument");
  aCB->NoteXPCOMChild(mWatchDocument);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mContent");
  aCB->NoteXPCOMChild(mElement);
}

void
IDTracker::Unlink()
{
  if (mWatchDocument && mWatchID) {
    mWatchDocument->RemoveIDTargetObserver(mWatchID, Observe, this,
                                           mReferencingImage);
  }
  if (mPendingNotification) {
    mPendingNotification->Clear();
    mPendingNotification = nullptr;
  }
  mWatchDocument = nullptr;
  mWatchID = nullptr;
  mElement = nullptr;
  mReferencingImage = false;
}

bool
IDTracker::Observe(Element* aOldElement,
                   Element* aNewElement, void* aData)
{
  IDTracker* p = static_cast<IDTracker*>(aData);
  if (p->mPendingNotification) {
    p->mPendingNotification->SetTo(aNewElement);
  } else {
    NS_ASSERTION(aOldElement == p->mElement, "Failed to track content!");
    ChangeNotification* watcher =
      new ChangeNotification(p, aOldElement, aNewElement);
    p->mPendingNotification = watcher;
    nsContentUtils::AddScriptRunner(watcher);
  }
  bool keepTracking = p->IsPersistent();
  if (!keepTracking) {
    p->mWatchDocument = nullptr;
    p->mWatchID = nullptr;
  }
  return keepTracking;
}

NS_IMPL_ISUPPORTS_INHERITED0(IDTracker::ChangeNotification,
                             mozilla::Runnable)

NS_IMPL_ISUPPORTS(IDTracker::DocumentLoadNotification,
                  nsIObserver)

NS_IMETHODIMP
IDTracker::DocumentLoadNotification::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* aData)
{
  NS_ASSERTION(PL_strcmp(aTopic, "external-resource-document-created") == 0,
               "Unexpected topic");
  if (mTarget) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aSubject);
    mTarget->mPendingNotification = nullptr;
    NS_ASSERTION(!mTarget->mElement, "Why do we have content here?");
    // If we got here, that means we had Reset() called with aWatch ==
    // true.  So keep watching if IsPersistent().
    mTarget->HaveNewDocument(doc, mTarget->IsPersistent(), mRef);
    mTarget->ElementChanged(nullptr, mTarget->mElement);
  }
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
