/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * shaver@mozilla.org
 */

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsIAutoCopy.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsISelectionListener.h"
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsIDOMDocument.h"
#include "nsIDocumentEncoder.h"

#include "nsIDocument.h"
#include "nsSupportsPrimitives.h"

// private clipboard data flavors for html copy, used by editor when pasting
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"


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
protected:
  nsCOMPtr<nsIClipboard> mClipboard;
  nsCOMPtr<nsITransferable> mTransferable;
  nsCOMPtr<nsIFormatConverter> mConverter;
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

  if (!mClipboard) {
    static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
    mClipboard = do_GetService(kCClipboardCID, &rv);
    if (NS_FAILED(rv))
      return rv;
  } 
  if (!(aReason & nsISelectionListener::MOUSEUP_REASON))
    return NS_OK;//dont care if we are still dragging. or if its not from a mouseup
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
  nsAutoString buffer, parents, info;

  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  docEncoder->Init(doc, NS_LITERAL_STRING("text/html"), 0);
  docEncoder->SetSelection(aSel);

  rv = docEncoder->EncodeToStringWithContext(buffer, parents, info);
  NS_ENSURE_SUCCESS(rv, rv);

  /* create a transferable */
  static NS_DEFINE_CID(kCTransferableCID, NS_TRANSFERABLE_CID);
  nsCOMPtr<nsITransferable> trans;
  trans = do_CreateInstance(kCTransferableCID);
  if (!trans)
    return NS_ERROR_FAILURE;

  if (!mConverter) {
    static NS_DEFINE_CID(kHTMLConverterCID, NS_HTMLFORMATCONVERTER_CID);
    mConverter = do_CreateInstance(kHTMLConverterCID);
    if (!mConverter)
      return NS_ERROR_FAILURE;
  }

  trans->AddDataFlavor(kHTMLMime);
  trans->SetConverter(mConverter);
  
  // Add the html DataFlavor to the transferable
  trans->AddDataFlavor(kHTMLMime);
  // Add the htmlcontext DataFlavor to the transferable
  trans->AddDataFlavor(kHTMLContext);
  // Add the htmlinfo DataFlavor to the transferable
  trans->AddDataFlavor(kHTMLInfo);

  // get wStrings to hold clip data
  nsCOMPtr<nsISupportsWString> dataWrapper, contextWrapper, infoWrapper;
  dataWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
  NS_ENSURE_TRUE(dataWrapper, NS_ERROR_FAILURE);
  contextWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
  NS_ENSURE_TRUE(contextWrapper, NS_ERROR_FAILURE);
  infoWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
  NS_ENSURE_TRUE(infoWrapper, NS_ERROR_FAILURE);

  // populate the strings
  dataWrapper->SetData ( NS_CONST_CAST(PRUnichar*,buffer.GetUnicode()) );
  contextWrapper->SetData ( NS_CONST_CAST(PRUnichar*,parents.GetUnicode()) );
  infoWrapper->SetData ( NS_CONST_CAST(PRUnichar*,info.GetUnicode()) );
      
  // QI the data object an |nsISupports| so that when the transferable holds
  // onto it, it will addref the correct interface.
  nsCOMPtr<nsISupports> genericDataObj ( do_QueryInterface(dataWrapper) );
  trans->SetTransferData(kHTMLMime, genericDataObj, buffer.Length()*2);
  genericDataObj = do_QueryInterface(contextWrapper);
  trans->SetTransferData(kHTMLContext, genericDataObj, parents.Length()*2);
  genericDataObj = do_QueryInterface(infoWrapper);
  trans->SetTransferData(kHTMLInfo, genericDataObj, info.Length()*2);

  // put the transferable on the clipboard
  mClipboard->SetData(trans, nsnull, nsIClipboard::kSelectionClipboard);

#ifdef DEBUG_CLIPBOARD
  static char *reasons[] = {
    "UNKNOWN", "NEW", "REMOVED", "ALTERED",
    "BOGUS4", "BOGUS5", "BOGUS6", "BOGUS7", "BOGUS8"
  };

  nsAutoString str;
  aSel->ToString(str);
  char *selStr = str.ToNewCString();
  fprintf(stderr, "SELECTION: %s, %p, %p [%s]\n", reasons[reason], doc, aSel,
          selStr);
  nsMemory::Free(selStr);
#endif
  return NS_OK;
}

