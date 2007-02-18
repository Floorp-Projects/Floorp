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

#include "nsIsIndexFrame.h"

#include "nsIContent.h"
#include "prtypes.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsPresState.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsHTMLParts.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsINameSpaceManager.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIStatefulFrame.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsHTMLParts.h"
#include "nsLinebreakConverter.h"
#include "nsILinkHandler.h"
#include "nsIHTMLDocument.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsICharsetConverterManager.h"
#include "nsEscape.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMKeyEvent.h"
#include "nsIFormControlFrame.h"
#include "nsINodeInfo.h"
#include "nsIDOMEventReceiver.h"
#include "nsContentCID.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"

nsIFrame*
NS_NewIsIndexFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsIsIndexFrame(aContext);
}

nsIsIndexFrame::nsIsIndexFrame(nsStyleContext* aContext) :
  nsAreaFrame(aContext)
{
  SetFlags(NS_BLOCK_SPACE_MGR);
}

nsIsIndexFrame::~nsIsIndexFrame()
{
  // remove ourself as a listener of the text control (bug 40533)
  if (mInputContent) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mInputContent));
    receiver->RemoveEventListenerByIID(this, NS_GET_IID(nsIDOMKeyListener));
  }
}

void
nsIsIndexFrame::Destroy()
{
  nsContentUtils::DestroyAnonymousContent(&mTextContent);
  nsContentUtils::DestroyAnonymousContent(&mInputContent);
  nsContentUtils::DestroyAnonymousContent(&mPreHr);
  nsContentUtils::DestroyAnonymousContent(&mPostHr);
  nsAreaFrame::Destroy();
}

// REVIEW: We don't need to override BuildDisplayList, nsAreaFrame will honour
// our visibility setting

NS_IMETHODIMP
nsIsIndexFrame::UpdatePromptLabel()
{
  if (!mTextContent) return NS_ERROR_UNEXPECTED;

  nsresult result = NS_OK;

  // Get the text from the "prompt" attribute.
  // If it is zero length, set it to a default value (localized)
  nsXPIDLString prompt;
  if (mContent)
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::prompt, prompt);

  if (prompt.IsEmpty()) {
    // Generate localized label.
    // We can't make any assumption as to what the default would be
    // because the value is localized for non-english platforms, thus
    // it might not be the string "This is a searchable index. Enter search keywords: "
    result =
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "IsIndexPrompt", prompt);
  }

  mTextContent->SetText(prompt, PR_TRUE);

  return NS_OK;
}

nsresult
nsIsIndexFrame::GetInputFrame(nsIFormControlFrame** oFrame)
{
  nsIPresShell *presShell = GetPresContext()->GetPresShell();
  if (!mInputContent) NS_WARNING("null content - cannot restore state");
  if (presShell && mInputContent) {
    nsIFrame *frame = presShell->GetPrimaryFrameFor(mInputContent);
    if (frame) {
      return CallQueryInterface(frame, oFrame);
    }
  }
  return NS_OK;
}

void
nsIsIndexFrame::GetInputValue(nsString& oString)
{
  nsIFormControlFrame* frame = nsnull;
  GetInputFrame(&frame);
  if (frame) {
    ((nsNewFrame*)frame)->GetValue(oString, PR_FALSE);
  }
}

void
nsIsIndexFrame::SetInputValue(const nsString& aString)
{
  nsIFormControlFrame* frame = nsnull;
  GetInputFrame(&frame);
  if (frame) {
    ((nsNewFrame*)frame)->SetValue(aString);
  }
}

void 
nsIsIndexFrame::SetFocus(PRBool aOn, PRBool aRepaint)
{
  nsIFormControlFrame* frame = nsnull;
  GetInputFrame(&frame);
  if (frame) {
    frame->SetFocus(aOn, aRepaint);
  }
}

