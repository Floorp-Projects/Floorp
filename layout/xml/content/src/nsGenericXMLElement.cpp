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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Annema <disttsc@bart.nl>
 */
#include "nsGenericXMLElement.h"

#include "nsIAtom.h"
#include "nsINodeInfo.h"
#include "nsIDocument.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIDOMRange.h"
#include "nsRange.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsISizeOfHandler.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"
#include "nsFrame.h"
#include "nsIPresShell.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsString.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsINameSpaceManager.h"
#include "nsLayoutAtoms.h"
#include "prprf.h"
#include "prmem.h"

#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMViewCSS.h"
#include "nsIXBLService.h"
#include "nsIBindingManager.h"
#include "nsIXBLBinding.h"

class nsIWebShell;

static NS_DEFINE_IID(kIDOMAttrIID, NS_IDOMATTR_IID);
static NS_DEFINE_IID(kIDOMNamedNodeMapIID, NS_IDOMNAMEDNODEMAP_IID);
static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
static NS_DEFINE_IID(kIDOMDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMDocumentFragmentIID, NS_IDOMDOCUMENTFRAGMENT_IID);

nsGenericXMLElement::nsGenericXMLElement()
{
  mNameSpace = nsnull;
}

nsGenericXMLElement::~nsGenericXMLElement()
{
  NS_IF_RELEASE(mNameSpace);
}

nsresult
nsGenericXMLElement::CopyInnerTo(nsIContent* aSrcContent,
                                 nsGenericXMLElement* aDst,
                                 PRBool aDeep)
{
  nsresult result = nsGenericContainerElement::CopyInnerTo(aSrcContent, aDst, aDeep);
  if (NS_OK == result) {
    aDst->mNameSpace = mNameSpace;
    NS_IF_ADDREF(mNameSpace);
  }
  return NS_OK;
}

  nsresult GetNodeName(nsAWritableString& aNodeName);
  nsresult GetLocalName(nsAWritableString& aLocalName);

nsresult
nsGenericXMLElement::GetNodeName(nsAWritableString& aNodeName)
{
  return mNodeInfo->GetQualifiedName(aNodeName);
}

nsresult
nsGenericXMLElement::GetLocalName(nsAWritableString& aLocalName)
{
  return mNodeInfo->GetLocalName(aLocalName);
}

nsresult
nsGenericXMLElement::GetScriptObject(nsIScriptContext* aContext, 
                                     void** aScriptObject)
{
  nsresult res = NS_OK;

  // XXX Yuck! Reaching into the generic content object isn't good.
  nsDOMSlots *slots = GetDOMSlots();
  if (nsnull == slots->mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    nsAutoString tag;
    mNodeInfo->GetQualifiedName(tag);

    res = factory->NewScriptXMLElement(tag, aContext, mContent,
                                       mParent, (void**)&slots->mScriptObject);
    NS_RELEASE(factory);
    
    if (nsnull != mDocument) {
      aContext->AddNamedReference((void *)&slots->mScriptObject,
                                  slots->mScriptObject,
                                  "nsGenericXMLElement::mScriptObject");

      // See if we have a frame.  
      nsCOMPtr<nsIPresShell> shell = getter_AddRefs(mDocument->GetShellAt(0));
      if (shell) {
        nsIFrame* frame;
        shell->GetPrimaryFrameFor(mContent, &frame);
        if (!frame) {
          // We must ensure that the XBL Binding is installed before we hand
          // back this object.
          nsCOMPtr<nsIBindingManager> bindingManager;
          mDocument->GetBindingManager(getter_AddRefs(bindingManager));
          nsCOMPtr<nsIXBLBinding> binding;
          bindingManager->GetBinding(mContent, getter_AddRefs(binding));
          if (!binding) {
            nsCOMPtr<nsIScriptGlobalObject> global;
            mDocument->GetScriptGlobalObject(getter_AddRefs(global));
            nsCOMPtr<nsIDOMViewCSS> viewCSS(do_QueryInterface(global));
            if (viewCSS) {
              nsCOMPtr<nsIDOMCSSStyleDeclaration> cssDecl;
              nsAutoString empty;
              nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mContent));
              viewCSS->GetComputedStyle(elt, empty, getter_AddRefs(cssDecl));
              if (cssDecl) {
                nsAutoString behavior; behavior.AssignWithConversion("-moz-binding");
                nsAutoString value;
                cssDecl->GetPropertyValue(behavior, value);
                if (!value.IsEmpty()) {
                  // We have a binding that must be installed.
                  nsresult rv;
                  PRBool dummy;
                  NS_WITH_SERVICE(nsIXBLService, xblService, "component://netscape/xbl", &rv);
                  xblService->LoadBindings(mContent, value, PR_FALSE, getter_AddRefs(binding), &dummy);
                  if (binding) {
                    binding->ExecuteAttachedHandler();
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  *aScriptObject = slots->mScriptObject;
  return res;
}

nsresult
nsGenericXMLElement::GetNameSpaceID(PRInt32& aNameSpaceID) const
{
  return mNodeInfo->GetNamespaceID(aNameSpaceID);
}

nsresult
nsGenericXMLElement::MaybeTriggerAutoLink(nsIWebShell *aShell)
{
  // Implement in subclass
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult 
nsGenericXMLElement::SetContainingNameSpace(nsINameSpace* aNameSpace)
{
  NS_IF_RELEASE(mNameSpace);
  
  mNameSpace = aNameSpace;
  
  NS_IF_ADDREF(mNameSpace);

  return NS_OK;
}

nsresult 
nsGenericXMLElement::GetContainingNameSpace(nsINameSpace*& aNameSpace) const
{
  aNameSpace = mNameSpace;
  NS_IF_ADDREF(aNameSpace);

  return NS_OK;  
}
