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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Adrian Havill <havill@redhat.com>
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



#define NS_IMPL_IDS
#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#undef NS_IMPL_IDS 

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsID.h"


#include "nsFormFrame.h"
#include "nsIFormControlFrame.h"
#include "nsFormControlFrame.h"
#include "nsFileControlFrame.h"
#include "nsRadioControlGroup.h"

#include "nsIForm.h"
#include "nsIFormControl.h"
#include "nsIAtom.h"
#include "nsHTMLIIDs.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsHTMLAtoms.h"
#include "nsCRT.h"
#include "nsIURL.h"
#include "nsIHTMLDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPrintContext.h"

#include "nsIFormProcessor.h"

#include "nsIIOService.h"
#include "nsIPref.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIDocument.h"
#include "nsILinkHandler.h"
#include "nsGfxRadioControlFrame.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsDOMError.h"
#include "nsHTMLParts.h"
#include "nsIReflowCommand.h"

#include "nsIUnicodeEncoder.h"

// FormSubmit observer notification
#include "nsIFormSubmitObserver.h"
#include "nsIObserverService.h"
#include "nsIServiceManager.h"

// Get base target for submission
#include "nsIHTMLContent.h"
#include "nsIDOMHTMLInputElement.h"

#include "xp_file.h"
#include "prio.h"
#include "prmem.h"
#include "prenv.h"
#include "prlong.h"

// Rewrite of Multipart form posting
#include "nsDirectoryServiceDefs.h"
#include "nsIFileSpec.h"
#include "nsFileSpec.h"
#include "nsIProtocolHandler.h"
#include "nsIServiceManager.h"
#include "nsEscape.h"
#include "nsLinebreakConverter.h"
#include "nsCExternalHandlerService.h"
#include "nsIMIMEService.h"

#include "nsILocalFile.h"		// Using nsILocalFile to get file size

// Security
#include "nsIScriptSecurityManager.h"

#include "nsIDOMWindowInternal.h"

static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);

//----------------------------------------------------------------------

static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID);
//ahmed 15-1
#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
static NS_DEFINE_CID(kUBidiUtilCID, NS_UNICHARBIDIUTIL_CID);
#endif
//end

// Marks if the first form is submitted or not. Once we submit the first
// form, this will become PR_TRUE
PRBool nsFormFrame::gFirstFormSubmitted = PR_FALSE;
PRBool nsFormFrame::gInitPasswordManager = PR_FALSE;

NS_IMETHODIMP
nsFormFrame::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(NS_GET_IID(nsIFormManager))) {
    *aInstancePtr = (void*)(nsIFormManager*)this;
    return NS_OK;
  }
  return nsBlockFrame::QueryInterface(aIID, aInstancePtr);
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
  : nsBlockFrame()
{
}

nsFormFrame::~nsFormFrame()
{
  RemoveRadioGroups();
  PRInt32 numControls = mFormControls.Count();
  PRInt32 i;
  // Traverse list from end to 0 -> void array remove method does less work
  for (i = (numControls-1); i>=0; i--) {
    nsIFormControlFrame* fcFrame = (nsIFormControlFrame*) mFormControls.ElementAt(i);
    fcFrame->SetFormFrame(nsnull);
    mFormControls.RemoveElement(fcFrame);
  }
}

