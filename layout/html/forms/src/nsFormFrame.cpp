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

#include "nsIDocument.h"
#include "nsILinkHandler.h"
#include "nsGfxRadioControlFrame.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsDOMError.h"
#include "nsHTMLParts.h"
#include "nsIReflowCommand.h"
#include "nsICategoryManager.h"
#include "nsIFormSubmitObserver.h"

// Security
#include "nsIScriptSecurityManager.h"

#include "nsIDOMWindowInternal.h"

PRBool nsFormFrame::gInitPasswordManager = PR_FALSE;

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
                                  NS_PASSWORDMANAGER_CATEGORY);
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

  // determine which radio buttons belong to which radio groups, unnamed radio
  // buttons don't go into any group since they can't be submitted. Determine
  // which controls are capable of form submission.

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
nsFormFrame::GetName(nsIFrame* aChildFrame,
                     nsAString& aName,
                     nsIContent* aContent)
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
nsFormFrame::GetValue(nsIFrame* aChildFrame,
                      nsAString& aValue,
                      nsIContent* aContent)
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
    if ((type == NS_FORM_INPUT_SUBMIT || type == NS_FORM_BUTTON_SUBMIT || type == NS_FORM_INPUT_IMAGE) && 
        submitFrame == nsnull) {
      CallQueryInterface(fcFrame, &submitFrame);
      NS_ASSERTION(submitFrame, "This has to be a frame!");
    } else if (type == NS_FORM_INPUT_TEXT || type == NS_FORM_INPUT_PASSWORD) {
      aInputTxtCnt++;
    }
  }
  return submitFrame;
}

