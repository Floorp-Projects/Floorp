/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsReferencedElement.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsBindingManager.h"
#include "nsEscape.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsCycleCollectionParticipant.h"

/**
 * If aURI is https and is set to use its default port (i.e. has port = -1),
 * then this function will clone it, and give the clone a hardcoded port of 443
 * (which should already be its default).  Then, if that hardcoded port
 * "sticks" (indicating that aURI has the wrong default port), then this
 * function returns the clone, so that the caller can use it instead of aURI.
 * Otherwise, this function just returns nullptr.
 *
 * (This lets us correct for aURI possibly having the wrong mDefaultPort.)
 */
already_AddRefed<nsIURI>
CloneSecureURIWithHardcodedPort(nsIURI* aURI)
{
  // nsIURI port numbers of interest to this function:
  static const int32_t kUsingDefaultPort = -1;
  static const int32_t kDefaultPortForHTTPS = 443;

  bool isHttps;
  if (NS_FAILED(aURI->SchemeIs("https", &isHttps)) || !isHttps) {
    // URI isn't HTTPS, so it's not affected by bug 1247733. We can bail.
    return nullptr;
  }

  int32_t port;
  if (NS_FAILED(aURI->GetPort(&port)) || port != kUsingDefaultPort) {
    // URI isn't using its default port, so it's not affected by bug 1247733.
    // We can bail.
    return nullptr;
  }

  nsCOMPtr<nsIURI> cloneWithHardcodedPort;
  if (NS_FAILED(aURI->Clone(getter_AddRefs(cloneWithHardcodedPort)))) {
    // Clone failed. Bail.
    return nullptr;
  }
  if (NS_FAILED(cloneWithHardcodedPort->SetPort(kDefaultPortForHTTPS))) {
    // Hardcoding port 443 failed. Bail.
    return nullptr;
  }
  if (NS_FAILED(cloneWithHardcodedPort->GetPort(&port)) ||
      port == kUsingDefaultPort) {
    // The port-hardcoding didn't make a difference, which means aURI already
    // has port 443 as its mDefaultPort. So, our clone won't behave any
    // differently from aURI. So it won't be useful for our caller. Bail.
    return nullptr;
  }
  MOZ_ASSERT(port == kDefaultPortForHTTPS,
             "How did we end up with a port other than the one we just set...?");

  // The port hardcoding *did* make a difference! This means aURI has a bogus
  // default port (it's not 443, despite the fact that aURI is https). Return
  // our clone that has a hardcoded URI, so the caller can use it instead of aURI.
  return cloneWithHardcodedPort.forget();
}

void
nsReferencedElement::Reset(nsIContent* aFromContent, nsIURI* aURI,
                           bool aWatch, bool aReferenceImage)
{
  MOZ_ASSERT(aFromContent, "Reset() expects non-null content pointer");

  Unlink();

  if (!aURI)
    return;

  nsAutoCString refPart;
  aURI->GetRef(refPart);
  // Unescape %-escapes in the reference. The result will be in the
  // origin charset of the URL, hopefully...
  NS_UnescapeURL(refPart);

  nsAutoCString charset;
  aURI->GetOriginCharset(charset);
  nsAutoString ref;
  nsresult rv = nsContentUtils::ConvertStringFromEncoding(charset,
                                                          refPart,
                                                          ref);
  if (NS_FAILED(rv)) {
    // XXX Eww. If fallible malloc failed, using a conversion method that
    // assumes UTF-8 and doesn't handle UTF-8 errors.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=951082
    CopyUTF8toUTF16(refPart, ref);
  }
  if (ref.IsEmpty())
    return;

  // Get the current document
  nsIDocument *doc = aFromContent->OwnerDoc();
  if (!doc)
    return;

  nsIContent* bindingParent = aFromContent->GetBindingParent();
  if (bindingParent) {
    nsXBLBinding* binding = bindingParent->GetXBLBinding();
    if (binding) {
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
          uint32_t length;
          anonymousChildren->GetLength(&length);
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
  nsIURI* docURI = doc->GetDocumentURI();
  rv = aURI->EqualsExceptRef(docURI, &isEqualExceptRef);

  // BEGIN HACKAROUND FOR BUG 1247733
  if (NS_SUCCEEDED(rv) && !isEqualExceptRef) {
    // We think the URIs don't match, but it might just be because bug 1247733
    // gave us the wrong mDefaultPort in our document URI (if it's HTTPS due
    // to an HSTS upgrade). So: if we're HTTPS and we're using the default
    // port, then assume that we maybe have the wrong default port, and try
    // the comparison again, using a hardcoded port.
    nsCOMPtr<nsIURI> clonedDocURI = CloneSecureURIWithHardcodedPort(docURI);
    if (clonedDocURI) {
      // Try the comparison again.
      rv = aURI->EqualsExceptRef(clonedDocURI, &isEqualExceptRef);
    }
  }
  // END HACKAROUND FOR BUG 1247733

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
    nsCOMPtr<nsIAtom> atom = do_GetAtom(ref);
    if (!atom)
      return;
    atom.swap(mWatchID);
  }

  mReferencingImage = aReferenceImage;

  HaveNewDocument(doc, aWatch, ref);
}

void
nsReferencedElement::ResetWithID(nsIContent* aFromContent, const nsString& aID,
                                 bool aWatch)
{
  nsIDocument *doc = aFromContent->OwnerDoc();
  if (!doc)
    return;

  // XXX Need to take care of XBL/XBL2

  if (aWatch) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(aID);
    if (!atom)
      return;
    atom.swap(mWatchID);
  }

  mReferencingImage = false;

  HaveNewDocument(doc, aWatch, aID);
}

void
nsReferencedElement::HaveNewDocument(nsIDocument* aDocument, bool aWatch,
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
nsReferencedElement::Traverse(nsCycleCollectionTraversalCallback* aCB)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mWatchDocument");
  aCB->NoteXPCOMChild(mWatchDocument);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mContent");
  aCB->NoteXPCOMChild(mElement);
}

void
nsReferencedElement::Unlink()
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
nsReferencedElement::Observe(Element* aOldElement,
                             Element* aNewElement, void* aData)
{
  nsReferencedElement* p = static_cast<nsReferencedElement*>(aData);
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

NS_IMPL_ISUPPORTS_INHERITED0(nsReferencedElement::ChangeNotification,
                             nsRunnable)

NS_IMPL_ISUPPORTS(nsReferencedElement::DocumentLoadNotification,
                  nsIObserver)

NS_IMETHODIMP
nsReferencedElement::DocumentLoadNotification::Observe(nsISupports* aSubject,
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