nsresult
nsIsIndexFrame::CreateAnonymousContent(nsTArray<nsIContent*>& aElements)
{
  // Get the node info manager (used to create hr's and input's)
  nsCOMPtr<nsIDocument> doc = mContent->GetDocument();
  nsNodeInfoManager *nimgr = doc->NodeInfoManager();

  // Create an hr
  nsCOMPtr<nsINodeInfo> hrInfo;
  nimgr->GetNodeInfo(nsGkAtoms::hr, nsnull, kNameSpaceID_None,
                     getter_AddRefs(hrInfo));

  NS_NewHTMLElement(getter_AddRefs(mPreHr), hrInfo);
  if (!mPreHr || !aElements.AppendElement(mPreHr))
    return NS_ERROR_OUT_OF_MEMORY;

  // Add a child text content node for the label
  NS_NewTextNode(getter_AddRefs(mTextContent), nimgr);
  if (!mTextContent)
    return NS_ERROR_OUT_OF_MEMORY;

  // set the value of the text node and add it to the child list
  UpdatePromptLabel();
  if (!aElements.AppendElement(mTextContent))
    return NS_ERROR_OUT_OF_MEMORY;

  // Create text input field
  nsCOMPtr<nsINodeInfo> inputInfo;
  nimgr->GetNodeInfo(nsGkAtoms::input, nsnull, kNameSpaceID_None,
                     getter_AddRefs(inputInfo));

  NS_NewHTMLElement(getter_AddRefs(mInputContent), inputInfo);
  if (!mInputContent)
    return NS_ERROR_OUT_OF_MEMORY;

  mInputContent->SetAttr(kNameSpaceID_None, nsGkAtoms::type,
                         NS_LITERAL_STRING("text"), PR_FALSE);

  if (!aElements.AppendElement(mInputContent))
    return NS_ERROR_OUT_OF_MEMORY;

  // Register as an event listener to submit on Enter press
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(mInputContent));
  receiver->AddEventListenerByIID(this, NS_GET_IID(nsIDOMKeyListener));

  // Create an hr
  NS_NewHTMLElement(getter_AddRefs(mPostHr), hrInfo);
  if (!mPostHr || !aElements.AppendElement(mPostHr))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsIsIndexFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(NS_GET_IID(nsIAnonymousContentCreator))) {
    *aInstancePtr = (void*)(nsIAnonymousContentCreator*) this;
    return NS_OK;
  } else  if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  } else  if (aIID.Equals(NS_GET_IID(nsIDOMKeyListener))) {
    *aInstancePtr = (void*)(nsIDOMKeyListener*) this;
    return NS_OK;
  }
  return nsHTMLContainerFrame::QueryInterface(aIID, aInstancePtr);
}

nscoord
nsIsIndexFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  // Our min width is our pref width; the rest of our reflow is
  // happily handled by nsAreaFrame
  result = GetPrefWidth(aRenderingContext);
  return result;
}

PRBool
nsIsIndexFrame::IsLeaf() const
{
  return PR_TRUE;
}

NS_IMETHODIMP
nsIsIndexFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                 nsIAtom*        aAttribute,
                                 PRInt32         aModType)
{
  nsresult rv = NS_OK;
  if (nsGkAtoms::prompt == aAttribute) {
    rv = UpdatePromptLabel();
  } else {
    rv = nsAreaFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
  }
  return rv;
}


nsresult 
nsIsIndexFrame::KeyPress(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMKeyEvent> keyEvent = do_QueryInterface(aEvent);
  if (keyEvent) {
    PRUint32 code;
    keyEvent->GetKeyCode(&code);
    if (code == 0) {
      keyEvent->GetCharCode(&code);
    }
    if (nsIDOMKeyEvent::DOM_VK_RETURN == code) {
      OnSubmit(GetPresContext());
      aEvent->PreventDefault(); // XXX Needed?
    }
  }

  return NS_OK;
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsIsIndexFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("IsIndex"), aResult);
}
#endif

