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
     // mState = eXBL_InResources;
     // ret = PR_FALSE; // The XML content sink should ignore all <resources>.
    }
    else if (aTagName == nsXBLAtoms::implementation) {
     // mState = eXBL_InImplementation;
     // ret = PR_FALSE; // The XML content sink should ignore the <implementation>.
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

      nsresult rv = nsXMLContentSink::CloseContainer(aNode);
      if (NS_FAILED(rv))
        return rv;

      if (mState == eXBL_InImplementation && tagAtom == nsXBLAtoms::implementation)
        mState = eXBL_InBinding;
      else if (mState == eXBL_InResources && tagAtom == nsXBLAtoms::resources)
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

nsresult
nsXBLContentSink::FlushText(PRBool aCreateTextNode,
                            PRBool* aDidFlush)
{
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
nsXBLContentSink::AddLeaf(const nsIParserNode& aNode)
{
  if (mState == eXBL_InHandlers) {
    if (mSecondaryState == eXBL_InHandler) {
      // Get the text and add it to the event handler.
      switch (aNode.GetTokenType()) {
        case eToken_text:
        case eToken_cdatasection:
          mHandler->SetHandlerText(aNode.GetText());
        case eToken_entity:
        {
          nsAutoString tmp;
          PRInt32 unicode = aNode.TranslateToUnicodeStr(tmp);
          if (unicode < 0)
            mHandler->SetHandlerText(aNode.GetText());
          else
            mHandler->SetHandlerText(aNode.GetText());
        }
      }
    }
    return NS_OK;
  }

  return nsXMLContentSink::AddLeaf(aNode);
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

    if (nameSpacePrefix || nameAtom.get() == nsLayoutAtoms::xmlnsNameSpace)
      continue;

    // Is this attribute one of the ones we care about?
    nsAReadableString* ref = nsnull;
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
