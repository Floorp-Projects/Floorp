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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
 *
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

#include "nsXBLContentSink.h"
#include "nsIDocument.h"
#include "nsIBindingManager.h"
#include "nsIDOMNode.h"
#include "nsIParser.h"
#include "nsXBLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsHTMLAtoms.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLTokens.h"
#include "nsIURI.h"
#include "nsTextFragment.h"
#include "nsXULElement.h"
#include "nsXULAtoms.h"
#include "nsXBLProtoImplProperty.h"
#include "nsXBLProtoImplMethod.h"
#include "nsXBLProtoImplField.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

nsresult
NS_NewXBLContentSink(nsIXMLContentSink** aResult,
                     nsIDocument* aDoc,
                     nsIURI* aURL,
                     nsIWebShell* aWebShell)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  nsXBLContentSink* it;
  NS_NEWXPCOM(it, nsXBLContentSink);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;
  nsresult rv = it->Init(aDoc, aURL, aWebShell);
  if (NS_FAILED(rv))
    return rv;
  return it->QueryInterface(NS_GET_IID(nsIXMLContentSink), (void **)aResult);
}

nsXBLContentSink::nsXBLContentSink()
{
  mState = eXBL_InDocument;
  mSecondaryState = eXBL_None;
  mDocInfo = nsnull;
  mIsChromeOrResource = PR_FALSE;
  mImplementation = nsnull;
  mImplMember = nsnull;
  mProperty = nsnull;
  mMethod = nsnull;
  mField = nsnull;
  mPrettyPrintXML = PR_FALSE;
}

nsXBLContentSink::~nsXBLContentSink()
{
}

nsresult
nsXBLContentSink::Init(nsIDocument* aDoc,
                       nsIURI* aURL,
                       nsIWebShell* aContainer)
{
  nsresult rv;
  rv = nsXMLContentSink::Init(aDoc, aURL, aContainer, nsnull);
  return rv;
}

nsresult
nsXBLContentSink::FlushText(PRBool aCreateTextNode,
                            PRBool* aDidFlush)
{
  if (mTextLength == 0) {
    if (aDidFlush)
      *aDidFlush = PR_FALSE;
    return NS_OK;
  }

  const nsASingleFragmentString& text = Substring(mText, mText+mTextLength);
  if (mState == eXBL_InHandlers) {
    // Get the text and add it to the event handler.
    if (mSecondaryState == eXBL_InHandler)
      mHandler->AppendHandlerText(text);
    mTextLength = 0;
    if (aDidFlush)
      *aDidFlush = PR_TRUE;
    return NS_OK;
  }
  else if (mState == eXBL_InImplementation) {
    if (mSecondaryState == eXBL_InConstructor ||
        mSecondaryState == eXBL_InDestructor) {
      // Construct a handler for the constructor/destructor.
      // XXXdwh This is just awful.  These used to be handlers called
      // BindingAttached and BindingDetached, and they're still implemented
      // using handlers.  At some point, we need to change these to just
      // be special functions on the class instead.
      nsCOMPtr<nsIXBLPrototypeHandler> handler;
      if (mSecondaryState == eXBL_InConstructor)
        mBinding->GetConstructor(getter_AddRefs(handler));
      else
        mBinding->GetDestructor(getter_AddRefs(handler));

      // Get the text and add it to the constructor/destructor.
      handler->AppendHandlerText(text);
    }
    else if (mSecondaryState == eXBL_InGetter ||
             mSecondaryState == eXBL_InSetter) {
      // Get the text and add it to the constructor/destructor.
      if (mSecondaryState == eXBL_InGetter)
        mProperty->AppendGetterText(text);
      else
        mProperty->AppendSetterText(text);
    }
    else if (mSecondaryState == eXBL_InBody) {
      // Get the text and add it to the method
      mMethod->AppendBodyText(text);
    }
    else if (mSecondaryState == eXBL_InField) {
      // Get the text and add it to the method
      mField->AppendFieldText(text);
    }
    mTextLength = 0;
    if (aDidFlush)
      *aDidFlush = PR_TRUE;
    return NS_OK;
  }

  PRBool isWS = PR_TRUE;
  if (mTextLength > 0) {
    const PRUnichar* cp = mText;
    const PRUnichar* end = mText + mTextLength;
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (!XP_IS_SPACE(ch)) {
        isWS = PR_FALSE;
        break;
      }
    }
  }

  if (isWS && mTextLength > 0) {
    mTextLength = 0;
    if (aDidFlush)
      *aDidFlush = PR_TRUE;
    return NS_OK;
  }

  return nsXMLContentSink::FlushText(aCreateTextNode, aDidFlush);
}