NS_IMETHODIMP
nsFormFrame::GetAction(nsString* aAction)
{
  nsresult result = NS_OK;
  if (mContent) {
    nsIDOMHTMLFormElement* form = nsnull;
    result = mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLFormElement), (void**)&form);
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
    result = mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLFormElement), (void**)&form);
    if ((NS_OK == result) && form) {
      form->GetTarget(*aTarget);
      if ((*aTarget).Length() == 0) {
        nsIHTMLContent* content = nsnull;
	result = form->QueryInterface(kIHTMLContentIID, (void**)&content);
	if ((NS_OK == result) && content) {
	  content->GetBaseTarget(*aTarget);
	  NS_RELEASE(content);
	}
      }
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
nsFormFrame::OnReset(nsIPresContext* aPresContext)
{
  PRInt32 numControls = mFormControls.Count();
  for (int i = 0; i < numControls; i++) {
    nsIFormControlFrame* fcFrame = (nsIFormControlFrame*) mFormControls.ElementAt(i);
    fcFrame->Reset(aPresContext);
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

void nsFormFrame::AddFormControlFrame(nsIPresContext* aPresContext, nsIFrame& aFrame)
{
  // Make sure we have a form control
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aFrame.QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
  if ((NS_OK != result) || (nsnull == fcFrame)) {
    return;
  }

  // Get this control's form frame and add this control to it
  nsCOMPtr<nsIContent> iContent;
  result = aFrame.GetContent(getter_AddRefs(iContent));
  if (NS_SUCCEEDED(result) && iContent) {
    nsCOMPtr<nsIFormControl> formControl;
    result = iContent->QueryInterface(NS_GET_IID(nsIFormControl), getter_AddRefs(formControl));
    if (NS_SUCCEEDED(result) && formControl) {
      nsCOMPtr<nsIDOMHTMLFormElement> formElem;
      result = formControl->GetForm(getter_AddRefs(formElem));
      if (NS_SUCCEEDED(result) && formElem) {
        nsCOMPtr<nsIPresShell> presShell;
        result = aPresContext->GetShell(getter_AddRefs(presShell));
        if (NS_SUCCEEDED(result) && presShell) {
          nsIContent* formContent;
          result = formElem->QueryInterface(NS_GET_IID(nsIContent), (void**)&formContent);
          if (NS_SUCCEEDED(result) && formContent) {
            nsFormFrame* formFrame = nsnull;
            result = presShell->GetPrimaryFrameFor(formContent, (nsIFrame**)&formFrame);
            if (NS_SUCCEEDED(result) && formFrame) {
              fcFrame->SetFormFrame(formFrame);
              formFrame->AddFormControlFrame(aPresContext, *fcFrame);
            }
            NS_RELEASE(formContent);
          }
        }
      }
    }
  }
}

void nsFormFrame::RemoveFormControlFrame(nsIFormControlFrame& aFrame)
{
  // Remove form control from array
  mFormControls.RemoveElement(&aFrame);
}

NS_IMETHODIMP
nsFormFrame::RemoveFrame(nsIPresContext* aPresContext,
                         nsIPresShell&   aPresShell,
                         nsIAtom*        aListName,
                         nsIFrame*       aOldFrame)
{
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aOldFrame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
  if ((NS_OK == result) || (nsnull != fcFrame)) {
    PRInt32 type;
    fcFrame->GetType(&type);
    if (NS_FORM_INPUT_RADIO == type) {
      nsRadioControlGroup * group;
      nsAutoString name;
      nsresult rv = GetRadioInfo(fcFrame, name, group);
      if (NS_SUCCEEDED(rv) && nsnull != group) {
        DoDefaultSelection(aPresContext, group, (nsGfxRadioControlFrame*)fcFrame);
      }
    }
  }

  return nsBlockFrame::RemoveFrame(aPresContext, aPresShell, aListName, aOldFrame);
}

nsresult nsFormFrame::GetRadioInfo(nsIFormControlFrame* aFrame,
                                   nsString& aName,
                                   nsRadioControlGroup *& aGroup)
{
  aGroup = nsnull;
  aName.SetLength(0);
  aFrame->GetName(&aName);
  PRBool hasName = aName.Length() > 0;

  // radio group processing
  if (hasName) { 
    int numGroups = mRadioGroups.Count();
    nsRadioControlGroup* group;
    for (int j = 0; j < numGroups; j++) {
      group = (nsRadioControlGroup *) mRadioGroups.ElementAt(j);
      nsString groupName;
      group->GetName(groupName);
      if (groupName.Equals(aName)) {
        aGroup = group;
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}

void nsFormFrame::DoDefaultSelection(nsIPresContext*          aPresContext, 
                                     nsRadioControlGroup *    aGroup,
                                     nsGfxRadioControlFrame * aRadioToIgnore)
{
#if 0
  // We do not want to do the default selection when printing
  // The code below ends up calling into the content, which then sets 
  // the state on the "primary" frame the frame being displayed
  // this causes it to be "reset" to the default selection, 
  // then when we go to get the current state of the UI it has been reset back
  // and then the printed state has the wrong value also
  nsCOMPtr<nsIPrintContext> printContext = do_QueryInterface(aPresContext);
  if (printContext) {
    return;
  }
  // If in standard mode, then a radio group MUST default 
  // to the first item in the group (it must be selected)
  nsCompatibility mode;
  nsFormControlHelper::GetFormCompatibilityMode(aPresContext, mode);
  if (eCompatibility_Standard == mode) {
    // first find out if any have a default selection
    PRInt32 i;
    PRInt32 numItems = aGroup->GetNumRadios();
    PRBool oneIsDefSelected = PR_FALSE;
    for (i=0;i<numItems;i++) {
      nsGfxRadioControlFrame * radioBtn = (nsGfxRadioControlFrame*) aGroup->GetRadioAt(i);
      nsCOMPtr<nsIContent> content;
      radioBtn->GetContent(getter_AddRefs(content));
      if (content) {
        nsCOMPtr<nsIDOMHTMLInputElement> input(do_QueryInterface(content));
        if (input) {
          PRBool initiallyChecked = PR_FALSE;
          if (radioBtn->IsRestored()) {
            initiallyChecked = radioBtn->GetRestoredChecked();
          } else {
            initiallyChecked = radioBtn->GetDefaultChecked();
          }

          PRBool currentValue = radioBtn->GetChecked();
          if (currentValue != initiallyChecked ) {
            input->SetChecked(initiallyChecked);
            //radioBtn->SetChecked(aPresContext, initSelected, PR_FALSE);
          }
          if (initiallyChecked) {
            oneIsDefSelected = PR_TRUE;
          }
        }
      }
    }

    // if there isn't a default selcted radio then 
    // select the firdst one in the group.
    // if aRadioToIgnore is not null then it is being deleted
    // so don't select that item, select the next one if there is one.
    if (!oneIsDefSelected && numItems > 0) {
      nsGfxRadioControlFrame * radioBtn = (nsGfxRadioControlFrame*) aGroup->GetRadioAt(0);
      if (aRadioToIgnore != nsnull && aRadioToIgnore == radioBtn) {
        if (numItems == 1) {
          return;
        }
        radioBtn = (nsGfxRadioControlFrame*) aGroup->GetRadioAt(1);
      }
      nsCOMPtr<nsIContent> content;
      radioBtn->GetContent(getter_AddRefs(content));
      if (content) {
        nsCOMPtr<nsIDOMHTMLInputElement> input(do_QueryInterface(content));
        if (input) {
          input->SetChecked(PR_TRUE);
        }
      }
    }
  }
#endif
}


void nsFormFrame::AddFormControlFrame(nsIPresContext* aPresContext, nsIFormControlFrame& aFrame)
{
  PRInt32 type;
  aFrame.GetType(&type);
  if (!gInitPasswordManager && type == NS_FORM_INPUT_PASSWORD) {
    // Initialize the password manager category
    gInitPasswordManager = PR_TRUE;
    NS_CreateServicesFromCategory(NS_PASSWORDMANAGER_CATEGORY,
                                  NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(void*,this)),
                                  NS_ConvertASCIItoUCS2(NS_PASSWORDMANAGER_CATEGORY).get());
  }

  // Add this control to the list
  // Sort by content ID - this assures we submit in document order (bug 18728)
  PRInt32 i = mFormControls.Count();

  nsCOMPtr<nsIContent> newContent;
  nsIFrame* newFrame = nsnull;
  nsresult rv = aFrame.QueryInterface(NS_GET_IID(nsIFrame), (void **)&newFrame);
  if (NS_SUCCEEDED(rv) && newFrame) {
    rv = newFrame->GetContent(getter_AddRefs(newContent));
    if (NS_SUCCEEDED(rv) && newContent) {
      PRUint32 newID;
      newContent->GetContentID(&newID);
      for (; i>0; i--) {
        nsIFormControlFrame* thisControl = (nsIFormControlFrame*) mFormControls.ElementAt(i-1);
        if (thisControl) {
          nsCOMPtr<nsIContent> thisContent;
          nsIFrame* thisFrame = nsnull;
          rv = thisControl->QueryInterface(NS_GET_IID(nsIFrame), (void **)&thisFrame);
          if (NS_SUCCEEDED(rv) && thisFrame) {
            rv = thisFrame->GetContent(getter_AddRefs(thisContent));
            if (NS_SUCCEEDED(rv) && thisContent) {
              PRUint32 thisID;
              thisContent->GetContentID(&thisID);
              if (newID > thisID)
                break;
            }
          }
        }
      }
    }
  }
  mFormControls.InsertElementAt(&aFrame, i);

  // determine which radio buttons belong to which radio groups, unnamed radio buttons
  // don't go into any group since they can't be submitted.

  if (NS_FORM_INPUT_RADIO == type) { 
    nsGfxRadioControlFrame* radioFrame = (nsGfxRadioControlFrame*)&aFrame;
    // gets the name of the radio group and the group
    nsRadioControlGroup * group;
    nsAutoString name;
    rv = GetRadioInfo(&aFrame, name, group);
    if (NS_SUCCEEDED(rv) && nsnull != group) {
      group->AddRadio(radioFrame);
    } else {
      group = new nsRadioControlGroup(name);
      mRadioGroups.AppendElement(group);
      group->AddRadio(radioFrame);
    }
    // allow only one checked radio button
    PRBool initiallyChecked = PR_FALSE;
    if (radioFrame->IsRestored()) {
      initiallyChecked = radioFrame->GetRestoredChecked();
    } else {
      initiallyChecked = radioFrame->GetDefaultChecked();
    }
    if (initiallyChecked) {
	    if (nsnull == group->GetCheckedRadio()) {
	      group->SetCheckedRadio(radioFrame);
	    } else {
	      radioFrame->SetChecked(aPresContext, PR_FALSE, PR_FALSE);
	    }
    }
    DoDefaultSelection(aPresContext, group);
  }
}

void nsFormFrame::RemoveRadioControlFrame(nsIFormControlFrame * aFrame)
{
  PRInt32 type;
  aFrame->GetType(&type);

  // radio group processing
  if (NS_FORM_INPUT_RADIO == type) { 
    nsGfxRadioControlFrame* radioFrame = (nsGfxRadioControlFrame*)aFrame;
    // gets the name of the radio group and the group
    nsRadioControlGroup * group;
    nsAutoString name;
    nsresult rv = GetRadioInfo(aFrame, name, group);
    if (NS_SUCCEEDED(rv) && nsnull != group) {
      group->RemoveRadio(radioFrame);
    }
  }
}

  
//--------------------------------------------------------
// Return the content of the currently selected item in
// the radio group of the incoming radiobutton.
//--------------------------------------------------------
nsresult
nsFormFrame::GetRadioGroupSelectedContent(nsGfxRadioControlFrame* aControl,
                                          nsIContent **           aRadiobtn)
{
  NS_ASSERTION(aControl, "nsGfxRadioControlFrame can't be null");

  // first get correct interface
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aControl->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
  if (NS_SUCCEEDED(result)) {
    // get the form frame for the radio btn
    nsFormFrame * formFrame = ((nsFormControlFrame *)aControl)->GetFormFrame();
    if (formFrame != nsnull) {
      // now get the radio group by name
      nsAutoString groupName;
      nsRadioControlGroup * group = nsnull;
      result = formFrame->GetRadioInfo(fcFrame, groupName, group);
      if (NS_SUCCEEDED(result) && nsnull != group) {
        // get the currently checked radio button
        nsGfxRadioControlFrame* currentCheckBtn = group->GetCheckedRadio();
        if (currentCheckBtn != nsnull) {
          currentCheckBtn->GetContent(aRadiobtn);
        }
      }
    }
  }
  return NS_OK;
}

//--------------------------------------------------------
// returns NS_ERROR_FAILURE if the radiobtn doesn't have a group
// returns NS_OK is it did have a radio group 
//--------------------------------------------------------
nsresult
nsFormFrame::OnRadioChecked(nsIPresContext*         aPresContext, 
                            nsGfxRadioControlFrame& aControl, 
                            PRBool                  aNewCheckedVal)
{
  // first get correct interface
  nsIFormControlFrame* fcFrame = nsnull;
  nsresult result = aControl.QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
  if (NS_SUCCEEDED(result)) {
    // now get the radio group by name
    nsAutoString groupName;
    nsRadioControlGroup * group = nsnull;
    result = GetRadioInfo(fcFrame, groupName, group);
    if (NS_SUCCEEDED(result) && nsnull != group) {
      // get the currently checked radio button
      nsGfxRadioControlFrame* currentCheckBtn = group->GetCheckedRadio();
      // is the new checked btn different than the current button?
      if (&aControl != currentCheckBtn) {
        // if the new button is being set to false
        // then don't do anything
        if (aNewCheckedVal) {
          // the current btn could be null
          if (currentCheckBtn != nsnull) {
            currentCheckBtn->SetChecked(aPresContext, !aNewCheckedVal, PR_FALSE);
          }
          aControl.SetChecked(aPresContext, aNewCheckedVal, PR_FALSE);
          group->SetCheckedRadio(&aControl);
        }
      } else {
        // here we are setting the same radio button 
        // as the one that is currently checked 
        //
        if (currentCheckBtn != nsnull) {
          currentCheckBtn->SetChecked(aPresContext, aNewCheckedVal, PR_FALSE);
          // So if we are setting the current btn to be 0 or off 
          // then we must set a default selction
          if (!aNewCheckedVal) {
            DoDefaultSelection(aPresContext, group, currentCheckBtn);
          }
        }
      }
    }
  }
  return NS_OK;
}


nsresult
NS_NewFormFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFormFrame* it = new (aPresShell) nsFormFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  it->SetFlags(aFlags);
  *aNewFrame = it;
  return NS_OK;
}

#if 0

PRIVATE
void DebugPrint(char* aLabel, nsString aString)
{
  char* out = ToNewCString(aString);
  printf("\n %s=%s\n", aLabel, out);
  delete [] out;
}

#endif

// Give the form process observer a chance to modify the value passed for form submission
nsresult
nsFormFrame::ProcessValue(nsIFormProcessor& aFormProcessor, nsIFormControlFrame* aFrameControl, const nsString& aName, nsString& aValue)
{
  nsresult res = NS_OK;

  nsIFrame *frame = nsnull;
  res = aFrameControl->QueryInterface(NS_GET_IID(nsIFrame), (void **)&frame);
  if (NS_SUCCEEDED(res) && (frame)) {
    nsCOMPtr<nsIContent> content;
    nsresult rv = frame->GetContent(getter_AddRefs(content));
    if (NS_SUCCEEDED(rv) && content) {
      nsCOMPtr<nsIDOMHTMLElement> formElement;
      res = content->QueryInterface(NS_GET_IID(nsIDOMHTMLElement), getter_AddRefs(formElement));
      if (NS_SUCCEEDED(res) && formElement) {
	 	    res = aFormProcessor.ProcessValue(formElement, aName, aValue);
		    NS_ASSERTION(NS_SUCCEEDED(res), "unable Notify form process observer"); 
      }
    }
  }

  return res;
}


// submission

NS_IMETHODIMP
nsFormFrame::OnSubmit(nsIPresContext* aPresContext, nsIFrame* aFrame)
{
  if (!mContent) {
    return NS_FORM_NOTOK;
  }

//ahmed
#ifdef IBMBIDI
  PRUint32 bidiOptions;
  aPresContext->GetBidi(&bidiOptions);
  mCtrlsModAtSubmit = GET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions);
  mTextDir          = GET_BIDI_OPTION_DIRECTION(bidiOptions);
#endif
//ahmed end
   // Get a service to process the value part of the form data
   // If one doesn't exist, that fine. It's not required.
  nsresult result = NS_OK;
  nsCOMPtr<nsIFormProcessor> formProcessor = 
           do_GetService(kFormProcessorCID, &result);

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
    aFrame->QueryInterface(NS_GET_IID(nsIFormControlFrame), (void**)&fcFrame);
  }

  nsCOMPtr<nsIFile> multipartDataFile;
  if (isURLEncoded) {
    result = ProcessAsURLEncoded(formProcessor, isPost, data, fcFrame);
  }
  else {
    result = ProcessAsMultipart(formProcessor, getter_AddRefs(multipartDataFile), fcFrame);
  }

  // Don't bother submitting form if we failed to generate a valid submission
  if (NS_FAILED(result))
    return result;

  // make the url string
  nsCOMPtr<nsILinkHandler> handler;
  if (NS_OK == aPresContext->GetLinkHandler(getter_AddRefs(handler))) {
    nsAutoString href;
    GetAction(&href);

    // Get the document.
    // We'll need it now to form the URL we're submitting to.
    // We'll also need it later to get the DOM window when notifying form submit observers (bug 33203)
    nsCOMPtr<nsIDocument> document;
    mContent->GetDocument(*getter_AddRefs(document));
    if (!document) return NS_OK; // No doc means don't submit, see Bug 28988

    // Resolve url to an absolute url
    nsCOMPtr<nsIURI> docURL;
    document->GetBaseURL(*getter_AddRefs(docURL));
    NS_ASSERTION(docURL, "No Base URL found in Form Submit!\n");
    if (!docURL) return NS_OK; // No base URL -> exit early, see Bug 30721

      // If an action is not specified and we are inside 
      // a HTML document then reload the URL. This makes us
      // compatible with 4.x browsers.
      // If we are in some other type of document such as XML or
      // XUL, do nothing. This prevents undesirable reloading of
      // a document inside XUL.

    if (href.IsEmpty()) {
      nsCOMPtr<nsIHTMLDocument> htmlDoc;
      if (PR_FALSE == NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIHTMLDocument),
                                             getter_AddRefs(htmlDoc)))) {   
        // Must be a XML, XUL or other non-HTML document type
        // so do nothing.
        return NS_OK;
      } 

      // Necko's MakeAbsoluteURI doesn't reuse the baseURL's rel path if it is
      // passed a zero length rel path.
      nsXPIDLCString relPath;
      docURL->GetSpec(getter_Copies(relPath));
      NS_ASSERTION(relPath, "Rel path couldn't be formed in form submit!\n");
      if (relPath) {
        href.AppendWithConversion(relPath);

      } else {
        result = NS_ERROR_OUT_OF_MEMORY;
      }
    } else {
      // Get security manager, check to see if access to action URI is allowed.
      nsCOMPtr<nsIScriptSecurityManager> securityManager = 
               do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &result);
      nsCOMPtr<nsIURI> actionURL;
      if (NS_FAILED(result)) return result;

      result = NS_NewURI(getter_AddRefs(actionURL), href, docURL);
      if (NS_SUCCEEDED(result)) {
        result = securityManager->CheckLoadURI(docURL, actionURL, nsIScriptSecurityManager::STANDARD);
        if (NS_FAILED(result)) return result;
      }

      nsXPIDLCString scheme;
      PRBool isMailto = PR_FALSE;
      if (actionURL && NS_FAILED(result = actionURL->SchemeIs("mailto",
                      &isMailto)))
        return result;
      if (isMailto) {
        PRBool enabled;
        if (NS_FAILED(result = securityManager->IsCapabilityEnabled("UniversalSendMail", &enabled)))
        {
          return result;
        }
        if (!enabled) {
          // Form submit to a mailto: URI requires the UniversalSendMail privilege
          return NS_ERROR_DOM_SECURITY_ERR;
        }
      }
    }

    nsAutoString target;
    GetTarget(&target);

    // Add the URI encoded form values to the URI
    // Get the scheme of the URI.
    nsCOMPtr<nsIURI> actionURL;
    nsXPIDLCString scheme;
    if (NS_SUCCEEDED(result = NS_NewURI(getter_AddRefs(actionURL), href, docURL))) {
      result = actionURL->GetScheme(getter_Copies(scheme));
    }
    nsAutoString theScheme; theScheme.AssignWithConversion( NS_STATIC_CAST(const char*, scheme) );
    // Append the URI encoded variable/value pairs for GET's
    if (!isPost) {
      if (!theScheme.EqualsIgnoreCase("javascript")) { // Not for JS URIs, see bug 26917

        // Bug 42616: Trim off named anchor and save it to add later
        PRInt32 namedAnchorPos = href.FindChar('#', PR_FALSE, 0);
        nsAutoString namedAnchor;
        if (kNotFound != namedAnchorPos) {
          href.Right(namedAnchor, (href.Length() - namedAnchorPos));
          href.Truncate(namedAnchorPos);
        }

        // Chop off old query string (bug 25330, 57333)
        // Only do this for GET not POST (bug 41585)
        PRInt32 queryStart = href.FindChar('?');
        if (kNotFound != queryStart) {
          href.Truncate(queryStart);
        }

        href.Append(PRUnichar('?'));        
        href.Append(data);

        // Bug 42616: Add named anchor to end after query string
        if (namedAnchor.Length()) {
          href.Append(namedAnchor);
        }
      }
    }

    nsAutoString absURLSpec;
    result = NS_MakeAbsoluteURI(absURLSpec, href, docURL);
    if (NS_FAILED(result)) return result;

    // If this is the first form, bring alive the first form submit
    // category observers
    if (!gFirstFormSubmitted) {
      gFirstFormSubmitted = PR_TRUE;
      NS_CreateServicesFromCategory(NS_FIRST_FORMSUBMIT_CATEGORY,
                                    NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(void*,this)),
                                    NS_ConvertASCIItoUCS2(NS_FIRST_FORMSUBMIT_CATEGORY).get());
    }

    // Notify observers that the form is being submitted.
    result = NS_OK;
    nsCOMPtr<nsIObserverService> service = 
             do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &result);
    if (NS_FAILED(result)) return result;

    nsString  theTopic; theTopic.AssignWithConversion(NS_FORMSUBMIT_SUBJECT);
    nsCOMPtr<nsIEnumerator> theEnum;
    result = service->EnumerateObserverList(theTopic.get(), getter_AddRefs(theEnum));
    if (NS_SUCCEEDED(result) && theEnum){
      nsCOMPtr<nsISupports> inst;
      PRBool cancelSubmit = PR_FALSE;

      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      document->GetScriptGlobalObject(getter_AddRefs(globalObject));  
      nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(globalObject);

      for (theEnum->First(); theEnum->IsDone() != NS_OK; theEnum->Next()) {
        nsresult gotObserver = NS_OK;
        gotObserver = theEnum->CurrentItem(getter_AddRefs(inst));
        if (NS_SUCCEEDED(gotObserver) && inst) {
          nsCOMPtr<nsIFormSubmitObserver> formSubmitObserver = do_QueryInterface(inst, &result);
          if (NS_SUCCEEDED(result) && formSubmitObserver) {
            nsresult notifyStatus = formSubmitObserver->Notify(mContent, window, actionURL, &cancelSubmit);
            if (NS_FAILED(notifyStatus)) { // assert/warn if we get here?
              return notifyStatus;
            }
          }
        }
        if (cancelSubmit) {
          return NS_OK;
        }
      }
    }

    // Now pass on absolute url to the click handler
    nsCOMPtr<nsIInputStream> postDataStream;
    if (isPost) {
      if (isURLEncoded) {
        nsCAutoString postBuffer;
        postBuffer.AssignWithConversion(data);
        NS_NewPostDataStream(getter_AddRefs(postDataStream), !isURLEncoded,
                             postBuffer.get(), 0);
      } else {
        // Cut-and-paste of NS_NewPostDataStream
        nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID));
        if (serv && multipartDataFile) {
          nsCOMPtr<nsIInputStream> rawStream;
          NS_NewLocalFileInputStream(getter_AddRefs(rawStream),
                                     multipartDataFile);
          if (rawStream) {
              NS_NewBufferedInputStream(getter_AddRefs(postDataStream),
                                        rawStream, 8192);
          }
        }
      }
    }    
    if (handler) {
#if defined(DEBUG_rods) || defined(DEBUG_pollmann)
      {
        printf("******\n");
        char * str = ToNewCString(data);
        printf("postBuffer[%s]\n", str);
        Recycle(str);

        str = ToNewCString(absURLSpec);
        printf("absURLSpec[%s]\n", str);
        Recycle(str);

        str = ToNewCString(target);
        printf("target    [%s]\n", str);
        Recycle(str);
        printf("******\n");
      }
#endif
      handler->OnLinkClick(mContent, eLinkVerb_Replace,
                           absURLSpec.get(),
                           target.get(), postDataStream);
    }
