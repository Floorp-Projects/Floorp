/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

#include "nsCopySupport.h"
#include "nsIDocumentEncoder.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIComponentManager.h" 
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsWidgetsCID.h"
#include "nsIEventStateManager.h"
#include "nsIPresContext.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsISupportsPrimitives.h"
#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
static NS_DEFINE_CID(kUBidiUtilCID, NS_UNICHARBIDIUTIL_CID);
#endif

static NS_DEFINE_CID(kCClipboardCID,           NS_CLIPBOARD_CID);
static NS_DEFINE_CID(kCTransferableCID,        NS_TRANSFERABLE_CID);
static NS_DEFINE_CID(kHTMLConverterCID,        NS_HTMLFORMATCONVERTER_CID);
static NS_DEFINE_CID(kTextEncoderCID,          NS_TEXT_ENCODER_CID);

// private clipboard data flavors for html copy, used by editor when pasting
#define kHTMLContext   "text/_moz_htmlcontext"
#define kHTMLInfo      "text/_moz_htmlinfo"


nsresult nsCopySupport::HTMLCopy(nsISelection *aSel, nsIDocument *aDoc, PRInt16 aClipboardID)
{
  nsresult rv = NS_OK;
  
  nsCOMPtr<nsIDocumentEncoder> docEncoder;

  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);

  rv = docEncoder->Init(aDoc, NS_LITERAL_STRING("text/html"), 0);
  if (NS_FAILED(rv)) return rv;
  rv = docEncoder->SetSelection(aSel);
  if (NS_FAILED(rv)) return rv;
  nsAutoString mimeType;
  rv = docEncoder->GetMimeType(mimeType);
  if (NS_FAILED(rv)) return rv;

  nsAutoString buffer, parents, info;
  PRBool bIsHTMLCopy = PR_FALSE;
  if (mimeType.EqualsWithConversion("text/html")) 
    bIsHTMLCopy = PR_TRUE;
    
  if (bIsHTMLCopy)
  {
    // encode the selection as html with contextual info
    rv = docEncoder->EncodeToStringWithContext(buffer, parents, info);
    if (NS_FAILED(rv)) 
      return rv;
  }
  else
  {
    // encode the selection
    rv = docEncoder->EncodeToString(buffer);
    if (NS_FAILED(rv)) 
      return rv;
  }
  
#ifdef IBMBIDI //ahmed
  rv = NS_OK;
  PRBool arabicCharset;
  
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDoc);
  if (doc) {
    nsIPresShell* shell = doc->GetShellAt(0);
    if (shell) {
      nsCOMPtr<nsIPresContext> context;
      shell->GetPresContext(getter_AddRefs(context) );
      if (context) {
        context->IsArabicEncoding(arabicCharset);
        if (arabicCharset) {
          nsCOMPtr<nsIUBidiUtils> bidiUtils = do_GetService("@mozilla.org/intl/unicharbidiutil;1");
          PRUint32 bidiOptions;
          PRBool isVisual;
          PRBool isBidiSystem;
    
          context->GetBidi(&bidiOptions);
          context->IsVisualMode(isVisual);
          context->GetIsBidiSystem(isBidiSystem);
          if ( (GET_BIDI_OPTION_CLIPBOARDTEXTMODE(bidiOptions) == IBMBIDI_CLIPBOARDTEXTMODE_LOGICAL)&&(isVisual)//&&(isBidiSystem)
             ) {
            nsAutoString newBuffer;
            if (isBidiSystem) {
#if 0 // Until we finalize the conversion routine
              if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_LTR) {
                bidiUtils->Conv_FE_06_WithReverse(buffer, newBuffer);
              } 
              if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL) {
                bidiUtils->Conv_FE_06 (buffer, newBuffer);
              }
#endif
            }
            else { //nonbidisystem
              bidiUtils->HandleNumbers(buffer, newBuffer);//ahmed
            }
            buffer = newBuffer;
          }
          //Mohamed
          else {
#if 0 // Until we finalize the conversion routine
            nsAutoString bidiCharset;
            context->GetBidiCharset(bidiCharset);
            if (bidiCharset.EqualsIgnoreCase("UTF-8") || (!isVisual)) {
              if ( (GET_BIDI_OPTION_CLIPBOARDTEXTMODE(bidiOptions) == IBMBIDI_CLIPBOARDTEXTMODE_VISUAL) || (!isBidiSystem) ) {
                nsAutoString newBuffer;
                bidiUtils->Conv_06_FE_WithReverse(buffer, newBuffer, GET_BIDI_OPTION_DIRECTION(bidiOptions));
                bidiUtils->HandleNumbers(newBuffer, buffer);
              }
            }
#endif
          }
        }
      }
    }
  }
