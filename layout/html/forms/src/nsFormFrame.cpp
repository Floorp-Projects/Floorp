/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsCOMPtr.h"
#include "nsFormFrame.h"
#include "nsIFormControlFrame.h"
#include "nsFormControlFrame.h"
#include "nsFileControlFrame.h"
#include "nsRadioControlFrame.h"

#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIAtom.h"
#include "nsHTMLIIDs.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsCRT.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsILinkHandler.h"
#include "nsRadioControlFrame.h"
#include "nsIRadioButton.h"
#include "nsDocument.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsLeafFrame.h"
#include "nsHTMLParts.h"
#include "nsIReflowCommand.h"

// GetParentHTMLFrameDocument
#include "nsIWebShell.h"
#include "nsIContentViewerContainer.h"
#include "nsIDocumentViewer.h"

#include "net.h"
#include "xp_file.h"
#include "prio.h"
#include "prmem.h"
#include "prenv.h"

#if defined(ClientWallet) || defined(SingleSignon)
#include "nsIServiceManager.h"
#endif

// GetParentHTMLFrameDocument
static NS_DEFINE_IID(kIWebshellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIContentViewerContainerIID, NS_ICONTENT_VIEWER_CONTAINER_IID);
static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);

#ifdef SingleSignon
#define FORM_TYPE_TEXT          1
#define FORM_TYPE_PASSWORD      7
#endif

#if defined(ClientWallet) || defined(SingleSignon)
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif

//----------------------------------------------------------------------

static NS_DEFINE_IID(kIFormManagerIID, NS_IFORMMANAGER_IID);
static NS_DEFINE_IID(kIFormIID, NS_IFORM_IID);
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);
static NS_DEFINE_IID(kIFormControlFrameIID, NS_IFORMCONTROLFRAME_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLFormElementIID, NS_IDOMHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIDOMNSHTMLFormElementIID, NS_IDOMNSHTMLFORMELEMENT_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);
static NS_DEFINE_IID(kIFrameIID, NS_IFRAME_IID);

// XXX This crud needs to go! The FindFrameWithContent *will* be using
// a hashtable so this code will be redundant. In addition,
// FindFrameWithContent will be connected to the frame construction
// and guaranteed to be kept properly up to date.
static nsFormFrameTable* gFormFrameTable;

nsFormFrame*
nsFormFrame::GetFormFrame(nsIPresContext& aPresContext,
                          nsIDOMHTMLFormElement& aFormElem)
{ 
  if (nsnull == gFormFrameTable) {
    gFormFrameTable = new nsFormFrameTable();
  }
  return gFormFrameTable->Get(aPresContext, aFormElem);
}

void
nsFormFrame::PutFormFrame(nsIPresContext& aPresContext,
                          nsIDOMHTMLFormElement& aFormElem, 
                          nsFormFrame& aFrame)
{ 
  if (nsnull == gFormFrameTable) {
    gFormFrameTable = new nsFormFrameTable();
  }
  gFormFrameTable->Put(aPresContext, aFormElem, aFrame);
}

void
nsFormFrame::RemoveFormFrame(nsFormFrame& aFrame)
{
  if (nsnull == gFormFrameTable) {
    gFormFrameTable = new nsFormFrameTable();
  }
  gFormFrameTable->Remove(aFrame);
}

nsFormFrameTableEntry::
nsFormFrameTableEntry(nsIPresContext&        aPresContext, 
                      nsIDOMHTMLFormElement& aFormElement,
                      nsFormFrame&           aFormFrame) : mPresContext(&aPresContext), 
                                                           mFormElement(&aFormElement), 
                                                           mFormFrame(&aFormFrame) 
{
}

nsFormFrameTableEntry::~nsFormFrameTableEntry()
{
}

nsFormFrameTable::~nsFormFrameTable() 
{ 
  PRInt32 count = mEntries.Count(); 
  for (PRInt32 i = 0; i < count; i++) {
    delete mEntries.ElementAt(i);
  }
  mEntries.Clear();
}

nsFormFrame* 
nsFormFrameTable::Get(nsIPresContext& aPresContext, nsIDOMHTMLFormElement& aFormElem) 
{
  PRInt32 count = mEntries.Count();
  for (PRInt32 i = 0; i < count; i++) {
    nsFormFrameTableEntry* entry = (nsFormFrameTableEntry *)mEntries.ElementAt(i);
    if ((entry->mPresContext == &aPresContext) && (entry->mFormElement == &aFormElem)) {
      return entry->mFormFrame;
    }
  }
  return nsnull;
}
  
void 
nsFormFrameTable::Put(nsIPresContext& aPresContext, nsIDOMHTMLFormElement& aFormElem, 
                           nsFormFrame& aFormFrame) 
{
  NS_ADDREF(&aFormElem);
  mEntries.AppendElement(new nsFormFrameTableEntry(aPresContext, aFormElem, aFormFrame));
}

void
nsFormFrameTable::Remove(nsFormFrame& aFormFrame) 
{
  PRInt32 hits[10];
  PRInt32 hitIndex = 0;
  PRInt32 count = mEntries.Count();
  PRInt32 i;
  for (i = 0; i < count; i++) {
    nsFormFrameTableEntry* entry = (nsFormFrameTableEntry *)mEntries.ElementAt(i);
    if (entry->mFormFrame == &aFormFrame) {
      hits[hitIndex] = i;
      hitIndex++;
      NS_IF_RELEASE(entry->mFormElement);
    }
  }
  for (i = hitIndex-1; i >= 0; i--) {
    delete mEntries.ElementAt(i);
    mEntries.RemoveElementAt(i);
  }
}


NS_IMETHODIMP
nsFormFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(kIFormManagerIID)) {
    *aInstancePtr = (void*)(nsIFormManager*)this;
    return NS_OK;
  }
  return nsLeafFrame::QueryInterface(aIID, aInstancePtr);
}

