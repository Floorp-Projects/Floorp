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

static NS_DEFINE_CID(kCSSParserCID,              NS_CSSPARSER_CID);

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
  rv = nsXMLContentSink::Init(aDoc, aURL, aContainer);
  return rv;
}

PRBool 
nsXBLContentSink::OnOpenContainer(const nsIParserNode& aNode, PRInt32 aNameSpaceID, nsIAtom* aTagName)
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
      ConstructHandler(aNode);
      ret = PR_FALSE;
    }
    else if (aTagName == nsXBLAtoms::resources) {
      mState = eXBL_InResources;
      ret = PR_FALSE; // The XML content sink should ignore all <resources>.
    }
    else if (mState == eXBL_InResources) {
      if (aTagName == nsXBLAtoms::stylesheet || aTagName == nsXBLAtoms::image)
        ConstructResource(aNode, aTagName);
      ret = PR_FALSE; // The XML content sink should ignore everything within a <resources> block.
    }
    else if (aTagName == nsXBLAtoms::implementation) {
      mState = eXBL_InImplementation;
      ConstructImplementation(aNode);
      ret = PR_FALSE; // The XML content sink should ignore the <implementation>.
    }
    else if (mState == eXBL_InImplementation) {
      if (aTagName == nsXBLAtoms::constructor) {
        mSecondaryState = eXBL_InConstructor;
        nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
        NS_NewXBLPrototypeHandler(nsnull, nsnull, nsnull, nsnull,
                                  nsnull, nsnull, nsnull, nsnull, nsnull,
                                  getter_AddRefs(newHandler));
        newHandler->SetEventName(nsXBLAtoms::constructor);
        mBinding->SetConstructor(newHandler);
      }
      else if (aTagName == nsXBLAtoms::destructor) {
        mSecondaryState = eXBL_InDestructor;
        nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
        NS_NewXBLPrototypeHandler(nsnull, nsnull, nsnull, nsnull,
                                  nsnull, nsnull, nsnull, nsnull, nsnull,
                                  getter_AddRefs(newHandler));
        newHandler->SetEventName(nsXBLAtoms::destructor);
        mBinding->SetDestructor(newHandler);
      }
      else if (aTagName == nsXBLAtoms::field) {
        mSecondaryState = eXBL_InField;
        ConstructField(aNode);
      }
      else if (aTagName == nsXBLAtoms::property) {
        mSecondaryState = eXBL_InProperty;
        ConstructProperty(aNode);
      }
      else if (aTagName == nsXBLAtoms::getter)
        mSecondaryState = eXBL_InGetter;
      else if (aTagName == nsXBLAtoms::setter)
        mSecondaryState = eXBL_InSetter;
      else if (aTagName == nsXBLAtoms::method) {
        mSecondaryState = eXBL_InMethod;
        ConstructMethod(aNode);
      }
      else if (aTagName == nsXBLAtoms::parameter)
        ConstructParameter(aNode);
      else if (aTagName == nsXBLAtoms::body)
        mSecondaryState = eXBL_InBody;

      ret = PR_FALSE; // Ignore everything we encounter inside an <implementation> block.
    }
  }

  return ret;
}

NS_IMETHODIMP
nsXBLContentSink::OpenContainer(const nsIParserNode& aNode)
{
  nsresult rv = nsXMLContentSink::OpenContainer(aNode);
  if (NS_FAILED(rv))
    return rv;

  if (mState == eXBL_InBinding && !mBinding)
    ConstructBinding();
  
  return rv;
}

