/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * shaver@mozilla.org
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsIAutoCopy.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionListener.h"
#include "nsWidgetsCID.h"
#include "nsIDOMDocument.h"
#include "nsCopySupport.h"
#include "nsIClipboard.h"

#include "nsIDocument.h"
#include "nsSupportsPrimitives.h"

class nsAutoCopyService : public nsIAutoCopyService , public nsISelectionListener
{
public:
  NS_DECL_ISUPPORTS

  nsAutoCopyService();
  virtual ~nsAutoCopyService(){}//someday maybe we have it able to shutdown during run

  //nsIAutoCopyService interfaces
  NS_IMETHOD Listen(nsISelection *aDomSelection);
  //end nsIAutoCopyService

  //nsISelectionListener interfaces
  NS_IMETHOD NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason);
  //end nsISelectionListener 
};

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsAutoCopyService, nsIAutoCopyService,nsISelectionListener)

nsresult
NS_NewAutoCopyService(nsIAutoCopyService** aResult)
{
  *aResult = new nsAutoCopyService;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

nsAutoCopyService::nsAutoCopyService()
{
  NS_INIT_REFCNT();
}

NS_IMETHODIMP
nsAutoCopyService::Listen(nsISelection *aDomSelection)
{
  if (!aDomSelection)
    return NS_ERROR_NULL_POINTER;
  nsCOMPtr<nsISelection> selection(aDomSelection);
  nsCOMPtr<nsISelectionPrivate> selectionPrivate(do_QueryInterface(selection));
  return selectionPrivate->AddSelectionListener(this);
}


/*
 * What we do now:
 * On every selection change, we copy to the clipboard anew, creating a
 * HTML buffer, a transferable, an nsISupportsWString and
 * a huge mess every time.  This is basically what nsPresShell::DoCopy does
 * to move the selection into the clipboard for Edit->Copy.
 * 
 * What we should do, to make our end of the deal faster:
 * Create a singleton transferable with our own magic converter.  When selection
 * changes (use a quick cache to detect ``real'' changes), we put the new
 * nsISelection in the transferable.  Our magic converter will take care of
 * transferable->whatever-other-format when the time comes to actually
 * hand over the clipboard contents.
 *
 * Other issues:
 * - which X clipboard should we populate?
 * - should we use a different one than Edit->Copy, so that inadvertant
 *   selections (or simple clicks, which currently cause a selection
 *   notification, regardless of if they're in the document which currently has
 *   selection!) don't lose the contents of the ``application''?  Or should we
 *   just put some intelligence in the ``is this a real selection?'' code to
 *   protect our selection against clicks in other documents that don't create
 *   selections?
 * - maybe we should just never clear the X clipboard?  That would make this 
 *   problem just go away, which is very tempting.
 */

#define DRAGGING 1

NS_IMETHODIMP
nsAutoCopyService::NotifySelectionChanged(nsIDOMDocument *aDoc, nsISelection *aSel, short aReason)
{
  nsresult rv;

  if (!(aReason & nsISelectionListener::MOUSEUP_REASON))
    return NS_OK; //dont care if we are still dragging. or if its not from a mouseup
  PRBool collapsed;
  if (!aDoc || !aSel || NS_FAILED(aSel->GetIsCollapsed(&collapsed)) || collapsed) {
#ifdef DEBUG_CLIPBOARD
    fprintf(stderr, "CLIPBOARD: no selection/collapsed selection\n");
#endif
    /* clear X clipboard? */
    return NS_OK;
  }

  nsCOMPtr<nsIDocument> doc;
  doc = do_QueryInterface(NS_REINTERPRET_CAST(nsISupports *,aDoc),&rv);
  if (NS_FAILED(rv))
    return rv;
  if (!doc)
    return NS_ERROR_NULL_POINTER;

  // call the copy code
  rv = nsCopySupport::HTMLCopy(aSel, doc, nsIClipboard::kSelectionClipboard);
  return rv;
}