nsrefcnt nsFormFrame::AddRef(void)
{
  NS_ERROR("not supported");
  return 0;
}

nsrefcnt nsFormFrame::Release(void)
{
  NS_ERROR("not supported");
  return 0;
}

nsFormFrame::nsFormFrame()
  : nsLeafFrame()
{
  mTextSubmitter = nsnull;
}

nsFormFrame::~nsFormFrame()
{
  mFormControls.Clear();
  RemoveFormFrame(*this);
  RemoveRadioGroups();
}

PRBool 
nsFormFrame::CanSubmit(nsFormControlFrame& aFrame)
{ 
  if (mTextSubmitter == &aFrame) {
    return PR_TRUE;
  }
  PRInt32 type;
  aFrame.GetType(&type);
  if ((NS_FORM_INPUT_SUBMIT == type) || (NS_FORM_INPUT_IMAGE == type)) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

void 
nsFormFrame::GetDesiredSize(nsIPresContext* aPresContext,
                            const nsHTMLReflowState& aReflowState,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width   = 0;
  aDesiredSize.height  = 0;
  aDesiredSize.ascent  = 0;
  aDesiredSize.descent = 0;
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = 0;
    aDesiredSize.maxElementSize->height = 0;
  }
}

NS_IMETHODIMP
nsFormFrame::GetAction(nsString* aAction)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsIDOMHTMLFormElement* form = nsnull;
    result = mContent->QueryInterface(kIDOMHTMLFormElementIID, (void**)&form);
    if ((NS_OK == result) && form) {
      form->GetAction(*aAction);
      NS_RELEASE(form);
    }
  }
  return result;
}

NS_IMETHODIMP
nsFormFrame::GetTarget(nsString* aTarget)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsIDOMHTMLFormElement* form = nsnull;
    result = mContent->QueryInterface(kIDOMHTMLFormElementIID, (void**)&form);
    if ((NS_OK == result) && form) {
      form->GetTarget(*aTarget);
      NS_RELEASE(form);
    }
  }
  return result;
}

NS_IMETHODIMP
nsFormFrame::GetMethod(PRInt32* aMethod)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsIHTMLContent* form = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&form);
    if ((NS_OK == result) && form) {
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_HAS_VALUE == (form->GetHTMLAttribute(nsHTMLAtoms::method, value))) {
        if (eHTMLUnit_Enumerated == value.GetUnit()) {
          *aMethod = value.GetIntValue();
          NS_RELEASE(form);
          return NS_OK;
        }
      }
      NS_RELEASE(form);
    }
  }
  *aMethod = NS_FORM_METHOD_GET;
  return result;
}

NS_IMETHODIMP
nsFormFrame::GetEnctype(PRInt32* aEnctype)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsIHTMLContent* form = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&form);
    if ((NS_OK == result) && form) {
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_HAS_VALUE == (form->GetHTMLAttribute(nsHTMLAtoms::enctype, value))) {
        if (eHTMLUnit_Enumerated == value.GetUnit()) {
          *aEnctype = value.GetIntValue();
          NS_RELEASE(form);
          return NS_OK;
        }
      }
      NS_RELEASE(form);
    }
  }
  *aEnctype = NS_FORM_ENCTYPE_URLENCODED;
  return result;
}

NS_IMETHODIMP
nsFormFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList)
{
  nsresult result = NS_OK;
  nsIDOMHTMLFormElement* content = nsnull;
  if (mContent) {
    nsresult result = mContent->QueryInterface(kIDOMHTMLFormElementIID, (void**)&content);
    if ((NS_OK == result) && (nsnull != content)) {
      nsFormFrame::PutFormFrame(aPresContext, *content, *this);
      NS_RELEASE(content);
    }
  }
  return result;
}

NS_IMETHODIMP
nsFormFrame::Reflow(nsIPresContext&      aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&      aStatus)
{
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_IMETHODIMP 
nsFormFrame::OnReset()
{
  PRInt32 numControls = mFormControls.Count();
  for (int i = 0; i < numControls; i++) {
	  nsIFormControlFrame* fcFrame = (nsIFormControlFrame*) mFormControls.ElementAt(i);
    fcFrame->Reset();
  }
  return NS_OK;
}

void nsFormFrame::RemoveRadioGroups()
{
  int numRadioGroups = mRadioGroups.Count();
  for (int i = 0; i < numRadioGroups; i++) {
    nsRadioControlGroup* radioGroup = (nsRadioControlGroup *) mRadioGroups.ElementAt(i);
    delete radioGroup;
  }
  mRadioGroups.Clear();
}

void nsFormFrame::AddFormControlFrame(nsIPresContext& aPresContext, nsIFrame& aFrame)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aFrame.QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  if ((NS_OK != result) || (nsnull == fcFrame)) {
    return;
  }

  nsIContent* iContent = nsnull;
  aFrame.GetContent(&iContent);
  if (nsnull != iContent) {
    nsIFormControl* formControl = nsnull;
    result = iContent->QueryInterface(kIFormControlIID, (void**)&formControl);
    if ((NS_OK == result) && (nsnull != formControl)) {
      nsIDOMHTMLFormElement* formElem = nsnull;
      result = formControl->GetForm(&formElem);
      if (nsnull != formElem) {
        nsFormFrame* formFrame = nsFormFrame::GetFormFrame(aPresContext, *formElem);
        if (nsnull != formFrame) {
          formFrame->AddFormControlFrame(*fcFrame);
          fcFrame->SetFormFrame(formFrame);
        }
        NS_RELEASE(formElem);
      }
      NS_RELEASE(formControl);
    }
    NS_RELEASE(iContent);
  }
}