NS_IMETHODIMP
nsXBLContentSink::ReportError(const PRUnichar* aErrorText, 
                              const PRUnichar* aSourceText)
{
  // XXX We should make sure the binding has no effect, but that it also
  // gets destroyed properly without leaking.  Perhaps we should even
  // ensure that the content that was bound is displayed with no
  // binding.

  // Report the error to the error console.
  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (consoleService) {
    // XXX It would be nice if the parser didn't pre-format it for us,
    // because then we could create a instance of nsIScriptError and the
    // error console would format this much more nicely for us.
    // However, that would require duplicating even more code between
    // the XML content sink and the XUL content sink.

    consoleService->LogStringMessage(aErrorText);
  }

#ifdef DEBUG
  // Report the error to stderr.
  fprintf(stderr,
          "\n%s\n%s\n\n",
          NS_LossyConvertUCS2toASCII(aErrorText).get(),
          NS_LossyConvertUCS2toASCII(aSourceText).get());
#endif

  // Most of what this does won't be too useful, but whatever...
  return nsXMLContentSink::ReportError(aErrorText, aSourceText);
}

NS_IMETHODIMP 
nsXBLContentSink::HandleStartElement(const PRUnichar *aName, 
                                     const PRUnichar **aAtts, 
                                     PRUint32 aAttsCount, 
                                     PRUint32 aIndex, 
                                     PRUint32 aLineNumber)
{
  nsresult rv = nsXMLContentSink::HandleStartElement(aName,aAtts,aAttsCount,aIndex,aLineNumber);
  if (NS_FAILED(rv))
    return rv;

  if (mState == eXBL_InBinding && !mBinding)
    ConstructBinding();
  
  return rv;
}

NS_IMETHODIMP 
nsXBLContentSink::HandleEndElement(const PRUnichar *aName)
{
  FlushText();

  if (mState != eXBL_InDocument) {
    nsCOMPtr<nsIAtom> nameSpacePrefix, tagAtom;

    SplitXMLName(nsDependentString(aName), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(tagAtom));

    PRInt32 nameSpaceID = GetNameSpaceId(nameSpacePrefix);
    if (nameSpaceID == kNameSpaceID_XBL) {
      if (mState == eXBL_InHandlers) {
        if (tagAtom == nsXBLAtoms::handlers) {
          mState = eXBL_InBinding;
          mHandler = nsnull;
        }
        else if (tagAtom == nsXBLAtoms::handler)
          mSecondaryState = eXBL_None;
        return NS_OK;
      }
      else if (mState == eXBL_InResources) {
        if (tagAtom == nsXBLAtoms::resources)
          mState = eXBL_InBinding;
        return NS_OK;
      }
      else if (mState == eXBL_InImplementation) {
        if (tagAtom == nsXBLAtoms::implementation)
          mState = eXBL_InBinding;
        else if (tagAtom == nsXBLAtoms::property) {
          mSecondaryState = eXBL_None;
          mProperty = nsnull;
        }
        else if (tagAtom == nsXBLAtoms::method) {
          mSecondaryState = eXBL_None;
          mMethod = nsnull;
        }
        else if (tagAtom == nsXBLAtoms::field) {
          mSecondaryState = eXBL_None;
          mField = nsnull;
        }
        else if (tagAtom == nsXBLAtoms::constructor ||
                 tagAtom == nsXBLAtoms::destructor)
          mSecondaryState = eXBL_None;
        else if (tagAtom == nsXBLAtoms::getter ||
                 tagAtom == nsXBLAtoms::setter)
          mSecondaryState = eXBL_InProperty;
        else if (tagAtom == nsXBLAtoms::parameter ||
                 tagAtom == nsXBLAtoms::body)
          mSecondaryState = eXBL_InMethod;
        return NS_OK;
      }

      nsresult rv = nsXMLContentSink::HandleEndElement(aName);
      if (NS_FAILED(rv))
        return rv;

      if (mState == eXBL_InImplementation && tagAtom == nsXBLAtoms::implementation)
        mState = eXBL_InBinding;
      else if (mState == eXBL_InBinding && tagAtom == nsXBLAtoms::binding) {
        mState = eXBL_InDocument;
        mBinding->Initialize();
        mBinding = nsnull; // Clear our current binding ref.
      }

      return NS_OK;
    }
  }

  return nsXMLContentSink::HandleEndElement(aName);
}