// submission
// much of this is cut and paste from nsFormFrame::OnSubmit
NS_IMETHODIMP
nsIsIndexFrame::OnSubmit(nsPresContext* aPresContext)
{
  if (!mContent || !mInputContent) {
    return NS_ERROR_UNEXPECTED;
  }

  nsresult result = NS_OK;

  // Begin ProcessAsURLEncoded
  nsAutoString data;

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  if(NS_FAILED(GetEncoder(getter_AddRefs(encoder))))  // Non-fatal error
     encoder = nsnull;

  nsAutoString value;
  GetInputValue(value);
  URLEncode(value, encoder, data);
  // End ProcessAsURLEncoded

  // make the url string
  nsILinkHandler *handler = aPresContext->GetLinkHandler();

  nsAutoString href;

  // Get the document.
  // We'll need it now to form the URL we're submitting to.
  // We'll also need it later to get the DOM window when notifying form submit observers (bug 33203)
  nsCOMPtr<nsIDocument> document = mContent->GetDocument();
  if (!document) return NS_OK; // No doc means don't submit, see Bug 28988

  // Resolve url to an absolute url
  nsIURI *baseURI = document->GetBaseURI();
  if (!baseURI) {
    NS_ERROR("No Base URL found in Form Submit!\n");
    return NS_OK; // No base URL -> exit early, see Bug 30721
  }

  // If an action is not specified and we are inside 
  // a HTML document then reload the URL. This makes us
  // compatible with 4.x browsers.
  // If we are in some other type of document such as XML or
  // XUL, do nothing. This prevents undesirable reloading of
  // a document inside XUL.

  nsresult rv;
  nsCOMPtr<nsIHTMLDocument> htmlDoc;
  htmlDoc = do_QueryInterface(document, &rv);
  if (NS_FAILED(rv)) {   
    // Must be a XML, XUL or other non-HTML document type
    // so do nothing.
    return NS_OK;
  } 

  // Necko's MakeAbsoluteURI doesn't reuse the baseURL's rel path if it is
  // passed a zero length rel path.
  nsCAutoString relPath;
  baseURI->GetSpec(relPath);
  if (!relPath.IsEmpty()) {
    CopyUTF8toUTF16(relPath, href);

    // If re-using the same URL, chop off old query string (bug 25330)
    PRInt32 queryStart = href.FindChar('?');
    if (kNotFound != queryStart) {
      href.Truncate(queryStart);
    }
  } else {
    NS_ERROR("Rel path couldn't be formed in form submit!\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Add the URI encoded form values to the URI
  // Get the scheme of the URI.
  nsCOMPtr<nsIURI> actionURL;
  nsXPIDLCString scheme;
  PRBool isJSURL = PR_FALSE;
  const nsACString &docCharset = document->GetDocumentCharacterSet();
  const nsPromiseFlatCString& flatDocCharset = PromiseFlatCString(docCharset);

  if (NS_SUCCEEDED(result = NS_NewURI(getter_AddRefs(actionURL), href,
                                      flatDocCharset.get(),
                                      baseURI))) {
    result = actionURL->SchemeIs("javascript", &isJSURL);
  }
  // Append the URI encoded variable/value pairs for GET's
  if (!isJSURL) { // Not for JS URIs, see bug 26917
    if (href.FindChar('?') == kNotFound) { // Add a ? if needed
      href.Append(PRUnichar('?'));
    } else {                              // Adding to existing query string
      if (href.Last() != '&' && href.Last() != '?') {   // Add a & if needed
        href.Append(PRUnichar('&'));
      }
    }
    href.Append(data);
  }
  nsCOMPtr<nsIURI> uri;
  result = NS_NewURI(getter_AddRefs(uri), href,
                     flatDocCharset.get(), baseURI);
  if (NS_FAILED(result)) return result;

  // Now pass on absolute url to the click handler
  if (handler) {
    handler->OnLinkClick(mContent, uri, nsnull);
  }
  return result;
}

void nsIsIndexFrame::GetSubmitCharset(nsCString& oCharset)
{
  oCharset.AssignLiteral("UTF-8"); // default to utf-8
  // XXX
  // We may want to get it from the HTML 4 Accept-Charset attribute first
  // see 17.3 The FORM element in HTML 4 for details

  // Get the charset from document
  nsIDocument* doc = mContent->GetDocument();
  if (doc) {
    oCharset = doc->GetDocumentCharacterSet();
  }
}

NS_IMETHODIMP nsIsIndexFrame::GetEncoder(nsIUnicodeEncoder** encoder)
{
  *encoder = nsnull;
  nsCAutoString charset;
  nsresult rv = NS_OK;
  GetSubmitCharset(charset);
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = CallGetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &ccm);
  if(NS_SUCCEEDED(rv) && (nsnull != ccm)) {
     rv = ccm->GetUnicodeEncoderRaw(charset.get(), encoder);
     NS_RELEASE(ccm);
     if (!*encoder) {
       rv = NS_ERROR_FAILURE;
     }
     if (NS_SUCCEEDED(rv)) {
       rv = (*encoder)->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, (PRUnichar)'?');
     }
  }
  return rv;
}

