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



#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsID.h"


#include "nsFormFrame.h"
#include "nsIFormControlFrame.h"
#include "nsFormControlFrame.h"
#include "nsFileControlFrame.h"

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
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMNSHTMLFormElement.h"
#include "nsDOMError.h"
#include "nsHTMLParts.h"
#include "nsHTMLReflowCommand.h"
#include "nsICategoryManager.h"
#include "nsIFormSubmitObserver.h"

// Security
#include "nsIScriptSecurityManager.h"

#include "nsIDOMWindowInternal.h"

PRBool nsFormFrame::gInitPasswordManager = PR_FALSE;

//----------------------------------------------------------------------

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
  PRInt32 numControls = mFormControls.Count();
  PRInt32 i;

  for (i = (numControls-1); i>=0; i--) {
    nsIFormControlFrame* fcFrame = (nsIFormControlFrame*) mFormControls.ElementAt(i);
    fcFrame->SetFormFrame(nsnull);
  }
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
    
  nsHTMLReflowCommand* reflowCmd;
  nsresult rv = NS_NewHTMLReflowCommand(&reflowCmd, aFrame,
                                        eReflowType_StyleChanged);
  if (NS_SUCCEEDED(rv)) {
    shell->AppendReflowCommand(reflowCmd);
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

