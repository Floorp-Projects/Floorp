/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsCopySupport.h"
#include "nsIDocumentEncoder.h"
#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIComponentManager.h" 
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsISelection.h"
#include "nsWidgetsCID.h"
#include "nsIEventStateManager.h"
#include "nsIPresContext.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsISupportsPrimitives.h"
#include "nsIDOMRange.h"

// for IBMBIDI
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIHTMLDocument.h"
#include "nsHTMLAtoms.h"
#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
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

  PRBool bIsPlainTextContext = PR_FALSE;

  rv = IsPlainTextContext(aSel, aDoc, &bIsPlainTextContext);
  if (NS_FAILED(rv)) return rv;

  PRBool bIsHTMLCopy = !bIsPlainTextContext;
  PRUint32 flags = 0;
  nsAutoString mimeType;

  if (bIsHTMLCopy)
    mimeType = NS_LITERAL_STRING(kHTMLMime);
  else
  {
    flags |= nsIDocumentEncoder::OutputBodyOnly | nsIDocumentEncoder::OutputPreformatted;
    mimeType = NS_LITERAL_STRING(kUnicodeMime);
  }

  rv = docEncoder->Init(aDoc, mimeType, flags);
  if (NS_FAILED(rv)) return rv;
  rv = docEncoder->SetSelection(aSel);
  if (NS_FAILED(rv)) return rv;
    
  nsAutoString buffer, parents, info;

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
    nsCOMPtr<nsIPresShell> shell;
    doc->GetShellAt(0, getter_AddRefs(shell));
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
          if ( (GET_BIDI_OPTION_CLIPBOARDTEXTMODE(bidiOptions) == IBMBIDI_CLIPBOARDTEXTMODE_LOGICAL)&&(isVisual)
             ) {
            nsAutoString newBuffer;
            if (isBidiSystem) { 
              if (GET_BIDI_OPTION_DIRECTION(bidiOptions) == IBMBIDI_TEXTDIRECTION_RTL) {
                bidiUtils->Conv_FE_06(buffer, newBuffer);
              }
              else {
                bidiUtils->Conv_FE_06_WithReverse(buffer, newBuffer);
              }
            }
            else { //nonbidisystem
              bidiUtils->HandleNumbers(buffer, newBuffer);//ahmed
            }
            buffer = newBuffer;
          }
          //Mohamed
          else {
            nsAutoString bidiCharset;
            context->GetBidiCharset(bidiCharset);
            if (bidiCharset.EqualsIgnoreCase("UTF-8") || (!isVisual)) {
              if ( (GET_BIDI_OPTION_CLIPBOARDTEXTMODE(bidiOptions) == IBMBIDI_CLIPBOARDTEXTMODE_VISUAL) || (!isBidiSystem) ) {
                nsAutoString newBuffer;
                bidiUtils->Conv_06_FE_WithReverse(buffer, newBuffer, GET_BIDI_OPTION_DIRECTION(bidiOptions));
                bidiUtils->HandleNumbers(newBuffer, buffer);
              }
            }
          }
        }
      }
    }
  }
#endif // IBMBIDI

  // Get the Clipboard
  nsCOMPtr<nsIClipboard> clipboard(do_GetService(kCClipboardCID, &rv));
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
      dataWrapper->SetData ( NS_CONST_CAST(PRUnichar*,buffer.get()) );
      if (bIsHTMLCopy)
      {
        contextWrapper->SetData ( NS_CONST_CAST(PRUnichar*,parents.get()) );
        infoWrapper->SetData ( NS_CONST_CAST(PRUnichar*,info.get()) );
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

nsresult nsCopySupport::IsPlainTextContext(nsISelection *aSel, nsIDocument *aDoc, PRBool *aIsPlainTextContext)
{
  nsresult rv;

  if (!aSel || !aIsPlainTextContext)
    return NS_ERROR_NULL_POINTER;

  *aIsPlainTextContext = PR_FALSE;
  
  nsCOMPtr<nsIDOMRange> range;
  nsCOMPtr<nsIDOMNode> commonParent;
  PRInt32 count = 0;

  rv = aSel->GetRangeCount(&count);
  NS_ENSURE_SUCCESS(rv, rv);

  // if selection is uninitialized return
  if (!count)
    return NS_ERROR_FAILURE;
  
  // we'll just use the common parent of the first range.  Implicit assumption
  // here that multi-range selections are table cell selections, in which case
  // the common parent is somewhere in the table and we don't really care where.
  rv = aSel->GetRangeAt(0, getter_AddRefs(range));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!range)
    return NS_ERROR_NULL_POINTER;
  range->GetCommonAncestorContainer(getter_AddRefs(commonParent));

  nsCOMPtr<nsIContent> tmp, selContent( do_QueryInterface(commonParent) );
  while (selContent)
  {
    // checking for selection inside a plaintext form widget
    nsCOMPtr<nsIAtom> atom;
    selContent->GetTag(*getter_AddRefs(atom));

    if (atom.get() == nsHTMLAtoms::input ||
        atom.get() == nsHTMLAtoms::textarea)
    {
      *aIsPlainTextContext = PR_TRUE;
      break;
    }

    if (atom.get() == nsHTMLAtoms::body)
    {
      // check for moz prewrap style on body.  If it's there we are 
      // in a plaintext editor.  This is pretty cheezy but I haven't 
      // found a good way to tell if we are in a plaintext editor.
      nsCOMPtr<nsIDOMElement> bodyElem = do_QueryInterface(selContent);
      nsAutoString wsVal;
      rv = bodyElem->GetAttribute(NS_LITERAL_STRING("style"), wsVal);
      if (NS_SUCCEEDED(rv) && (kNotFound != wsVal.Find(NS_LITERAL_STRING("-moz-pre-wrap").get())))
      {
        *aIsPlainTextContext = PR_TRUE;
        break;
      }
    }
    selContent->GetParent(*getter_AddRefs(tmp));
    selContent = tmp;
  }
  
  // also consider ourselves in a text widget if we can't find an html document
  nsCOMPtr<nsIHTMLDocument> htmlDoc = do_QueryInterface(aDoc);
  if (!htmlDoc) *aIsPlainTextContext = PR_TRUE;

  return NS_OK;
}