// We need to delete the data file somewhere...
//    if (!isURLEncoded) {
//      nsFileSpec mdf = nsnull;
//      result = multipartDataFile->GetFileSpec(&mdf);
//      if (NS_SUCCEEDED(result) && mdf) {
//        mdf.Delete(PR_FALSE);
//      }
//    }
  }
  return result;
}

// XXX i18n helper routines
char*
nsFormFrame::UnicodeToNewBytes(const PRUnichar* aSrc, PRUint32 aLen, nsIUnicodeEncoder* encoder)
{
#ifdef IBMBIDI
  //ahmed 15-1
  nsAutoString temp;
  nsresult rv = NS_OK;
  nsCOMPtr<nsIUBidiUtils> bidiUtils = do_GetService("@mozilla.org/intl/unicharbidiutil;1");
  nsAutoString newBuffer;
  //This condition handle the RTL,LTR for a logical file
  if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL )&&( mCharset.EqualsIgnoreCase("windows-1256") ) ){
    bidiUtils->Conv_06_FE_WithReverse(nsString(aSrc), newBuffer,mTextDir);
    aSrc = (PRUnichar *)newBuffer.get();
    aLen=newBuffer.Length();
  }
  else if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_LOGICAL )&&( mCharset.EqualsIgnoreCase("IBM864") ) ){
    //For 864 file, When it is logical, if LTR then only convert
    //If RTL will mak a reverse for the buffer
    bidiUtils->Conv_FE_06(nsString(aSrc), newBuffer);
    aSrc = (PRUnichar *)newBuffer.get();
    temp = newBuffer;
    aLen=newBuffer.Length();
    if (mTextDir == 2) { //RTL
    //Now we need to reverse the Buffer, it is by searshing the buffer
      PRUint32 loop = aLen;
      unsigned int z;
      for (z=0; z<=aLen; z++){
        temp.SetCharAt((PRUnichar)aSrc[loop], z);
        loop--;
      }
    }
    aSrc = (PRUnichar *)temp.get();
  }
  else if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL )&&( mCharset.EqualsIgnoreCase("IBM864"))&& (mTextDir == IBMBIDI_TEXTDIRECTION_RTL) ){

    bidiUtils->Conv_FE_06(nsString(aSrc), newBuffer);
    aSrc = (PRUnichar *)newBuffer.get();
    temp = newBuffer;
    aLen=newBuffer.Length();
    //Now we need to reverse the Buffer, it is by searshing the buffer
    PRUint32 loop = aLen;
    unsigned int z;
    for (z=0; z<=aLen; z++){
      temp.SetCharAt((PRUnichar)aSrc[loop], z);
      loop--;
    }
    aSrc = (PRUnichar *)temp.get();
  }