NS_IMETHODIMP 
nsXBLContentSink::HandleCDataSection(const PRUnichar *aData, 
                                     PRUint32 aLength)
{
  if (mState == eXBL_InHandlers || mState == eXBL_InImplementation)
    return AddText(aData, aLength);
  return nsXMLContentSink::HandleCDataSection(aData, aLength);
}

PRBool 
nsXBLContentSink::OnOpenContainer(const PRUnichar **aAtts, 
                                  PRUint32 aAttsCount, 
                                  PRInt32 aNameSpaceID, 
                                  nsIAtom* aTagName)
{
  PRBool ret = PR_TRUE;
  if (aNameSpaceID == kNameSpaceID_XBL) {
    if (aTagName == nsXBLAtoms::bindings) {
      NS_NewXBLDocumentInfo(mDocument, &mDocInfo);
      if (!mDocInfo)
        return NS_ERROR_FAILURE;

      nsCOMPtr<nsIBindingManager> bindingManager;
      mDocument->GetBindingManager(getter_AddRefs(bindingManager));
      bindingManager->PutXBLDocumentInfo(mDocInfo);

      nsCOMPtr<nsIURI> url;
      mDocument->GetDocumentURL(getter_AddRefs(url));
      
      PRBool isChrome = PR_FALSE;
      PRBool isRes = PR_FALSE;

      url->SchemeIs("chrome", &isChrome);
      url->SchemeIs("resource", &isRes);
      mIsChromeOrResource = isChrome || isRes;
      
      nsIXBLDocumentInfo* info = mDocInfo;
      NS_RELEASE(info); // We keep a weak ref. We've created a cycle between doc/binding manager/doc info.
    }
    else if (aTagName == nsXBLAtoms::binding)
      mState = eXBL_InBinding;
    else if (aTagName == nsXBLAtoms::handlers) {
      mState = eXBL_InHandlers;
      ret = PR_FALSE; // The XML content sink should not do anything with <handlers>.
    }
    else if (aTagName == nsXBLAtoms::handler) {
      mSecondaryState = eXBL_InHandler;
      ConstructHandler(aAtts);
      ret = PR_FALSE;
    }
    else if (aTagName == nsXBLAtoms::resources) {
      mState = eXBL_InResources;
      ret = PR_FALSE; // The XML content sink should ignore all <resources>.
    }
    else if (mState == eXBL_InResources) {
      if (aTagName == nsXBLAtoms::stylesheet || aTagName == nsXBLAtoms::image)
        ConstructResource(aAtts, aTagName);
      ret = PR_FALSE; // The XML content sink should ignore everything within a <resources> block.
    }
    else if (aTagName == nsXBLAtoms::implementation) {
      mState = eXBL_InImplementation;
      ConstructImplementation(aAtts);
      ret = PR_FALSE; // The XML content sink should ignore the <implementation>.
    }
    else if (mState == eXBL_InImplementation) {
      if (aTagName == nsXBLAtoms::constructor) {
        mSecondaryState = eXBL_InConstructor;
        nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
        NS_NewXBLPrototypeHandler(nsnull, nsnull, nsnull, nsnull, nsnull,
                                  nsnull, nsnull, nsnull, nsnull, nsnull,
                                  getter_AddRefs(newHandler));
        newHandler->SetEventName(nsXBLAtoms::constructor);
        mBinding->SetConstructor(newHandler);
      }
      else if (aTagName == nsXBLAtoms::destructor) {
        mSecondaryState = eXBL_InDestructor;
        nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
        NS_NewXBLPrototypeHandler(nsnull, nsnull, nsnull, nsnull, nsnull,
                                  nsnull, nsnull, nsnull, nsnull, nsnull,
                                  getter_AddRefs(newHandler));
        newHandler->SetEventName(nsXBLAtoms::destructor);
        mBinding->SetDestructor(newHandler);
      }
      else if (aTagName == nsXBLAtoms::field) {
        mSecondaryState = eXBL_InField;
        ConstructField(aAtts);
      }
      else if (aTagName == nsXBLAtoms::property) {
        mSecondaryState = eXBL_InProperty;
        ConstructProperty(aAtts);
      }
      else if (aTagName == nsXBLAtoms::getter)
        mSecondaryState = eXBL_InGetter;
      else if (aTagName == nsXBLAtoms::setter)
        mSecondaryState = eXBL_InSetter;
      else if (aTagName == nsXBLAtoms::method) {
        mSecondaryState = eXBL_InMethod;
        ConstructMethod(aAtts);
      }
      else if (aTagName == nsXBLAtoms::parameter)
        ConstructParameter(aAtts);
      else if (aTagName == nsXBLAtoms::body)
        mSecondaryState = eXBL_InBody;

      ret = PR_FALSE; // Ignore everything we encounter inside an <implementation> block.
    }
  }

  return ret;
}