// XXX i18n helper routines
char*
nsIsIndexFrame::UnicodeToNewBytes(const PRUnichar* aSrc, PRUint32 aLen, nsIUnicodeEncoder* encoder)
{
   char* res = nsnull;
   if(NS_SUCCEEDED(encoder->Reset()))
   {
      PRInt32 maxByteLen = 0;
      if(NS_SUCCEEDED(encoder->GetMaxLength(aSrc, (PRInt32) aLen, &maxByteLen))) 
      {
          res = new char[maxByteLen+1];
          if(nsnull != res) 
          {
             PRInt32 reslen = maxByteLen;
             PRInt32 reslen2 ;
             PRInt32 srclen = aLen;
             encoder->Convert(aSrc, &srclen, res, &reslen);
             reslen2 = maxByteLen-reslen;
             encoder->Finish(res+reslen, &reslen2);
             res[reslen+reslen2] = '\0';
          }
      }

   }
   return res;
}

// XXX i18n helper routines
void
nsIsIndexFrame::URLEncode(const nsString& aString, nsIUnicodeEncoder* encoder, nsString& oString) 
{
  char* inBuf = nsnull;
  if(encoder)
    inBuf  = UnicodeToNewBytes(aString.get(), aString.Length(), encoder);

  if(nsnull == inBuf)
    inBuf  = ToNewCString(aString);

  // convert to CRLF breaks
  char* convertedBuf = nsLinebreakConverter::ConvertLineBreaks(inBuf,
                           nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakNet);
  delete [] inBuf;
  
  char* outBuf = nsEscape(convertedBuf, url_XPAlphas);
  oString.AssignASCII(outBuf);
  nsMemory::Free(outBuf);
  nsMemory::Free(convertedBuf);
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsIsIndexFrame::SaveState(SpecialStateID aStateID, nsPresState** aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  // Get the value string
  nsAutoString stateString;
  GetInputValue(stateString);

  nsresult res = NS_OK;
  if (! stateString.IsEmpty()) {

    // Construct a pres state and store value in it.
    res = NS_NewPresState(aState);
    NS_ENSURE_SUCCESS(res, res);
    res = (*aState)->SetStateProperty(NS_LITERAL_STRING("value"), stateString);
  }

  return res;
}

NS_IMETHODIMP
nsIsIndexFrame::RestoreState(nsPresState* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  // Set the value to the stored state.
  nsAutoString stateString;
  nsresult res = aState->GetStateProperty(NS_LITERAL_STRING("value"), stateString);
  NS_ENSURE_SUCCESS(res, res);

  SetInputValue(stateString);
  return NS_OK;
}