#endif
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
nsString*
nsFormFrame::URLEncode(const nsString& aString, nsIUnicodeEncoder* encoder) 
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
  nsString* result = new nsString;
  result->AssignWithConversion(outBuf);
  nsCRT::free(outBuf);
  nsMemory::Free(convertedBuf);
  return result;
}

void nsFormFrame::GetSubmitCharset(nsString& oCharset)
{
  oCharset.AssignWithConversion("UTF-8"); // default to utf-8
  nsresult rv;
  // XXX
  // We may want to get it from the HTML 4 Accept-Charset attribute first
  // see 17.3 The FORM element in HTML 4 for details
  nsresult result = NS_OK;
  nsAutoString acceptCharsetValue;
  if (mContent) {
    nsIHTMLContent* form = nsnull;
    result = mContent->QueryInterface(kIHTMLContentIID, (void**)&form);
    if (NS_SUCCEEDED(result) && (nsnull != form)) {
      nsHTMLValue value;
      result = form->GetHTMLAttribute(nsHTMLAtoms::acceptcharset, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        if (eHTMLUnit_String == value.GetUnit()) {
          value.GetStringValue(acceptCharsetValue);
        }
      }
      NS_RELEASE(form);
    }
  }
#ifdef DEBUG_ftang
  printf("accept-charset = %s\n", NS_LossyConvertUCS2toASCII(acceptCharsetValue).get());
#endif
  PRInt32 l = acceptCharsetValue.Length();
  if(l > 0 ) {
    PRInt32 offset=0;
    PRInt32 spPos=0;
    // get charset from charsets one by one
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &rv));
    if(NS_SUCCEEDED(rv) && (nsnull != calias)) {
      do {
        spPos = acceptCharsetValue.FindChar(PRUnichar(' '),PR_TRUE, offset);
        PRInt32 cnt = ((-1==spPos)?(l-offset):(spPos-offset));
        if(cnt > 0) {
          nsAutoString charset;
          acceptCharsetValue.Mid(charset, offset, cnt);
#ifdef DEBUG_ftang
          printf("charset[i] = %s\n", NS_LossyConvertUCS2toASCII(charset).get());
#endif
          if(NS_SUCCEEDED(calias->GetPreferred(charset,oCharset)))
            return;
        }
        offset = spPos + 1;
      } while(spPos != -1);
    }
  }
  // if there are no accept-charset or all the charset are not supported
  // Get the charset from document
  nsIDocument* doc = nsnull;
  mContent->GetDocument(doc);
  if( nsnull != doc ) {
    rv = doc->GetDocumentCharacterSet(oCharset);
    NS_RELEASE(doc);
  }