NS_IMETHODIMP
nsXBLContentSink::CloseContainer(const nsIParserNode& aNode)
{
  FlushText();

  if (mState != eXBL_InDocument) {
    nsCOMPtr<nsIAtom> nameSpacePrefix, tagAtom;

    SplitXMLName(aNode.GetText(), getter_AddRefs(nameSpacePrefix),
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

      nsresult rv = nsXMLContentSink::CloseContainer(aNode);
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

  return nsXMLContentSink::CloseContainer(aNode);
}

NS_IMETHODIMP
nsXBLContentSink::AddCDATASection(const nsIParserNode& aNode)
{
  if (mState == eXBL_InHandlers || mState == eXBL_InImplementation)
    return AddText(aNode.GetText());
  return nsXMLContentSink::AddCDATASection(aNode);
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

nsresult
nsXBLContentSink::AddAttributesToXULPrototype(const nsIParserNode& aNode, nsXULPrototypeElement* aElement)
{
  // Add tag attributes to the element
  nsresult rv;
  PRInt32 count = aNode.GetAttributeCount();

  // Create storage for the attributes
  nsXULPrototypeAttribute* attrs = nsnull;
  if (count > 0) {
    attrs = new nsXULPrototypeAttribute[count];
    if (!attrs)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  aElement->mAttributes    = attrs;
  aElement->mNumAttributes = count;

  // Copy the attributes into the prototype
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  
  for (PRInt32 i = 0; i < count; i++) {
    const nsAReadableString& key = aNode.GetKeyAt(i);

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
    
    const nsAReadableString& valueStr = aNode.GetValueAt(i);
    attrs->mValue.SetValue(valueStr);
    ++attrs;
  }

  // XUL elements may require some additional work to compute
  // derived information.
  if (aElement->mNodeInfo->NamespaceEquals(nsXULAtoms::nameSpaceID)) {
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

nsresult
nsXBLContentSink::CreateElement(const nsIParserNode& aNode, PRInt32 aNameSpaceID, 
                                nsINodeInfo* aNodeInfo, nsIContent** aResult)
{
  if (aNameSpaceID == nsXULAtoms::nameSpaceID) {
    nsXULPrototypeElement* prototype = new nsXULPrototypeElement(0);
    if (!prototype)
      return NS_ERROR_OUT_OF_MEMORY;
    
    prototype->mNodeInfo = aNodeInfo;

    // Reset the refcnt to 0.  Normally XUL prototype elements get a refcnt of 1
    // to represent ownership by the XUL prototype document.  In our case we have
    // no prototype document, and our initial ref count of 1 will come from being
    // wrapped by a real XUL element in the Create call below.
    prototype->mRefCnt = 0;

    AddAttributesToXULPrototype(aNode, prototype);

    // Following this function call, the prototype's ref count will be 1.
    nsresult rv = nsXULElement::Create(prototype, mDocument, PR_FALSE, aResult);

    if (NS_FAILED(rv)) return rv;
    return NS_OK;
  }
  else
    return nsXMLContentSink::CreateElement(aNode, aNameSpaceID, aNodeInfo, aResult);
}

nsresult 
nsXBLContentSink::AddAttributes(const nsIParserNode& aNode,
                                nsIContent* aContent,
                                PRBool aIsHTML)
{
  if (aContent->IsContentOfType(nsIContent::eXUL))
    return NS_OK; // Nothing to do, since the proto already has the attrs.
  else 
    return nsXMLContentSink::AddAttributes(aNode, aContent, aIsHTML);
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
nsXBLContentSink::ConstructHandler(const nsIParserNode& aNode)
{
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();

  nsAReadableString* event      = nsnull;
  nsAReadableString* modifiers  = nsnull;
  nsAReadableString* button     = nsnull;
  nsAReadableString* clickcount = nsnull;
  nsAReadableString* keycode    = nsnull;
  nsAReadableString* charcode   = nsnull;
  nsAReadableString* phase      = nsnull;
  nsAReadableString* command    = nsnull;
  nsAReadableString* action     = nsnull;

  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("event")))
      event = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("modifiers")))
      modifiers = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("button")))
      button = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("clickcount")))
      clickcount = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("keycode")))
      keycode = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("key")) || key.Equals(NS_LITERAL_STRING("charcode")))
      charcode = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("phase")))
      phase = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("command")))
      command = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("action")))
      action = &(aNode.GetValueAt(i));
    else
      continue; // Nope, it's some irrelevant attribute. Ignore it and move on.
  }

  if (command && !mIsChromeOrResource)
    // Make sure the XBL doc is chrome or resource if we have a command
    // shorthand syntax.
    return; // Don't even make this handler.

  // All of our pointers are now filled in.  Construct our handler with all of these
  // parameters.
  nsCOMPtr<nsIXBLPrototypeHandler> newHandler;
  NS_NewXBLPrototypeHandler(event, phase, action, command,
                            keycode, charcode, modifiers, button, clickcount,
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
nsXBLContentSink::ConstructResource(const nsIParserNode& aNode, nsIAtom* aResourceType)
{
  if (!mBinding)
    return;

  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("src"))) {
      mBinding->AddResource(aResourceType, aNode.GetValueAt(i));
      break;
    }
  }
}

void
nsXBLContentSink::ConstructImplementation(const nsIParserNode& aNode)
{
  mImplementation = nsnull;
  mImplMember = nsnull;
      
  if (!mBinding)
    return;

  nsAReadableString* name       = nsnull;
  
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("name")))
      name = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("implements")))
      mBinding->ConstructInterfaceTable(aNode.GetValueAt(i));
  }

  NS_NewXBLProtoImpl(mBinding, name, &mImplementation);
}

void
nsXBLContentSink::ConstructField(const nsIParserNode& aNode)
{
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();

  nsAReadableString* name       = nsnull;
  nsAReadableString* readonly   = nsnull;
  
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("name")))
      name = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("readonly")))
      readonly = &(aNode.GetValueAt(i));
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
nsXBLContentSink::ConstructProperty(const nsIParserNode& aNode)
{
  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();

  nsAReadableString* name       = nsnull;
  nsAReadableString* readonly   = nsnull;
  nsAReadableString* onget      = nsnull;
  nsAReadableString* onset      = nsnull;
  
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("name")))
      name = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("readonly")))
      readonly = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("onget")))
      onget = &(aNode.GetValueAt(i));
    else if (key.Equals(NS_LITERAL_STRING("onset")))
      onset = &(aNode.GetValueAt(i));
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
nsXBLContentSink::ConstructMethod(const nsIParserNode& aNode)
{
  mMethod = nsnull;

  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();

  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("name"))) {
      mMethod = new nsXBLProtoImplMethod(aNode.GetValueAt(i));
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
nsXBLContentSink::ConstructParameter(const nsIParserNode& aNode)
{
  if (!mMethod)
    return;

  nsCOMPtr<nsIAtom> nameSpacePrefix, nameAtom;
  PRInt32 ac = aNode.GetAttributeCount();
  for (PRInt32 i = 0; i < ac; i++) {
    // Get upper-cased key
    const nsAReadableString& key = aNode.GetKeyAt(i);

    SplitXMLName(key, getter_AddRefs(nameSpacePrefix),
                 getter_AddRefs(nameAtom));

    if (nameSpacePrefix || nameAtom == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    if (key.Equals(NS_LITERAL_STRING("name"))) {
      mMethod->AddParameter(aNode.GetValueAt(i));
      break;
    }
  }
}
