/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *     Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
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
#include "nsIFrameFrame.h"

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

  nsIFrame *frame = GetFrame(PR_FALSE);

  if (frame) {
    nsIFrameFrame *frame_frame = do_QueryFrame(frame);
    if (frame_frame) {
      // Ok, the frame for mContent is a nsIFrameFrame, it knows how
      // to reach the docshell, so ask it...

      return frame_frame->GetDocShell(aResult);
    }
  }

  if (!mContent) {
    return NS_OK;
  }
  
  // No nsIFrameFrame available for mContent, try if there's a mapping
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