#ifdef IBMBIDI
//ahmed 15-1   why it is here, I think you have to put it after the next conditions
  mCharset=oCharset;
//ahmed 
  if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL )&&( oCharset.EqualsIgnoreCase("windows-1256") ) ) {
//Mohamed
    oCharset.AssignWithConversion("IBM864");
  }
  else if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_LOGICAL )&&( oCharset.EqualsIgnoreCase("IBM864") ) ) {
    oCharset.AssignWithConversion("IBM864i");
  }
  else if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL )&&( oCharset.EqualsIgnoreCase("ISO-8859-6") ) )  {
    oCharset.AssignWithConversion("IBM864");
  }
  else if( ( mCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL )&&( oCharset.EqualsIgnoreCase("UTF-8") ) ) {
    oCharset.AssignWithConversion("IBM864");
  }

#endif
}

NS_IMETHODIMP nsFormFrame::GetEncoder(nsIUnicodeEncoder** encoder)
{
  *encoder = nsnull;
  nsAutoString charset;
  nsresult rv = NS_OK;
  GetSubmitCharset(charset);
#ifdef DEBUG_ftang
  printf("charset=%s\n", NS_LossyConvertUCS2toASCII(charset).get());
#endif
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    NS_GET_IID(nsICharsetConverterManager),
                                    (nsISupports**)&ccm);
  if(NS_SUCCEEDED(rv) && (nsnull != ccm)) {
     rv = ccm->GetUnicodeEncoder(&charset, encoder);
     nsServiceManager::ReleaseService( kCharsetConverterManagerCID, ccm);
     if (nsnull == encoder) {
       rv = NS_ERROR_FAILURE;
     }
     if (NS_SUCCEEDED(rv)) {
       rv = (*encoder)->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, (PRUnichar)'?');
     }
  }
  return NS_OK;
}