void nsFormFrame::AddFormControlFrame(nsIFormControlFrame& aFrame)
{
  mFormControls.AppendElement(&aFrame);

  // determine which radio buttons belong to which radio groups, unnamed radio buttons
  // don't go into any group since they can't be submitted. Determine which controls
  // are capable of form submission.

  nsString name;
  name.SetLength(0);
  aFrame.GetName(&name);
  PRBool hasName = name.Length() > 0;
  PRInt32 type;
  aFrame.GetType(&type);

  // a solo text control can be a submitter (if return is hit)
  if (NS_FORM_INPUT_TEXT == type) {
    mTextSubmitter = (nsnull == mTextSubmitter) ? &aFrame : nsnull;
  }

  // radio group processing
  if (hasName && (NS_FORM_INPUT_RADIO == type)) { 
    nsRadioControlFrame* radioFrame = (nsRadioControlFrame*)&aFrame;
    int numGroups = mRadioGroups.Count();
    PRBool added = PR_FALSE;
    nsRadioControlGroup* group;
    for (int j = 0; j < numGroups; j++) {
      group = (nsRadioControlGroup *) mRadioGroups.ElementAt(j);
      nsString groupName;
      group->GetName(groupName);
      if (groupName.Equals(name)) {
        group->AddRadio(radioFrame);
        added = PR_TRUE;
        break;
      }
    }
    if (!added) {
      group = new nsRadioControlGroup(name);
      mRadioGroups.AppendElement(group);
      group->AddRadio(radioFrame);
    }
    // allow only one checked radio button
    if (radioFrame->GetChecked(PR_TRUE)) {
	    if (nsnull == group->GetCheckedRadio()) {
	      group->SetCheckedRadio(radioFrame);
	    } else {
	      radioFrame->SetChecked(PR_FALSE, PR_TRUE);
	    }
    }
  }
}
  
void
nsFormFrame::OnRadioChecked(nsRadioControlFrame& aControl, PRBool aChecked)
{
  nsString radioName;
  aControl.GetName(&radioName);
  if (0 == radioName.Length()) { // don't consider a radio without a name 
    return;
  }
 
  // locate the radio group with the name of aRadio
  int numGroups = mRadioGroups.Count();
  for (int j = 0; j < numGroups; j++) {
    nsRadioControlGroup* group = (nsRadioControlGroup *) mRadioGroups.ElementAt(j);
    nsString groupName;
    group->GetName(groupName);
    nsRadioControlFrame* checkedRadio = group->GetCheckedRadio();
    if (groupName.Equals(radioName)) {
      if (aChecked) {
        if (&aControl != checkedRadio) {
          if (checkedRadio) {
            checkedRadio->SetChecked(PR_FALSE, PR_FALSE);
          }
          group->SetCheckedRadio(&aControl);
        }
      } else if (&aControl == checkedRadio) {
        checkedRadio->SetChecked(PR_FALSE, PR_FALSE);
      }
    }
  }
}


nsresult
NS_NewFormFrame(nsIFrame*& aResult)
{
  nsIFrame* frame = new nsFormFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}


PRIVATE
void DebugPrint(char* aLabel, nsString aString)
{
  char* out = aString.ToNewCString();
  printf("\n %s=%s\n", aLabel, out);
  delete [] out;
}

// submission

NS_IMETHODIMP
nsFormFrame::OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  if (!mContent) {
    return NS_FORM_NOTOK;
  }

  nsString data; // this could be more efficient, by allocating a larger buffer

  PRInt32 method, enctype;
  GetMethod(&method);
  GetEnctype(&enctype);

  PRBool isURLEncoded = (NS_FORM_ENCTYPE_MULTIPART != enctype);

  // for enctype=multipart/form-data, force it to be post
  // if method is "" (not specified) use "get" as default
  PRBool isPost = (NS_FORM_METHOD_POST == method) || !isURLEncoded; 

  nsIFormControlFrame* fcFrame = nsnull;

  // Since JS Submit() calls are not linked to an element, aFrame is null.
  // fcframe will remain null, but IsSuccess will return succes in this case.
  if (aFrame != nsnull) {
    aFrame->QueryInterface(kIFormControlFrameIID, (void**)&fcFrame);
  }
  
  if (isURLEncoded) {
    ProcessAsURLEncoded(isPost, data, fcFrame);
  }
  else {
    ProcessAsMultipart(data, fcFrame);
  }


  // make the url string
  nsILinkHandler* handler;
  if (NS_OK == aPresContext->GetLinkHandler(&handler)) {
    // Resolve url to an absolute url
    nsIURL* docURL = nsnull;
    nsIDocument* doc = nsnull;
    mContent->GetDocument(doc);
    while (doc && !docURL) {
      doc->GetBaseURL(docURL);
      if (!docURL) {
        doc = GetParentHTMLFrameDocument(doc);
        if (!doc) break;
      }
    }
    NS_IF_RELEASE(doc);

    nsAutoString target;
    GetTarget(&target);

    nsAutoString href;
    GetAction(&href);
    if (!isPost) {
      href.Append(data);
    }
    nsAutoString absURLSpec;
    nsAutoString base;
    NS_MakeAbsoluteURL(docURL, base, href, absURLSpec);
    NS_IF_RELEASE(docURL);

#ifdef ClientWallet
#ifndef HTMLDialogs 
    if (href == "internal-walletPrefill-handler") {
      nsresult res;
      nsIWalletService *walletservice;
      res = nsServiceManager::GetService(kWalletServiceCID,
                                         kIWalletServiceIID,
                                         (nsISupports **)&walletservice);
      if ((NS_OK == res) && (nsnull != walletservice)) {
        res = walletservice->WALLET_Prefill(nsnull, PR_FALSE);
        NS_RELEASE(walletservice);
        return NS_OK;
      }
    }
    if (href == "internal-walletEditor-handler") {
      nsresult res;
      nsIWalletService *walletservice;
      res = nsServiceManager::GetService(kWalletServiceCID,
                                         kIWalletServiceIID,
                                         (nsISupports **)&walletservice);
      if ((NS_OK == res) && (nsnull != walletservice)) {
        res = walletservice->WALLET_PreEdit(nsnull);
        NS_RELEASE(walletservice);
        return NS_OK;
      }
    }
#endif
#endif

    // Now pass on absolute url to the click handler
    nsIPostData* postData = nsnull;
    if (isPost) {
      nsresult rv;
      char* postBuffer = data.ToNewCString();

      rv = NS_NewPostData(!isURLEncoded, postBuffer, &postData);
      if (NS_OK != rv) {
        delete [] postBuffer;
      }

      /* The postBuffer is now owned by the IPostData instance */
    }    
    handler->OnLinkClick(mContent, eLinkVerb_Replace, absURLSpec, target, postData);
    NS_IF_RELEASE(postData);
    NS_RELEASE(handler);

DebugPrint("url", absURLSpec);
DebugPrint("data", data);
  }
  return NS_OK;
}

