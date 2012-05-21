/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsIContainerBoxObject.h"
#include "nsIBrowserBoxObject.h"
#include "nsIEditorBoxObject.h"
#include "nsIIFrameBoxObject.h"
#include "nsBoxObject.h"
#include "nsIDocShell.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIFrame.h"
#include "nsSubDocumentFrame.h"

/**
 * nsContainerBoxObject implements deprecated nsIBrowserBoxObject,
 * nsIEditorBoxObject and nsIIFrameBoxObject interfaces only because of the
 * backward compatibility.
 */

class nsContainerBoxObject : public nsBoxObject,
                             public nsIBrowserBoxObject,
                             public nsIEditorBoxObject,
                             public nsIIFrameBoxObject
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICONTAINERBOXOBJECT
  NS_DECL_NSIBROWSERBOXOBJECT
  NS_DECL_NSIEDITORBOXOBJECT
  NS_DECL_NSIIFRAMEBOXOBJECT
};

NS_INTERFACE_MAP_BEGIN(nsContainerBoxObject)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIContainerBoxObject, nsIBrowserBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIEditorBoxObject)
  NS_INTERFACE_MAP_ENTRY(nsIIFrameBoxObject)
NS_INTERFACE_MAP_END_INHERITING(nsBoxObject)

NS_IMPL_ADDREF_INHERITED(nsContainerBoxObject, nsBoxObject)
NS_IMPL_RELEASE_INHERITED(nsContainerBoxObject, nsBoxObject)

NS_IMETHODIMP nsContainerBoxObject::GetDocShell(nsIDocShell** aResult)
{
  *aResult = nsnull;

  nsIFrame *frame = GetFrame(false);

  if (frame) {
    nsSubDocumentFrame *subDocFrame = do_QueryFrame(frame);
    if (subDocFrame) {
      // Ok, the frame for mContent is an nsSubDocumentFrame, it knows how
      // to reach the docshell, so ask it...

      return subDocFrame->GetDocShell(aResult);
    }
  }

  if (!mContent) {
    return NS_OK;
  }
  
  // No nsSubDocumentFrame available for mContent, try if there's a mapping
  // between mContent's document to mContent's subdocument.

  // XXXbz sXBL/XBL2 issue -- ownerDocument or currentDocument?
  nsIDocument *doc = mContent->GetDocument();

  if (!doc) {
    return NS_OK;
  }
  
  nsIDocument *sub_doc = doc->GetSubDocumentFor(mContent);

  if (!sub_doc) {
    return NS_OK;
  }

  nsCOMPtr<nsISupports> container = sub_doc->GetContainer();

  if (!container) {
    return NS_OK;
  }

  return CallQueryInterface(container, aResult);
}

nsresult
NS_NewContainerBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsContainerBoxObject();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