#endif // IBMBIDI

  // Get the Clipboard
  NS_WITH_SERVICE(nsIClipboard, clipboard, kCClipboardCID, &rv);
  if (NS_FAILED(rv)) 
    return rv;

  if ( clipboard ) 
  {
    // Create a transferable for putting data on the Clipboard
    nsCOMPtr<nsITransferable> trans = do_CreateInstance(kCTransferableCID);
    if ( trans ) 
    {
      if (bIsHTMLCopy)
      {
        // set up the data converter
        nsCOMPtr<nsIFormatConverter> htmlConverter = do_CreateInstance(kHTMLConverterCID);
        NS_ENSURE_TRUE(htmlConverter, NS_ERROR_FAILURE);
        trans->SetConverter(htmlConverter);
      }
      
      // get wStrings to hold clip data
      nsCOMPtr<nsISupportsWString> dataWrapper, contextWrapper, infoWrapper;
      dataWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
      NS_ENSURE_TRUE(dataWrapper, NS_ERROR_FAILURE);
      if (bIsHTMLCopy)
      {
        contextWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
        NS_ENSURE_TRUE(contextWrapper, NS_ERROR_FAILURE);
        infoWrapper = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
        NS_ENSURE_TRUE(infoWrapper, NS_ERROR_FAILURE);
      }
      
      // populate the strings
      dataWrapper->SetData ( NS_CONST_CAST(PRUnichar*,buffer.GetUnicode()) );
      if (bIsHTMLCopy)
      {
        contextWrapper->SetData ( NS_CONST_CAST(PRUnichar*,parents.GetUnicode()) );
        infoWrapper->SetData ( NS_CONST_CAST(PRUnichar*,info.GetUnicode()) );
      }
      
      // QI the data object an |nsISupports| so that when the transferable holds
      // onto it, it will addref the correct interface.
      nsCOMPtr<nsISupports> genericDataObj ( do_QueryInterface(dataWrapper) );
      if (bIsHTMLCopy)
      {
        if (buffer.Length())
        {
          // Add the html DataFlavor to the transferable
          trans->AddDataFlavor(kHTMLMime);
          trans->SetTransferData(kHTMLMime, genericDataObj, buffer.Length()*2);
        }
        if (parents.Length())
        {
          // Add the htmlcontext DataFlavor to the transferable
          trans->AddDataFlavor(kHTMLContext);
          genericDataObj = do_QueryInterface(contextWrapper);
          trans->SetTransferData(kHTMLContext, genericDataObj, parents.Length()*2);
        }
        if (info.Length())
        {
          // Add the htmlinfo DataFlavor to the transferable
          trans->AddDataFlavor(kHTMLInfo);
          genericDataObj = do_QueryInterface(infoWrapper);
          trans->SetTransferData(kHTMLInfo, genericDataObj, info.Length()*2);
        }
      }
      else
      {
        if (buffer.Length())
        {
         // Add the unicode DataFlavor to the transferable
          trans->AddDataFlavor(kUnicodeMime);
          trans->SetTransferData(kUnicodeMime, genericDataObj, buffer.Length()*2);
        }
      }
      // put the transferable on the clipboard
      clipboard->SetData(trans, nsnull, aClipboardID);
    }
  }
  return rv;
}