// netlib has a general function (netlib\modules\liburl\src\escape.c) 
// which does url encoding. Since netlib is not yet available for raptor,
// the following will suffice. Convert space to +, don't convert alphanumeric,
// conver each non alphanumeric char to %XY where XY is the hexadecimal 
// equavalent of the binary representation of the character. 
// 
PRIVATE
void URLEncode(char* aInString, char* aOutString)
{
  if (nsnull == aInString) {
	return;
  }
  static char *toHex = "0123456789ABCDEF";
  char* outChar = aOutString;
  for (char* inChar = aInString; *inChar; inChar++) {
    if(' ' == *inChar) {                                     // convert space to +
	    *outChar++ = '+';
	  } else if ( (((*inChar - '0') >= 0) && (('9' - *inChar) >= 0)) ||   // don't conver alphanumeric
                (((*inChar - 'a') >= 0) && (('z' - *inChar) >= 0)) ||   // or '.' or '_'
				        (((*inChar - 'A') >= 0) && (('Z' - *inChar) >= 0)) ||
                ('.' == *inChar) || ('_' == *inChar)) {
	    *outChar++ = *inChar;
	  } else {                                                 // convert all else to hex
	    *outChar++ = '%';
      *outChar++ = toHex[(*inChar >> 4) & 0x0F];
      *outChar++ = toHex[*inChar & 0x0F];
	  }
  }
  *outChar = 0;  // terminate the string
}

PRIVATE
nsString* URLEncode(nsString& aString) 
{  
  char* inBuf  = aString.ToNewCString();
	char* outBuf = new char[ (strlen(inBuf) * 3) + 1 ];
	URLEncode(inBuf, outBuf);
	nsString* result = new nsString(outBuf);
	delete [] outBuf;
	delete [] inBuf;
	return result;
}

// Hack to get the document from the parent HTML Frame.
// We need this to find the base URL for submitting forms that are created in Javascript
nsIDocument*
nsFormFrame::GetParentHTMLFrameDocument(nsIDocument* doc) {
  nsIDocument* parentDocument = nsnull;
  nsIScriptContextOwner* webshellOwner = nsnull;
  if (!doc) return nsnull;
  if ((webshellOwner = doc->GetScriptContextOwner())) {
    nsIWebShell* webshell = nsnull;
    if (NS_OK == webshellOwner->QueryInterface(kIWebshellIID, (void **)&webshell)) {
      nsIWebShell* pWebshell = nsnull;
      if (NS_OK == webshell->GetParent(pWebshell)) {
        nsIContentViewerContainer* pContentViewerContainer = nsnull;
        if (NS_OK == pWebshell->QueryInterface(kIContentViewerContainerIID, (void **)&pContentViewerContainer)) {
          nsIContentViewer* pContentViewer = nsnull;
          if (NS_OK == pContentViewerContainer->GetContentViewer(&pContentViewer)) {
            nsIDocumentViewer* pDocumentViewer;
            if (NS_OK == pContentViewer->QueryInterface(kIDocumentViewerIID, (void **)&pDocumentViewer)) {
              nsIDocument* pDocument = nsnull;
              if (NS_OK == pDocumentViewer->GetDocument(pDocument)) {
                parentDocument = pDocument;
                NS_RELEASE(doc); // Release the child doc for the caller
              }
              NS_RELEASE (pDocumentViewer);
            }
            NS_RELEASE(pContentViewer);
          }
          NS_RELEASE(pContentViewerContainer);
        }
        NS_RELEASE(pWebshell);
      }
      NS_RELEASE(webshell);
     }
     NS_RELEASE(webshellOwner);
  }
  return parentDocument;
}