void 
nsXBLContentSink::ConstructBinding()
{
  nsCOMPtr<nsIContent> binding = getter_AddRefs(GetCurrentContent());
  nsAutoString id;
  binding->GetAttr(kNameSpaceID_None, nsHTMLAtoms::id, id);
  nsCAutoString cid; cid.AssignWithConversion(id);

  if (!cid.IsEmpty()) {
    NS_NewXBLPrototypeBinding(cid, binding, mDocInfo, getter_AddRefs(mBinding));
    mDocInfo->SetPrototypeBinding(cid, mBinding);
    binding->UnsetAttr(kNameSpaceID_None, nsHTMLAtoms::id, PR_FALSE);
  }
}


void
nsXBLContentSink::ConstructHandler(const PRUnichar **aAtts)
{
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;

  const PRUnichar* event          = nsnull;
  const PRUnichar* modifiers      = nsnull;
  const PRUnichar* button         = nsnull;
  const PRUnichar* clickcount     = nsnull;
  const PRUnichar* keycode        = nsnull;
  const PRUnichar* charcode       = nsnull;
  const PRUnichar* phase          = nsnull;
  const PRUnichar* command        = nsnull;
  const PRUnichar* action         = nsnull;
  const PRUnichar* preventdefault = nsnull;

  for (; *aAtts; aAtts += 2) {
    // Get upper-cased key

    SplitXMLName(nsDependentString(aAtts[0]), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (nameAtom == nsXBLAtoms::event)
      event = aAtts[1];
    else if (nameAtom == nsXBLAtoms::modifiers)
      modifiers = aAtts[1];
    else if (nameAtom == nsXBLAtoms::button)
      button = aAtts[1];
    else if (nameAtom == nsXBLAtoms::clickcount)
      clickcount = aAtts[1];
    else if (nameAtom == nsXBLAtoms::keycode)
      keycode = aAtts[1];
    else if (nameAtom == nsXBLAtoms::key || nameAtom == nsXBLAtoms::charcode)
      charcode = aAtts[1];
    else if (nameAtom == nsXBLAtoms::phase)
      phase = aAtts[1];
    else if (nameAtom == nsXBLAtoms::command)
      command = aAtts[1];
    else if (nameAtom == nsXBLAtoms::action)
      action = aAtts[1];
    else if (nameAtom == nsXBLAtoms::preventdefault)
      preventdefault = aAtts[1];
    else {
       // Nope, it's some irrelevant attribute. Ignore it and move on.
     }
  }

  if (command && !mIsChromeOrResource)
    // Make sure the XBL doc is chrome or resource if we have a command
    // shorthand syntax.
    return; // Don't even make this handler.

  // All of our pointers are now filled in.  Construct our handler with all of these
  // parameters.
  nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
  NS_NewXBLPrototypeHandler(event, phase, action, command, 
                            keycode, charcode, modifiers, button,
                            clickcount, preventdefault,
                            getter_AddRefs(newHandler));
  if (newHandler) {
    // Add this handler to our chain of handlers.
    if (mHandler)
      mHandler->SetNextHandler(newHandler); // Already have a chain. Just append to the end.
    else
      mBinding->SetPrototypeHandlers(newHandler); // We're the first handler in the chain.

    mHandler = newHandler; // Adjust our mHandler pointer to point to the new last handler in the chain.
  }
}

void
nsXBLContentSink::ConstructResource(const PRUnichar **aAtts,
                                    nsIAtom* aResourceType)
{
  if (!mBinding)
    return;

  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  for (; *aAtts; aAtts += 2) {
    // Get upper-cased key
    const nsDependentString key(aAtts[0]);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("src"))) {
      mBinding->AddResource(aResourceType, nsDependentString(aAtts[1]));
      break;
    }
  }
}