NS_IMETHODIMP nsFormFrame::GetPlatformEncoder(nsIUnicodeEncoder** encoder)
{
  *encoder = nsnull;
  nsAutoString localeCharset;
  nsresult rv = NS_OK;
  
  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    NS_GET_IID(nsICharsetConverterManager),
                                    (nsISupports**)&ccm);

  if(NS_SUCCEEDED(rv) && (nsnull != ccm)) {

     nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);

     if (NS_SUCCEEDED(rv)) {
        rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, localeCharset);
     }

     if (NS_FAILED(rv)) {
       NS_ASSERTION(0, "error getting locale charset, using ISO-8859-1");
       localeCharset.AssignWithConversion("ISO-8859-1");
       rv = NS_OK;
     }

     // get unicode converter mgr
     //nsCOMPtr<nsICharsetConverterManager> ccm = 
     //         do_GetService(kCharsetConverterManagerCID, &rv); 

     if (NS_SUCCEEDED(rv)) {
       rv = ccm->GetUnicodeEncoder(&localeCharset, encoder);
     }
   } 

  return NS_OK;
}


#define CRLF "\015\012"   
nsresult nsFormFrame::ProcessAsURLEncoded(nsIFormProcessor* aFormProcessor, PRBool isPost, nsString& aData, nsIFormControlFrame* aFrame)
{
  nsresult rv = NS_OK;
  nsString buf;
  PRBool firstTime = PR_TRUE;
  PRUint32 numChildren = mFormControls.Count();

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  if(NS_FAILED(GetEncoder(getter_AddRefs(encoder))))  // Non-fatal error
     encoder = nsnull;

  // collect and encode the data from the children controls
  for (PRUint32 childX = 0; childX < numChildren; childX++) {
    nsIFormControlFrame* child = (nsIFormControlFrame*) mFormControls.ElementAt(childX);
    if (child && child->IsSuccessful(aFrame)) {
      PRInt32 numValues = 0;
      PRInt32 maxNumValues = child->GetMaxNumValues();
      if (0 >= maxNumValues) {
        continue;
      }
      nsString* names = new nsString[maxNumValues];
      if (!names) {
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        nsString* values = new nsString[maxNumValues];
        if (!values) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
          if (PR_TRUE == child->GetNamesValues(maxNumValues, numValues, values, names)) {
            for (int valueX = 0; valueX < numValues; valueX++){
              if (PR_TRUE == firstTime) {
                firstTime = PR_FALSE;
              } else {
                buf.AppendWithConversion("&");
              }
              nsString* convName = URLEncode(names[valueX], encoder);
              buf += *convName;
              delete convName;
              buf.AppendWithConversion("=");
              nsAutoString newValue;
              newValue.Append(values[valueX]);
              if (aFormProcessor) {
                ProcessValue(*aFormProcessor, child, names[valueX], newValue);
              }
              nsString* convValue = URLEncode(newValue, encoder);
              buf += *convValue;
              delete convValue;
            }
          }
          delete [] values;
        }
        delete [] names;
      }
    }
  }
    aData.SetLength(0);
    if (isPost) {
      char size[16];
      sprintf(size, "%d", buf.Length());
      aData.AssignWithConversion("Content-type: application/x-www-form-urlencoded");
#ifdef SPECIFY_CHARSET_IN_CONTENT_TYPE
      nsString charset;
      GetSubmitCharset(charset);
      aData += "; charset=";
      aData += charset;
#endif
      aData.AppendWithConversion(CRLF);
      aData.AppendWithConversion("Content-Length: ");
      aData.AppendWithConversion(size);
      aData.AppendWithConversion(CRLF);
      aData.AppendWithConversion(CRLF);
    } 
  aData += buf;
  return rv;
}

// return the filename without the leading directories (Unix basename)
PRUint32
nsFormFrame::GetFileNameWithinPath(nsString aPathName)
{
  // We need to operator on Unicode strings and not on nsCStrings
  // because Shift_JIS and Big5 encoded filenames can have
  // embedded directory separators in them.
#ifdef XP_MAC
  // On a Mac the only invalid character in a file name is a :
  // so we have to avoid the test for '\'. We can't use
  // PR_DIRECTORY_SEPARATOR_STR (even though ':' is a dir sep for MacOS)
  // because this is set to '/' for reasons unknown to this coder.
  PRInt32 fileNameStart = aPathName.RFind(":");
#else
  PRInt32 fileNameStart = aPathName.RFind(PR_DIRECTORY_SEPARATOR_STR);
#endif
  // if no directory separator is found (-1), return the whole
  // string, otherwise return the basename only
  return (PRUint32) (fileNameStart + 1);
}

nsresult
nsFormFrame::GetContentType(char* aPathName, char** aContentType)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(aContentType, "null pointer");

  if (aPathName && *aPathName) {
    // Get file extension and mimetype from that.g936
    char* fileExt = aPathName + nsCRT::strlen(aPathName);
    while (fileExt > aPathName) {
      if (*(--fileExt) == '.') {
        break;
      }
    }

    if (*fileExt == '.' && *(fileExt + 1)) {
      nsCOMPtr<nsIMIMEService> MIMEService =
        do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv))
        return rv;

      rv = MIMEService->GetTypeFromExtension(fileExt + 1, aContentType);

      if (NS_SUCCEEDED(rv)) {
        return NS_OK;
      }
    }
  }
  *aContentType = nsCRT::strdup("application/octet-stream");
  if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

#define CONTENT_DISP "Content-Disposition: form-data; name=\""
#define FILENAME "\"; filename=\""
#define CONTENT_TYPE "Content-Type: "
#define CONTENT_ENCODING "Content-Encoding: "
#define CONTENT_TRANSFER "Content-Transfer-Encoding: "
#define BINARY_CONTENT "binary"
#define BUFSIZE 1024
#define MULTIPART "multipart/form-data"
#define SEP "--"