#define CRLF "\015\012"   
void nsFormFrame::ProcessAsURLEncoded(PRBool isPost, nsString& aData, nsIFormControlFrame* aFrame)
{
  nsString buf;
  PRBool firstTime = PR_TRUE;
  PRUint32 numChildren = mFormControls.Count();

#if defined(ClientWallet) || defined(SingleSignon)
  /* get url name as ascii string */
  char *URLName = nsnull;
  nsIURL* docURL = nsnull;
  nsIDocument* doc = nsnull;
  mContent->GetDocument(doc);

  while (doc && !docURL) {
    docURL = doc->GetDocumentURL();
    if (!docURL) {
      doc = GetParentHTMLFrameDocument(doc);
      if (!doc) break;
    }
  }
  if (nsnull != docURL) {
    const char* spec;
    (void)docURL->GetSpec(&spec);
    URLName = (char*)PR_Malloc(PL_strlen(spec)+1);
    PL_strcpy(URLName, spec);
    NS_IF_RELEASE(docURL);
  }
#endif

#ifdef SingleSignon
#define MAX_ARRAY_SIZE 50
  char* name_array[MAX_ARRAY_SIZE];
  char* value_array[MAX_ARRAY_SIZE];
  uint8 type_array[MAX_ARRAY_SIZE];
  PRInt32 value_cnt = 0;
#endif

#ifdef ClientWallet
  /* determine if form is significant enough to capture data for */
  PRBool OKToCapture = FALSE;
  PRInt32 count = 0;
  PRUint32 numChildren2 = mFormControls.Count();
  for (PRUint32 childX2 = 0; childX2 < numChildren2; childX2++) {
    nsIFormControlFrame* child = (nsIFormControlFrame*) mFormControls.ElementAt(childX2);
    if (child && child->IsSuccessful(aFrame)) {
      PRInt32 type;
      child->GetType(&type);
      if (type == NS_FORM_INPUT_TEXT) {
        count++;
      }
    }
  }
  nsIWalletService *service2;
  nsresult res2 = nsServiceManager::GetService(kWalletServiceCID,
    kIWalletServiceIID,
    (nsISupports **)&service2);
  if ((NS_OK == res2) && (nsnull != service2)) {
    service2->WALLET_OKToCapture(&OKToCapture, count, URLName);
    NS_RELEASE(service2);
  }
#endif

  // collect and encode the data from the children controls
  for (PRUint32 childX = 0; childX < numChildren; childX++) {
	  nsIFormControlFrame* child = (nsIFormControlFrame*) mFormControls.ElementAt(childX);
	  if (child && child->IsSuccessful(aFrame)) {
		  PRInt32 numValues = 0;
		  PRInt32 maxNumValues = child->GetMaxNumValues();
			if (maxNumValues <= 0) {
				continue;
			}
		  nsString* names = new nsString[maxNumValues];
		  nsString* values = new nsString[maxNumValues];
			if (PR_TRUE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
#if defined(ClientWallet) || defined(SingleSignon)
				PRInt32 type;
				child->GetType(&type);
#endif
#ifdef ClientWallet
                              if (OKToCapture && (NS_FORM_INPUT_TEXT == type)) {
                                      nsIWalletService *service;
                                      nsresult res = nsServiceManager::GetService(kWalletServiceCID,
                                              kIWalletServiceIID,
                                              (nsISupports **)&service);
                                      if ((NS_OK == res) && (nsnull != service)) {
                                              res = service->WALLET_Capture(doc, *names, *values);
                                              NS_RELEASE(service);
                                      }
                              }
#endif
#ifdef SingleSignon
				if ((type == NS_FORM_INPUT_PASSWORD) || (type == NS_FORM_INPUT_TEXT)) {
					if (type == NS_FORM_INPUT_PASSWORD) {
						type_array[value_cnt] = FORM_TYPE_PASSWORD;
					} else {
						type_array[value_cnt] = FORM_TYPE_TEXT;
					}
					value_array[value_cnt] =
						values[0].ToNewCString();
					name_array[value_cnt] =
						names[0].ToNewCString();
					value_cnt++;
				}
#endif
				for (int valueX = 0; valueX < numValues; valueX++) {
				  if (PR_TRUE == firstTime) {
					  firstTime = PR_FALSE;
				  } else {
				    buf += "&";
				  }
					nsString* convName = URLEncode(names[valueX]);
				  buf += *convName;
					delete convName;
					buf += "=";
					nsString* convValue = URLEncode(values[valueX]);
					buf += *convValue;
					delete convValue;
				}
			}
      delete [] names;
			delete [] values;
		}
	}

#ifdef SingleSignon
  nsIWalletService *service;
  nsresult res = nsServiceManager::GetService(kWalletServiceCID,
                                          kIWalletServiceIID,
                                          (nsISupports **)&service);
  if ((NS_OK == res) && (nsnull != service)) {
	  res = service->SI_RememberSignonData
		(URLName, (char**)name_array, (char**)value_array, (char**)type_array, value_cnt);
	  NS_RELEASE(service);
  }
  while (value_cnt--) {
	PR_FREEIF(name_array[value_cnt]);
	PR_FREEIF(value_array[value_cnt]);
  }
#endif
#if defined(ClientWallet) || defined(SingleSignon)
  if (nsnull != doc) {
        NS_RELEASE(doc);
  }
  if (nsnull != URLName) {
        PR_FREEIF(URLName);
  }
#endif

  aData.SetLength(0);
  if (isPost) {
    char size[16];
    sprintf(size, "%d", buf.Length());
    aData = "Content-type: application/x-www-form-urlencoded";
    aData += CRLF;
    aData += "Content-Length: ";
    aData += size;
    aData += CRLF;
    aData += CRLF;
  }
  else {
    aData += '?';
  }

  aData += buf;
}

// include the file name without the directory
const char* nsFormFrame::GetFileNameWithinPath(char* aPathName)
{
  char* fileNameStart = PL_strrchr(aPathName, '\\'); // windows
  if (!fileNameStart) { // try unix
    fileNameStart = PL_strrchr(aPathName, '\\');
  }
  if (fileNameStart) { 
    return fileNameStart+1;
  }
  else {
    return aPathName;
  }
}

// this needs to be provided by a higher level, since navigator might override
// the temp directory. XXX does not check that parm is big enough
PRBool
nsFormFrame::Temp_GetTempDir(char* aTempDirName)
{
  aTempDirName[0] = 0;
  const char* env;

  if ((env = (const char *) getenv("TMP")) == nsnull) {
    if ((env = (const char *) getenv("TEMP")) == nsnull) {
	    strcpy(aTempDirName, ".");
    }
  }
  if (*env == '\0')	{	// null string means "." 
    strcpy(aTempDirName, ".");
  }
  if (0 == aTempDirName[0]) {
    strcpy(aTempDirName, env);
  }
  return PR_TRUE;
}

