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
    // Oops -- we don't support off-document references
    return;
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
    return;
  }

  if (aWatch) {
    nsCOMPtr<nsIAtom> atom = do_GetAtom(ref);
    if (!atom)
      return;
    atom.swap(mWatchID);
    mWatchDocument = doc;
    mContent = mWatchDocument->AddIDTargetObserver(mWatchID, Observe, this);
    return;
  }
  
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
  NS_ASSERTION(domDoc, "Content doesn't reference a dom Document");

  nsCOMPtr<nsIDOMElement> element;
  rv = domDoc->GetElementById(ref, getter_AddRefs(element));
  if (element) {
    mContent = do_QueryInterface(element);
  }
}

void
nsReferencedElement::Traverse(nsCycleCollectionTraversalCallback* aCB)
{
  aCB->NoteXPCOMChild(mWatchDocument);
  aCB->NoteXPCOMChild(mContent);
}

void
nsReferencedElement::Unlink()
{
  if (mWatchDocument && mWatchID) {
    mWatchDocument->RemoveIDTargetObserver(mWatchID, Observe, this);
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
    p->mPendingNotification = new Notification(p, aOldContent, aNewContent);
    nsContentUtils::AddScriptRunner(p->mPendingNotification);
  }
  PRBool keepTracking = p->IsPersistent();
  if (!keepTracking) {
    p->mWatchDocument = nsnull;
    p->mWatchID = nsnull;
  }
  return keepTracking;
}