nsresult nsFormFrame::ProcessAsMultipart(nsIFormProcessor* aFormProcessor,
                                         nsIFile** aMultipartDataFile,
                                         nsIFormControlFrame* aFrame)
{
  *aMultipartDataFile = nsnull;

  nsresult rv;
  PRBool compatibleSubmit = PR_TRUE;
  nsCOMPtr<nsIPref> prefService(do_GetService(NS_PREF_CONTRACTID));
  if (prefService)
    prefService->GetBoolPref("browser.forms.submit.backwards_compatible",
                             &compatibleSubmit);

  char buffer[BUFSIZE];
  PRInt32 numChildren = mFormControls.Count();

  // Create a temporary file to write the form post data to
  nsCOMPtr<nsIFile> postDataFileName;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR,
                              getter_AddRefs(postDataFileName));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get special directory for file upload");
    return rv;
  }

  if (!postDataFileName) {
    NS_WARNING("Failed to get special directory for file upload");
    return NS_ERROR_FAILURE;
  }

  // append the name that we want to use
  rv = postDataFileName->Append("formpost");
  if (NS_FAILED(rv))
    return rv;

  // this file should be private
  rv = postDataFileName->CreateUnique(nsnull, nsIFile::NORMAL_FILE_TYPE,
                                      0600);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create unique file for post data!");
    return rv;
  }

  // create a stream from that file.
  nsCOMPtr<nsIOutputStream> postDataLocalFile;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(postDataLocalFile),
                                   postDataFileName,
                                   -1, /* default */
                                   0600); /* should be private */
  if (NS_FAILED(rv)) {
    NS_WARNING("Post data file stream couldn't be opened!");
    return rv;
  }

  // create a buffered output stream
  nsCOMPtr<nsIOutputStream> postDataFile;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(postDataFile),
                                  postDataLocalFile,
                                  8192);
  if (NS_FAILED(rv)) {
    NS_WARNING("Post data buffered file stream couldn't be opened!");
    return rv;
  }

  // write the content-type, boundary to the tmp file
  char boundary[80];
  sprintf(boundary, "---------------------------%d%d%d", 
          rand(), rand(), rand());
  sprintf(buffer, "Content-type: %s; boundary=%s" CRLF, MULTIPART, boundary);
  PRUint32 wantbytes = 0, gotbytes = 0;
  rv = postDataFile->Write(buffer, wantbytes = PL_strlen(buffer), &gotbytes);
  if (NS_FAILED(rv) || (wantbytes != gotbytes)) return rv;

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  if(NS_FAILED( GetEncoder(getter_AddRefs(encoder))))  // Non-fatal error
     encoder = nsnull;

  nsCOMPtr<nsIUnicodeEncoder> platformencoder;
  if(NS_FAILED(GetPlatformEncoder(getter_AddRefs(platformencoder))))  // Non-fatal error
     platformencoder = nsnull;


  PRInt32 boundaryLen = PL_strlen(boundary);
  PRInt32 contDispLen = PL_strlen(CONTENT_DISP);
  PRInt32 crlfLen     = PL_strlen(CRLF);
  PRInt32 sepLen      = PL_strlen(SEP);

  // compute the content length
  /////////////////////////////

  PRInt32 contentLen = 0; // extra crlf after content-length header not counted
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
        for (int valueX = 0; valueX < numValues; valueX++) {
          char* name = nsnull;
          char* value = nsnull;
          char* fname = nsnull; // basename (path removed)

          nsString valueStr = values[valueX];
          if (aFormProcessor) {
            ProcessValue(*aFormProcessor, child, names[valueX], valueStr);
          }

          if(encoder) {
              name  = UnicodeToNewBytes(names[valueX].get(), names[valueX].Length(), encoder);
          }
   
          //use the platformencoder only for values containing file names 
          PRUint32 fileNameStart = 0;
          if (NS_FORM_INPUT_FILE == type) { 
            fileNameStart = GetFileNameWithinPath(valueStr);
            if(platformencoder) {
                value  = UnicodeToNewBytes(valueStr.get(), valueStr.Length(), platformencoder);

                // filename with the leading dirs stripped
                fname = UnicodeToNewBytes(valueStr.get() + fileNameStart,
                                          valueStr.Length() - fileNameStart,
                                          platformencoder);
            }
          } else {
            if(encoder) {
                value  = UnicodeToNewBytes(valueStr.get(), valueStr.Length(), encoder);
            }
          } 

          if(nsnull == name)
            name  = ToNewCString(names[valueX]);
          if(nsnull == value)
            value = ToNewCString(valueStr);

          if (0 == names[valueX].Length()) {
            continue;
          }

          // convert value to CRLF line breaks
          char* newValue = nsLinebreakConverter::ConvertLineBreaks(value,
                           nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakNet);
          if (value)
            nsMemory::Free(value);
          value = newValue;
          
          // Add boundary line
          contentLen += sepLen + boundaryLen + crlfLen;

          // File inputs should include Content-Transfer-Encoding
          if (NS_FORM_INPUT_FILE == type && !compatibleSubmit) {
            contentLen += PL_strlen(CONTENT_TRANSFER);
            // XXX is there any way to tell when "8bit" or "7bit" etc may be more appropriate than
            // always using "binary"?
            contentLen += PL_strlen(BINARY_CONTENT);
            contentLen += crlfLen;
          }
          // End Content-Transfer-Encoding line

          // Add Content-Disp line
          contentLen += contDispLen;
          contentLen += PL_strlen(name);

          // File inputs also list filename on Content-Disp line
          if (NS_FORM_INPUT_FILE == type) { 
            contentLen += PL_strlen(FILENAME);
            contentLen += PL_strlen(fname);
          }
          // End Content-Disp Line (quote plus CRLF)
          contentLen += 1 + crlfLen;  // ending name quote plus CRLF

          // File inputs add Content-Type line
          if (NS_FORM_INPUT_FILE == type) {
            char* contentType = nsnull;
            rv = GetContentType(value, &contentType);
            if (NS_FAILED(rv)) break; // Need to free up anything here?
            contentLen += PL_strlen(CONTENT_TYPE);
            contentLen += PL_strlen(contentType) + crlfLen;
            nsCRT::free(contentType);
          }
	  
          // Blank line before value
          contentLen += crlfLen;

          // File inputs add file contents next
          if (NS_FORM_INPUT_FILE == type &&
              PL_strlen(value)) { // Don't bother if no file specified
            do {
              // Because we have a native path to the file we can't use PR_GetFileInfo
              // on the Mac as it expects a Unix style path.  Instead we'll use our
              // spiffy new nsILocalFile
              nsILocalFile* tempFile = nsnull;
              rv = NS_NewLocalFile(value, PR_TRUE, &tempFile);
              NS_ASSERTION(tempFile, "Couldn't create nsIFileSpec to get file size!");
              if (NS_FAILED(rv) || !tempFile)
                break; // NS_ERROR_OUT_OF_MEMORY
              PRUint32 fileSize32 = 0;
              PRInt64  fileSize = LL_Zero();
              rv = tempFile->GetFileSize(&fileSize);
              if (NS_FAILED(rv)) {
                NS_RELEASE(tempFile);
                break;
              }
              LL_L2UI(fileSize32, fileSize);
              contentLen += fileSize32;
              NS_RELEASE(tempFile);
            } while (PR_FALSE);

            // Add CRLF after file
            contentLen += crlfLen;
          } else {
            // Non-file inputs add value line
            contentLen += PL_strlen(value) + crlfLen;
          }
          if (name)
            nsMemory::Free(name);
          if (value)
            nsMemory::Free(value);
          if (fname)
            nsMemory::Free(fname);
        }
        delete [] names;
        delete [] values;
      }

      *aMultipartDataFile = postDataFileName.get();
      NS_ADDREF(*aMultipartDataFile);
    }
  }

  // Add the post file boundary line
  contentLen += sepLen + boundaryLen + sepLen + crlfLen;

  // write the content 
  ////////////////////

  sprintf(buffer, "Content-Length: %d" CRLF CRLF, contentLen);
  rv = postDataFile->Write(buffer, wantbytes = PL_strlen(buffer), &gotbytes);
  if (NS_SUCCEEDED(rv) && (wantbytes == gotbytes)) {

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
            char* name = nsnull;
            char* value = nsnull;
            char* fname = nsnull; // basename (path removed)

            nsString valueStr = values[valueX];

            if(encoder) {
              name  = UnicodeToNewBytes(names[valueX].get(), names[valueX].Length(), encoder);
            }

            //use the platformencoder only for values containing file names 
            PRUint32 fileNameStart = 0;
            if (NS_FORM_INPUT_FILE == type) { 
              fileNameStart = GetFileNameWithinPath(valueStr);
              if(platformencoder) {
                  value = UnicodeToNewBytes(valueStr.get(), valueStr.Length(), platformencoder);

                  // filename with the leading dirs stripped
                  fname = UnicodeToNewBytes(valueStr.get() + fileNameStart,
                                            valueStr.Length() - fileNameStart, platformencoder);
              }
            } else { 
              if(encoder) {
                  value = UnicodeToNewBytes(valueStr.get(), valueStr.Length(), encoder);
              }
            }

            if(nsnull == name)
              name  = ToNewCString(names[valueX]);
            if(nsnull == value)
              value = ToNewCString(valueStr);

            if (0 == names[valueX].Length()) {
              continue;
            }

            // convert value to CRLF line breaks
            char* newValue = nsLinebreakConverter::ConvertLineBreaks(value,
                             nsLinebreakConverter::eLinebreakAny, nsLinebreakConverter::eLinebreakNet);
            if (value)
              nsMemory::Free(value);
            value = newValue;

       	    // Print boundary line
            sprintf(buffer, SEP "%s" CRLF, boundary);
            rv = postDataFile->Write(buffer, wantbytes = PL_strlen(buffer), &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs should include Content-Transfer-Encoding to prep server side
            // MIME decoders
            if (NS_FORM_INPUT_FILE == type && !compatibleSubmit) {
              rv = postDataFile->Write(CONTENT_TRANSFER, wantbytes = PL_strlen(CONTENT_TRANSFER), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

              // XXX is there any way to tell when "8bit" or "7bit" etc may be more appropriate than
              // always using "binary"?

              rv = postDataFile->Write(BINARY_CONTENT, wantbytes = PL_strlen(BINARY_CONTENT), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

              rv = postDataFile->Write(CRLF, wantbytes = PL_strlen(CRLF), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }

            // Print Content-Disp line
            rv = postDataFile->Write(CONTENT_DISP, wantbytes = contDispLen, &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;	  
            rv = postDataFile->Write(name, wantbytes = PL_strlen(name), &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs also list filename on Content-Disp line
            if (NS_FORM_INPUT_FILE == type) {
              rv = postDataFile->Write(FILENAME, wantbytes = PL_strlen(FILENAME), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              rv = postDataFile->Write(fname, wantbytes = PL_strlen(fname), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }

            // End Content Disp
            rv = postDataFile->Write("\"" CRLF , wantbytes = PL_strlen("\"" CRLF), &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs write Content-Type line
            if (NS_FORM_INPUT_FILE == type) {
              char* contentType = nsnull;
              rv = GetContentType(value, &contentType);
              if (NS_FAILED(rv)) break;
              rv = postDataFile->Write(CONTENT_TYPE, wantbytes = PL_strlen(CONTENT_TYPE), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              rv = postDataFile->Write(contentType, wantbytes = PL_strlen(contentType), &gotbytes);
              nsCRT::free(contentType);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              rv = postDataFile->Write(CRLF, wantbytes = PL_strlen(CRLF), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              // end content-type header
            }

            // Blank line before value
            rv = postDataFile->Write(CRLF, wantbytes = PL_strlen(CRLF), &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs print file contents next
            if (NS_FORM_INPUT_FILE == type) {
              nsIFileSpec* contentFile = nsnull;
              rv = NS_NewFileSpec(&contentFile);
              NS_ASSERTION(contentFile, "Post content file couldn't be created!");
              if (NS_FAILED(rv) || !contentFile) break; // NS_ERROR_OUT_OF_MEMORY
              rv = contentFile->SetNativePath(value);
              NS_ASSERTION(contentFile, "Post content file path couldn't be set!");
              if (NS_FAILED(rv)) {
                NS_RELEASE(contentFile);
                break;
              }
              // Print file contents
              PRInt32 size = 1;
              while (1) {
                char* readbuffer = nsnull;
                rv = contentFile->Read(&readbuffer, BUFSIZE, &size);
                if (NS_FAILED(rv) || 0 >= size) break;
                rv = postDataFile->Write(readbuffer, wantbytes = size, &gotbytes);
                if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              }
              NS_RELEASE(contentFile);
              // Print CRLF after file
              rv = postDataFile->Write(CRLF, wantbytes = PL_strlen(CRLF), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }

            // Non-file inputs print value line
            else {
              rv = postDataFile->Write(value, wantbytes = PL_strlen(value), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              rv = postDataFile->Write(CRLF, wantbytes = PL_strlen(CRLF), &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }
            if (name)
              nsMemory::Free(name);
            if (value)
              nsMemory::Free(value);
            if (fname)
              nsMemory::Free(fname);
          }
          delete [] names;
          delete [] values;
        }
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    sprintf(buffer, SEP "%s" SEP CRLF, boundary);
    rv = postDataFile->Write(buffer, wantbytes = PL_strlen(buffer), &gotbytes);
    if (NS_SUCCEEDED(rv) && (wantbytes == gotbytes)) {
      rv = postDataFile->Close();
      if (NS_FAILED(rv)) {
        NS_WARNING("Failed to close post data file stream!");
        return rv;
      }
      rv = postDataLocalFile->Close();
      if (NS_FAILED(rv)) {
        NS_WARNING("FAiled to close post data file!");
        return rv;
      }
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "Generating the form post temp file failed.\n");
  return rv;
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

// Bug 99920 - Finds the first submit button and how many text inputs there
// if there is only one text input or password then submission can take place
// or it can take place if it finds at least one submit button
nsIFrame* 
nsFormFrame::GetFirstSubmitButtonAndTxtCnt(PRInt32& aInputTxtCnt)
{
  nsIFrame* submitFrame = nsnull;
  aInputTxtCnt          = 0;

  PRInt32 numControls = mFormControls.Count();
  for (int i = 0; i < numControls; i++) {
    nsIFormControlFrame* fcFrame = (nsIFormControlFrame*) mFormControls.ElementAt(i);
    PRInt32 type;
    fcFrame->GetType(&type);
    if ((type == NS_FORM_INPUT_SUBMIT || type == NS_FORM_BUTTON_SUBMIT) && 
        submitFrame == nsnull) {
      NS_ASSERTION(fcFrame->QueryInterface(NS_GET_IID(nsIFrame), (void**)&submitFrame) == NS_OK,
                   "This has to be a frame!");
    } else if (type == NS_FORM_INPUT_TEXT || type == NS_FORM_INPUT_PASSWORD) {
      aInputTxtCnt++;
    }
  }
  return submitFrame;
}