// the following is a temporary measure until NET_cinfo_find_type or its
// replacement is available
void nsFormFrame::Temp_GetContentType(char* aPathName, char* aContentType)
{
  if (!aPathName) {
    strcpy(aContentType, "unknown");
    return;
  }

  int len = strlen(aPathName);
  if (len <= 0) {
    strcpy(aContentType, "unknown");
    return;
  }

  char* fileExt = &aPathName[len-1];
  for (int i = len-1; i >= 0; i--) {
    if ('.' == aPathName[i]) {
      break;
    }
    fileExt--;
  }
  if ((0 == nsCRT::strcasecmp(fileExt, ".html")) ||
      (0 == nsCRT::strcasecmp(fileExt, ".htm"))) {
    strcpy(aContentType, "text/html");
  }
  else if (0 == nsCRT::strcasecmp(fileExt, ".txt")) { 
    strcpy(aContentType, "text/plain");
  }
  else if (0 == nsCRT::strcasecmp(fileExt, ".gif")) { 
    strcpy(aContentType, "image/gif");
  }
  else if ((0 == nsCRT::strcasecmp(fileExt, ".jpeg")) ||
           (0 == nsCRT::strcasecmp(fileExt, ".jpg"))) {
    strcpy(aContentType, "image/jpeg");
  }
  else { // don't bother trying to do the others here
    strcpy(aContentType, "unknown");
  }
}


#if 0
  char* tmpDir = aFileName;		// copy name to fname */
  // Keep generating file names till we find one that's not in use
    while ((*env != '\0') && count++ {
      *ptr++ = *env++;
    }
    if (ptr[-1] != '\\' && ptr[-1] != '/')
      *ptr++ = '\\';		/* append backslash if not in env variable */
    /* Append a suitable file name */
    next_file_num++;		/* advance counter */
    sprintf(ptr, "JPG%03d.TMP", next_file_num);
    /* Probe to see if file name is already in use */
    if ((tfile = fopen(fname, READ_BINARY)) == NULL)
      break;
    fclose(tfile);		/* oops, it's there; close tfile & try again */
  }

  char* tempDir = getenv("temp");
  if (!tempDir) {
    tempDir = getenv("tmp");
  }
  if (tempDir) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
#endif

#define CONTENT_DISP "Content-Disposition: form-data; name=\""
#define FILENAME "\"; filename=\""
#define CONTENT_TYPE "Content-Type: "
#define CONTENT_ENCODING "Content-Encoding: "
#define BUFSIZE 1024
#define MULTIPART "multipart/form-data"
#define END "--"

void nsFormFrame::ProcessAsMultipart(nsString& aData, nsIFormControlFrame* aFrame)
{
  aData.SetLength(0);
  char buffer[BUFSIZE];
  PRInt32 numChildren = mFormControls.Count();

  // construct a temporary file to put the data into 
  char tmpFileName[BUFSIZE];
  char* result = Temp_GenerateTempFileName((PRInt32)BUFSIZE, tmpFileName);
  if (!result) {
    return;
  }

  PRFileDesc* tmpFile = PR_Open(tmpFileName, PR_CREATE_FILE | PR_WRONLY, 0644);
  if (!tmpFile) {
    return;
	}

  // write the content-type, boundary to the tmp file
	char boundary[80];
  sprintf(boundary, "-----------------------------%d%d%d", 
          rand(), rand(), rand());
  sprintf(buffer, "Content-type: %s; boundary=%s" CRLF, MULTIPART, boundary);
	PRInt32 len = PR_Write(tmpFile, buffer, PL_strlen(buffer));
  if (len < 0) {
    return;
  }

  PRInt32 boundaryLen = PL_strlen(boundary);
	PRInt32	contDispLen = PL_strlen(CONTENT_DISP);
  PRInt32 crlfLen     = PL_strlen(CRLF);

  // compute the content length, passing through all of the form controls
	PRInt32 contentLen = crlfLen; // extra crlf after content-length header

  PRInt32 childX;  // stupid compiler
  for (childX = 0; childX < numChildren; childX++) {
    nsIFormControlFrame* child = (nsIFormControlFrame*) mFormControls.ElementAt(childX);
    if (child) {
      PRInt32 type;
      child->GetType(&type);
      if (child->IsSuccessful(aFrame)) {
		    PRInt32 numValues = 0;
		    PRInt32 maxNumValues = child->GetMaxNumValues();
			  if (maxNumValues <= 0) {
          continue;
        }
		    nsString* names  = new nsString[maxNumValues];
		    nsString* values = new nsString[maxNumValues];
			  if (PR_FALSE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
          continue;
        }
        contentLen += boundaryLen + crlfLen;
        contentLen += contDispLen;
		    for (int valueX = 0; valueX < numValues; valueX++) {
          char* name  = names[valueX].ToNewCString();
          char* value = values[valueX].ToNewCString();
          if ((0 == names[valueX].Length()) || (0 == values[valueX].Length())) {
            continue;
          }
				  contentLen += PL_strlen(name);
			    contentLen += 1 + crlfLen;  // ending name quote plus CRLF
          if (NS_FORM_INPUT_FILE == type) { 
            contentLen += PL_strlen(FILENAME);

            // include the file name without the directory
            char* fileNameStart = PL_strrchr(value, '/'); // unix
            if (!fileNameStart) { // try windows
              fileNameStart = PL_strrchr(value, '\\');
            }
            fileNameStart = (fileNameStart) ? fileNameStart+1 : value; 
			      contentLen += PL_strlen(fileNameStart);

					  // determine the content-type of the file

            char contentType[128];
					  Temp_GetContentType(value, &contentType[0]);
				    contentLen += PL_strlen(CONTENT_TYPE);
					  contentLen += PL_strlen(contentType) + crlfLen + crlfLen;

			      // get the size of the file
				    PRFileInfo fileInfo;
				    if (PR_SUCCESS == PR_GetFileInfo(value, &fileInfo)) {
					    contentLen += fileInfo.size;
            }
				  }
          else {
            contentLen += PL_strlen(value) + crlfLen;
          }
          delete [] name;
          delete [] value;
        }
 			  delete [] names;
			  delete [] values;
		  }

      aData = tmpFileName;
    }
	}

  contentLen += boundaryLen + PL_strlen(END) + crlfLen;

  // write the content 
  sprintf(buffer, "Content-Length: %d" CRLF CRLF, contentLen);
	PR_Write(tmpFile, buffer, PL_strlen(buffer));

  // write the content passing through all of the form controls a 2nd time
  for (childX = 0; childX < numChildren; childX++) {
	  nsIFormControlFrame* child = (nsIFormControlFrame*) mFormControls.ElementAt(childX);
    if (child) {
      PRInt32 type;
      child->GetType(&type);
      if (child->IsSuccessful(aFrame)) {
		    PRInt32 numValues = 0;
		    PRInt32 maxNumValues = child->GetMaxNumValues();
			  if (maxNumValues <= 0) {
          continue;
        }
		    nsString* names  = new nsString[maxNumValues];
		    nsString* values = new nsString[maxNumValues];
			  if (PR_FALSE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
          continue;
        }
		    for (int valueX = 0; valueX < numValues; valueX++) {
          char* name  = names[valueX].ToNewCString();
          char* value = values[valueX].ToNewCString();
          if ((0 == names[valueX].Length()) || (0 == values[valueX].Length())) {
            continue;
          }
	        sprintf(buffer, "%s" CRLF, boundary);
		      PR_Write(tmpFile, buffer, PL_strlen(buffer));
			    PR_Write(tmpFile, CONTENT_DISP, contDispLen);
				  PR_Write(tmpFile, name, PL_strlen(name));

          if (NS_FORM_INPUT_FILE == type) {
				    PR_Write(tmpFile, FILENAME, PL_strlen(FILENAME));
            const char* fileNameStart = GetFileNameWithinPath(value);
            PR_Write(tmpFile, fileNameStart, PL_strlen(fileNameStart)); 
          }
			    PR_Write(tmpFile, "\"" CRLF, PL_strlen("\"" CRLF)); // end content disp

          if (NS_FORM_INPUT_FILE == type) {
					  // determine the content-type of the file
            char contentType[128];
					  Temp_GetContentType(value, &contentType[0]);
	          PR_Write(tmpFile, CONTENT_TYPE, PL_strlen(CONTENT_TYPE));
	          PR_Write(tmpFile, contentType, PL_strlen(contentType));
				    PR_Write(tmpFile, CRLF, PL_strlen(CRLF));
			      PR_Write(tmpFile, CRLF, PL_strlen(CRLF)); // end content-type header

            PRFileDesc* contentFile = PR_Open(value, PR_RDONLY, 0644);
					  if(contentFile) {
              PRInt32 size;
						  while((size = PR_Read(contentFile, buffer, BUFSIZE)) > 0) {
							  PR_Write(tmpFile, buffer, size);
						  }
						  PR_Close(contentFile);
					  }
				  }
          else {
 					  PR_Write(tmpFile, value, PL_strlen(value));
            PR_Write(tmpFile, CRLF, crlfLen);
          }
          delete [] name;
          delete [] value;
        }
 			  delete [] names;
			  delete [] values;
      }
		}
	}

	sprintf(buffer, "%s--" CRLF, boundary);
  PR_Write(tmpFile, buffer, PL_strlen(buffer));

  PR_Close(tmpFile);
	
	//StrAllocCopy(url_struct->post_data, tmpfilename);
	//url_struct->post_data_is_file = TRUE;
}