void
nsXBLContentSink::ConstructImplementation(const PRUnichar **aAtts)
{
  mImplementation = nsnull;
  mImplMember = nsnull;
      
  if (!mBinding)
    return;

  const PRUnichar* name = nsnull;
  
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  for (; *aAtts; aAtts +=2) {
    // Get upper-cased key

    SplitXMLName(nsDependentString(aAtts[0]), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (nameAtom == nsXBLAtoms::name) {
      name = aAtts[1];
    }
    else if (nameAtom == nsXBLAtoms::implements) {
      mBinding->ConstructInterfaceTable(nsDependentString(aAtts[1]));
    }
  }

  NS_NewXBLProtoImpl(mBinding, name, &mImplementation);
}

void
nsXBLContentSink::ConstructField(const PRUnichar **aAtts)
{
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;

  const PRUnichar* name     = nsnull;
  const PRUnichar* readonly = nsnull;
  
  for (; *aAtts; aAtts += 2) {
    // Get upper-cased key
    SplitXMLName(nsDependentString(aAtts[0]), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (nameAtom == nsXBLAtoms::name) {
      name = aAtts[1];
    }
    else if (nameAtom == nsXBLAtoms::readonly) {
      readonly = aAtts[1];
    }
  }

  // All of our pointers are now filled in.  Construct our field with all of these
  // parameters.
  mField = new nsXBLProtoImplField(name, readonly);
  if (mField) {
    // Add this member to our chain.
    if (mImplMember)
      mImplMember->SetNext(mField); // Already have a chain. Just append to the end.
    else
      mImplementation->SetMemberList(mField); // We're the first member in the chain.

    mImplMember = mField; // Adjust our pointer to point to the new last member in the chain.
  }
}

void
nsXBLContentSink::ConstructProperty(const PRUnichar **aAtts)
{
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;

  const PRUnichar* name     = nsnull;
  const PRUnichar* readonly = nsnull;
  const PRUnichar* onget    = nsnull;
  const PRUnichar* onset    = nsnull;
  
  for (; *aAtts; aAtts += 2) {
    // Get upper-cased key
    SplitXMLName(nsDependentString(aAtts[0]), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (nameAtom == nsXBLAtoms::name) {
      name = aAtts[1];
    }
    else if (nameAtom == nsXBLAtoms::readonly) {
      readonly = aAtts[1];
    }
    else if (nameAtom == nsXBLAtoms::onget) {
      onget = aAtts[1];
    }
    else if (nameAtom == nsXBLAtoms::onset) {
      onset = aAtts[1];
    }
  }

  // All of our pointers are now filled in.  Construct our property with all of these
  // parameters.
  mProperty = new nsXBLProtoImplProperty(name, onget, onset, readonly);
  if (mProperty) {
    // Add this member to our chain.
    if (mImplMember)
      mImplMember->SetNext(mProperty); // Already have a chain. Just append to the end.
    else
      mImplementation->SetMemberList(mProperty); // We're the first member in the chain.

    mImplMember = mProperty; // Adjust our pointer to point to the new last member in the chain.
  }
}

void
nsXBLContentSink::ConstructMethod(const PRUnichar **aAtts)
{
  mMethod = nsnull;

  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;

  for(; *aAtts; aAtts += 2) {
    // Get upper-cased key
    SplitXMLName(nsDependentString(aAtts[0]), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (nameAtom == nsXBLAtoms::name) {
      mMethod = new nsXBLProtoImplMethod(aAtts[1]);
      break;
    }
  }

  if (mMethod) {
    // Add this member to our chain.
    if (mImplMember)
      mImplMember->SetNext(mMethod); // Already have a chain. Just append to the end.
    else
      mImplementation->SetMemberList(mMethod); // We're the first member in the chain.

    mImplMember = mMethod; // Adjust our pointer to point to the new last member in the chain.
  }
}

void
nsXBLContentSink::ConstructParameter(const PRUnichar **aAtts)
{
  if (!mMethod)
    return;

  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;

  for (; *aAtts; aAtts += 2) {
    // Get upper-cased key
    SplitXMLName(nsDependentString(aAtts[0]), getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (nameAtom == nsXBLAtoms::name) {
      mMethod->AddParameter(nsDependentString(aAtts[1]));
      break;
    }
  }
}

nsresult
nsXBLContentSink::CreateElement(const PRUnichar** aAtts, 
                                PRUint32 aAttsCount, 
                                PRInt32 aNameSpaceID, 
                                nsINodeInfo* aNodeInfo, 
                                nsIContent** aResult)
{
  if (aNameSpaceID == kNameSpaceID_XUL) {
    nsXULPrototypeElement* prototype = new nsXULPrototypeElement();
    if (!prototype)
      return NS_ERROR_OUT_OF_MEMORY;
    
    prototype->mNodeInfo = aNodeInfo;

    // Reset the refcnt to 0.  Normally XUL prototype elements get a refcnt of 1
    // to represent ownership by the XUL prototype document.  In our case we have
    // no prototype document, and our initial ref count of 1 will come from being
    // wrapped by a real XUL element in the Create call below.
    prototype->mRefCnt = 0;

    AddAttributesToXULPrototype(aAtts, aAttsCount, prototype);

    // Following this function call, the prototype's ref count will be 1.
    nsresult rv = nsXULElement::Create(prototype, mDocument, PR_FALSE, aResult);

    if (NS_FAILED(rv)) return rv;
    return NS_OK;
  }
  else
    return nsXMLContentSink::CreateElement(aAtts, aAttsCount, aNameSpaceID, aNodeInfo, aResult);
}

nsresult 
nsXBLContentSink::AddAttributes(const PRUnichar** aAtts,
                                nsIContent* aContent,
                                PRBool aIsHTML)
{
  if (aContent->IsContentOfType(nsIContent::eXUL))
    return NS_OK; // Nothing to do, since the proto already has the attrs.
  else 
    return nsXMLContentSink::AddAttributes(aAtts, aContent, aIsHTML);
}

nsresult
nsXBLContentSink::AddAttributesToXULPrototype(const PRUnichar **aAtts, 
                                              PRUint32 aAttsCount, 
                                              nsXULPrototypeElement* aElement)
{
  // Add tag attributes to the element
  nsresult rv;

  // Create storage for the attributes
  nsXULPrototypeAttribute* attrs = nsnull;
  if (aAttsCount > 0) {
    attrs = new nsXULPrototypeAttribute[aAttsCount];
    if (!attrs)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  aElement->mAttributes    = attrs;
  aElement->mNumAttributes = aAttsCount;

  // Copy the attributes into the prototype
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  
  for (; *aAtts; aAtts += 2) {
    const nsDependentString key(aAtts[0]);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    PRInt32 nameSpaceID;

    if (nameSpacePrefix)
        nameSpaceID = GetNameSpaceId(nameSpacePrefix);
    else {
      if (nameAtom == nsLayoutAtoms::xmlnsNameSpace)
        nameSpaceID = kNameSpaceID_XMLNS;
      else
        nameSpaceID = kNameSpaceID_None;
    }

    if (kNameSpaceID_Unknown == nameSpaceID) {
      nameSpaceID = kNameSpaceID_None;
      nameAtom = dont_AddRef(NS_NewAtom(key));
      nameSpacePrefix = nsnull;
    } 

    mNodeInfoManager->GetNodeInfo(nameAtom, nameSpacePrefix, nameSpaceID,
                                  *getter_AddRefs(attrs->mNodeInfo));
    
    attrs->mValue.SetValue(nsDependentString(aAtts[1]));
    ++attrs;
  }

  // XUL elements may require some additional work to compute
  // derived information.
  if (aElement->mNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
    nsAutoString value;

    // Compute the element's class list if the element has a 'class' attribute.
    rv = aElement->GetAttr(kNameSpaceID_None, nsXULAtoms::clazz, value);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      rv = nsClassList::ParseClasses(&aElement->mClassList, value);
      if (NS_FAILED(rv)) return rv;
    }

    // Parse the element's 'style' attribute
    rv = aElement->GetAttr(kNameSpaceID_None, nsHTMLAtoms::style, value);
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_CONTENT_ATTR_HAS_VALUE) {
      if (!mCSSParser) {
          rv = nsComponentManager::CreateInstance(kCSSParserCID,
                                                  nsnull,
                                                  NS_GET_IID(nsICSSParser),
                                                  getter_AddRefs(mCSSParser));

          if (NS_FAILED(rv)) return rv;
      }

      rv = mCSSParser->ParseStyleAttribute(value, mDocumentURL,
                             getter_AddRefs(aElement->mInlineStyleRule));

      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to parse style rule");
      if (NS_FAILED(rv)) return rv;
    }
  }

  return NS_OK;
}
