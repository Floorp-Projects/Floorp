/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan   <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsReferencedElement.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsBindingManager.h"
#include "nsIURL.h"
#include "nsEscape.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIDOMNode.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsCycleCollectionParticipant.h"

static PRBool EqualExceptRef(nsIURL* aURL1, nsIURL* aURL2)
{
  nsCOMPtr<nsIURI> u1;
  nsCOMPtr<nsIURI> u2;

  nsresult rv = aURL1->Clone(getter_AddRefs(u1));
  if (NS_SUCCEEDED(rv)) {
    rv = aURL2->Clone(getter_AddRefs(u2));
  }
  if (NS_FAILED(rv))
    return PR_FALSE;

  nsCOMPtr<nsIURL> url1 = do_QueryInterface(u1);
  nsCOMPtr<nsIURL> url2 = do_QueryInterface(u2);
  if (!url1 || !url2) {
    NS_WARNING("Cloning a URL produced a non-URL");
    return PR_FALSE;
  }
  url1->SetRef(EmptyCString());
  url2->SetRef(EmptyCString());

  PRBool equal;
  rv = url1->Equals(url2, &equal);
  return NS_SUCCEEDED(rv) && equal;
}

void
nsReferencedElement::Reset(nsIContent* aFromContent, nsIURI* aURI, PRBool aWatch)
{
  Unlink();

  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  if (!url)
    return;

  nsCAutoString refPart;
  url->GetRef(refPart);
  // Unescape %-escapes in the reference. The result will be in the
  // origin charset of the URL, hopefully...
  NS_UnescapeURL(refPart);

  nsCAutoString charset;
  url->GetOriginCharset(charset);
  nsAutoString ref;
  nsresult rv = nsContentUtils::ConvertStringFromCharset(charset, refPart, ref);
  if (NS_FAILED(rv)) {
    CopyUTF8toUTF16(refPart, ref);
  }
  if (ref.IsEmpty())
    return;

  // Get the current document
  nsIDocument *doc = aFromContent->GetCurrentDoc();
  if (!doc)
    return;

  // This will be the URI of the document the content belongs to
  // (the URI of the XBL document if the content is anonymous
  // XBL content)
  nsCOMPtr<nsIURL> documentURL = do_QueryInterface(doc->GetDocumentURI());
  nsIContent* bindingParent = aFromContent->GetBindingParent();
  PRBool isXBL = PR_FALSE;
  if (bindingParent) {
    nsXBLBinding* binding = doc->BindingManager()->GetBinding(bindingParent);
    if (binding) {
      // XXX sXBL/XBL2 issue
      // If this is an anonymous XBL element then the URI is
      // relative to the binding document. A full fix requires a
      // proper XBL2 implementation but for now URIs that are
      // relative to the binding document should be resolve to the
      // copy of the target element that has been inserted into the
      // bound document.
      documentURL = do_QueryInterface(binding->PrototypeBinding()->DocURI());
      isXBL = PR_TRUE;
    }
  }
  if (!documentURL)
    return;

  if (!EqualExceptRef(url, documentURL)) {
    // Don't take the XBL codepath here, since we'll want to just
    // normally set up our external resource document and then watch
    // it as needed.
    isXBL = PR_FALSE;
    nsRefPtr<nsIDocument::ExternalResourceLoad> load;
    doc = doc->RequestExternalResource(url, aFromContent, getter_AddRefs(load));
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

  // Get the element
  if (isXBL) {
    nsCOMPtr<nsIDOMNodeList> anonymousChildren;
    doc->BindingManager()->
      GetAnonymousNodesFor(bindingParent, getter_AddRefs(anonymousChildren));

    if (anonymousChildren) {
      PRUint32 length;
      anonymousChildren->GetLength(&length);
      for (PRUint32 i = 0; i < length && !mContent; ++i) {
        nsCOMPtr<nsIDOMNode> node;
        anonymousChildren->Item(i, getter_AddRefs(node));
        nsCOMPtr<nsIContent> c = do_QueryInterface(node);
        if (c) {
          mContent = nsContentUtils::MatchElementId(c, ref);
        }
      }
    }

    // We don't have watching working yet for XBL, so bail out here.
    return;
  }

  if (aWatch) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(ref);
    if (!atom)
      return;
    atom.swap(mWatchID);
  }

  HaveNewDocument(doc, aWatch, ref);
}

void
nsReferencedElement::HaveNewDocument(nsIDocument* aDocument, PRBool aWatch,
                                     const nsString& aRef)
{
  if (aWatch) {
    mWatchDocument = aDocument;
    if (mWatchDocument) {
      mContent = mWatchDocument->AddIDTargetObserver(mWatchID, Observe, this);
    }
    return;
  }
  
  if (!aDocument) {
    return;
  }
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(aDocument);
  NS_ASSERTION(domDoc, "Content doesn't reference a dom Document");

  nsCOMPtr<nsIDOMElement> element;
  domDoc->GetElementById(aRef, getter_AddRefs(element));
  if (element) {
    mContent = do_QueryInterface(element);
  }
}

void
nsReferencedElement::Traverse(nsCycleCollectionTraversalCallback* aCB)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mWatchDocument");
  aCB->NoteXPCOMChild(mWatchDocument);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*aCB, "mContent");
  aCB->NoteXPCOMChild(mContent);
}

void
nsReferencedElement::Unlink()
{
  if (mWatchDocument && mWatchID) {
    mWatchDocument->RemoveIDTargetObserver(mWatchID, Observe, this);
  }
  if (mPendingNotification) {
    mPendingNotification->Clear();
    mPendingNotification = nsnull;
  }
  mWatchDocument = nsnull;
  mWatchID = nsnull;
  mContent = nsnull;
}

PRBool
nsReferencedElement::Observe(nsIContent* aOldContent,
                             nsIContent* aNewContent, void* aData)
{
  nsReferencedElement* p = static_cast<nsReferencedElement*>(aData);
  if (p->mPendingNotification) {
    p->mPendingNotification->SetTo(aNewContent);
  } else {
    NS_ASSERTION(aOldContent == p->mContent, "Failed to track content!");
    ChangeNotification* watcher =
      new ChangeNotification(p, aOldContent, aNewContent);
    p->mPendingNotification = watcher;
    nsContentUtils::AddScriptRunner(watcher);
  }
  PRBool keepTracking = p->IsPersistent();
  if (!keepTracking) {
    p->mWatchDocument = nsnull;
    p->mWatchID = nsnull;
  }
  return keepTracking;
}

NS_IMPL_ISUPPORTS_INHERITED0(nsReferencedElement::ChangeNotification,
                             nsRunnable)

NS_IMPL_ISUPPORTS1(nsReferencedElement::DocumentLoadNotification,
                   nsIObserver)

NS_IMETHODIMP
nsReferencedElement::DocumentLoadNotification::Observe(nsISupports* aSubject,
                                                       const char* aTopic,
                                                       const PRUnichar* aData)
{
  NS_ASSERTION(PL_strcmp(aTopic, "external-resource-document-created") == 0,
               "Unexpected topic");
  if (mTarget) {
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(aSubject);
    mTarget->mPendingNotification = nsnull;
    NS_ASSERTION(!mTarget->mContent, "Why do we have content here?");
    // If we got here, that means we had Reset() called with aWatch ==
    // PR_TRUE.  So keep watching if IsPersistent().
    mTarget->HaveNewDocument(doc, mTarget->IsPersistent(), mRef);
    mTarget->ContentChanged(nsnull, mTarget->mContent);
  }
  return NS_OK;
}