// THE FOLLOWING WAS TAKEN FROM CMD/WINFE/FEGUI AND MODIFIED TO JUST 
// GENERATE A TEMPFILE NAME. 


// Windows _tempnam() lets the TMP environment variable override things sent in
//  so it look like we're going to have to make a temp name by hand
//
// The user should *NOT* free the returned string.  It is stored in static space
//  and so is not valid across multiple calls to this function
//
// The names generated look like
//   c:\netscape\cache\m0.moz
//   c:\netscape\cache\m1.moz
// up to...
//   c:\netscape\cache\m9999999.moz
// after that if fails
//
char* nsFormFrame::Temp_GenerateTempFileName(PRInt32 aMaxSize, char* file_buf)
{
  char directory[128];
  Temp_GetTempDir(&directory[0]);
  static char ext[] = ".TMP";
  static char prefix[] = "nsform";

  //  We need to base our temporary file names on time, and not on sequential
  //    addition because of the cache not being updated when the user
  //    crashes and files that have been deleted are over written with
  //    other files; bad data.
  //  The 52 valid DOS file name characters are
  //    0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_^$~!#%&-{}@`'()
  //  We will only be using the first 32 of the choices.
  //
  //  Time name format will be M+++++++.MOZ
  //    Where M is the single letter prefix (can be longer....)
  //    Where +++++++ is the 7 character time representation (a full 8.3
  //      file name will be made).
  //    Where .MOZ is the file extension to be used.
  //
  //  In the event that the time requested is the same time as the last call
  //    to this function, then the current time is incremented by one,
  //    as is the last time called to facilitate unique file names.
  //  In the event that the invented file name already exists (can't
  //    really happen statistically unless the clock is messed up or we
  //    manually incremented the time), then the times are incremented
  //    until an open name can be found.
  //
  //  time_t (the time) has 32 bits, or 4,294,967,296 combinations.
  //  We will map the 32 bits into the 7 possible characters as follows:
  //    Starting with the lsb, sets of 5 bits (32 values) will be mapped
  //      over to the appropriate file name character, and then
  //      incremented to an approprate file name character.
  //    The left over 2 bits will be mapped into the seventh file name
  //      character.
  //
  
  int i_letter, i_timechars, i_numtries = 0;
  char ca_time[8];
  time_t this_call = (time_t)0;
  
  //  We have to base the length of our time string on the length
  //    of the incoming prefix....
  //
  i_timechars = 8 - strlen(prefix);
  
  //  Infinite loop until the conditions are satisfied.
  //  There is no danger, unless every possible file name is used.
  //
  while(1)  {
    //    We used to use the time to generate this.
    //    Now, we use some crypto to avoid bug #47027
    //RNG_GenerateGlobalRandomBytes((void *)&this_call, sizeof(this_call));
	  char* output=(char *)&this_call;
    size_t len = sizeof(this_call);
	  size_t i;
	  srand((unsigned int) PR_IntervalToMilliseconds(PR_IntervalNow()));
	  for (i=0;i<len; i++) {
		  int t = rand();
		  *output = (char) (t % 256);
		  output++;
	  }

    //  Convert the time into a 7 character string.
    //  Strip only the signifigant 5 bits.
    //  We'll want to reverse the string to make it look coherent
    //    in a directory of file names.
    //
    for(i_letter = 0; i_letter < i_timechars; i_letter++) {
      ca_time[i_letter] = (char)((this_call >> (i_letter * 5)) & 0x1F);
      
      //  Convert any numbers to their equivalent ascii code
      //
      if(ca_time[i_letter] <= 9)  {
        ca_time[i_letter] += '0';
      }
      //  Convert the character to it's equivalent ascii code
      //
      else  {
        ca_time[i_letter] += 'A' - 10;
      }
    }
    
    //  End the created time string.
    //
    ca_time[i_letter] = '\0';
    
    //  Reverse the time string.
    //
// XXX fix this
#ifdef XP_PC
    _strrev(ca_time);
#endif   
    //  Create the fully qualified path and file name.
    //
    sprintf(file_buf, "%s\\%s%s%s", directory, prefix, ca_time, ext);

    //  Determine if the file exists, and mark that we've tried yet
    //    another file name (mark to be used later).
    //  
	  //  Use the system call instead of XP_Stat since we already
	  //  know the name and we don't want recursion
	  //
// XXX fix this
#ifdef XP_PC
	  struct _stat statinfo;
    int status  = _stat(file_buf, &statinfo);
#else
    int status = 1;
#endif
    i_numtries++;
    
    //  If it does not exists, we are successful, return the name.
    //
    if(status == -1)  {
      //      TRACE("Temp file name is %s\n", file_buf);
      return(file_buf);
    }
    
    //  If there is no room for additional characters in the time,
    //    we'll have to return NULL here, or we go infinite.
    //    This is a one case scenario where the requested prefix is
    //    actually 8 letters long.
    //  Infinite loops could occur with a 7, 6, 5, etc character prefixes
    //    if available files are all eaten up (rare to impossible), in
    //    which case, we should check at some arbitrary frequency of
    //    tries before we give up instead of attempting to Vulcanize
    //    this code.  Live long and prosper.
    //
    if(i_timechars == 0)  {
      break;
    }
    else if(i_numtries == 0x00FF) {
      break;
    }
  }
  
  //  Requested name is thought to be impossible to generate.
  //
  //TRACE("No more temp file names....\n");
  return(NULL);
  
}


// static helper functions for nsIFormControls

PRBool
nsFormFrame::GetDisabled(nsIFrame* aChildFrame, nsIContent* aContent) 
{
  PRBool result = PR_FALSE;

  nsIContent* content = aContent;
  if (nsnull == content) {
    aChildFrame->GetContent(&content);
  }
  if (nsnull != content) {
    nsIHTMLContent* htmlContent = nsnull;
    content->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if (nsnull != htmlContent) {
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_HAS_VALUE == htmlContent->GetHTMLAttribute(nsHTMLAtoms::disabled, value)) {
        result = PR_TRUE;
      }
      NS_RELEASE(htmlContent);
    }
    if (nsnull == aContent) {
      NS_RELEASE(content);
    }
  }
  return result;
}

PRBool
nsFormFrame::GetReadonly(nsIFrame* aChildFrame, nsIContent* aContent) 
{
  PRBool result = PR_FALSE;

  nsIContent* content = aContent;
  if (nsnull == content) {
    aChildFrame->GetContent(&content);
  }
  if (nsnull != content) {
    nsIHTMLContent* htmlContent = nsnull;
    content->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if (nsnull != htmlContent) {
      nsHTMLValue value;
      if (NS_CONTENT_ATTR_HAS_VALUE == htmlContent->GetHTMLAttribute(nsHTMLAtoms::readonly, value)) {
        result = PR_TRUE;
      }
      NS_RELEASE(htmlContent);
    }
    if (nsnull == aContent) {
      NS_RELEASE(content);
    }
  }
  return result;
}

nsresult
nsFormFrame::GetName(nsIFrame* aChildFrame, nsString& aName, nsIContent* aContent) 
{
  nsresult result = NS_FORM_NOTOK;

  nsIContent* content = aContent;
  if (nsnull == content) {
    aChildFrame->GetContent(&content);
  }
  if (nsnull != content) {
    nsIHTMLContent* htmlContent = nsnull;
    result = content->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if (NS_SUCCEEDED(result) && (nsnull != htmlContent)) {
      nsHTMLValue value;
      result = htmlContent->GetHTMLAttribute(nsHTMLAtoms::name, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(aName);
        }
      }
      NS_RELEASE(htmlContent);
    }
    if (nsnull == aContent) {
      NS_RELEASE(content);
    }
  }
  return result;
}

nsresult
nsFormFrame::GetValue(nsIFrame* aChildFrame, nsString& aValue, nsIContent* aContent) 
{
  nsresult result = NS_FORM_NOTOK;

  nsIContent* content = aContent;
  if (nsnull == content) {
    aChildFrame->GetContent(&content);
  }
  if (nsnull != content) {
    nsIHTMLContent* htmlContent = nsnull;
    result = content->QueryInterface(kIHTMLContentIID, (void**)&htmlContent);
    if (NS_SUCCEEDED(result) && (nsnull != htmlContent)) {
      nsHTMLValue value;
      result = htmlContent->GetHTMLAttribute(nsHTMLAtoms::value, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(aValue);
        }
      }
      NS_RELEASE(htmlContent);
    }
    if (nsnull == aContent) {
      NS_RELEASE(content);
    }
  }
  return result;
}

void
nsFormFrame::StyleChangeReflow(nsIPresContext* aPresContext,
                               nsIFrame* aFrame)
{
  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
    
  nsIReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        nsIReflowCommand::StyleChanged);
  if (NS_SUCCEEDED(rv)) {
    shell->AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
}

