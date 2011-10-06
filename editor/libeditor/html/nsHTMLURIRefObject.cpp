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

/* Here is the list, from beppe and glazman:
    href >> A, AREA, BASE, LINK
    src >> FRAME, IFRAME, IMG, INPUT, SCRIPT
    <META http-equiv="refresh" content="3,http://www.acme.com/intro.html">
    longdesc >> FRAME, IFRAME, IMG
    usemap >> IMG, INPUT, OBJECT
    action >> FORM
    background >> BODY
    codebase >> OBJECT, APPLET
    classid >> OBJECT
    data >> OBJECT
    cite >> BLOCKQUOTE, DEL, INS, Q
    profile >> HEAD
    ARCHIVE attribute on APPLET ; warning, it contains a list of URIs.

    Easier way of organizing the list:
    a:      href
    area:   href
    base:   href
    body:   background
    blockquote: cite (not normally rewritable)
    link:   href
    frame:  src, longdesc
    iframe: src, longdesc
    input:  src, usemap
    form:   action
    img:    src, longdesc, usemap
    script: src
    applet: codebase, archive <list>
    object: codebase, data, classid, usemap
    head:   profile
    del:    cite
    ins:    cite
    q:      cite
 */

/* Here is how to open a channel for testing
   (from embed/qa/testembed/Tests.cpp):

  nsCOMPtr<nsIChannel> theChannel;
  nsCString uri;
  nsCOMPtr<nsIURI> theURI;
  rv = NS_NewURI(getter_AddRefs(theURI), theSpec);
  if (!theURI)
    error;
  rv = NS_OpenURI(getter_AddRefs(theChannel), theURI, nsnull, theLoadGroup);
  if (!theChannel)
    error;
  nsCOMPtr<nsILoadGroup> theLoadGroup(do_CreateInstance(NS_LOADGROUP_CONTRACTID));
  if (!theLoadGroup)
    error;
		nsCOMPtr<nsIStreamListener> listener(static_cast<nsIStreamListener*>(qaBrowserImpl));
		//nsCOMPtr<nsIWeakReference> thisListener(do_GetWeakReference(listener));
		//qaWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsIStreamListener));

		// this calls nsIStreamListener::OnDataAvailable()
		rv = theChannel->AsyncOpen(listener, nsnull);

		nsCOMPtr<nsIRequest> theRequest = do_QueryInterface(theChannel);
    // Now we can do things on nsIRequest (like what?)
 */

#include "nsHTMLURIRefObject.h"

#include "nsAString.h"
#include "nsString.h"
#include "nsIDOMAttr.h"
#include "nsIDOMElement.h"

// String classes change too often and I can't keep up.
// Set this macro to this week's approved case-insensitive compare routine.
#define MATCHES(tagName, str) tagName.EqualsIgnoreCase(str)

nsHTMLURIRefObject::nsHTMLURIRefObject()
{
  mCurAttrIndex = mAttributeCnt = 0;
}

nsHTMLURIRefObject::~nsHTMLURIRefObject()
{
}

//Interfaces for addref and release and queryinterface
NS_IMPL_ISUPPORTS1(nsHTMLURIRefObject, nsIURIRefObject)

NS_IMETHODIMP
nsHTMLURIRefObject::Reset()
{
  mCurAttrIndex = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLURIRefObject::GetNextURI(nsAString & aURI)
{
  NS_ENSURE_TRUE(mNode, NS_ERROR_NOT_INITIALIZED);

  nsAutoString tagName;
  nsresult rv = mNode->GetNodeName(tagName);
  NS_ENSURE_SUCCESS(rv, rv);

  // Loop over attribute list:
  if (!mAttributes)
  {
    nsCOMPtr<nsIDOMElement> element (do_QueryInterface(mNode));
    NS_ENSURE_TRUE(element, NS_ERROR_INVALID_ARG);

    mCurAttrIndex = 0;
    mNode->GetAttributes(getter_AddRefs(mAttributes));
    NS_ENSURE_TRUE(mAttributes, NS_ERROR_NOT_INITIALIZED);

    rv = mAttributes->GetLength(&mAttributeCnt);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(mAttributeCnt, NS_ERROR_FAILURE);
    mCurAttrIndex = 0;
  }
#ifdef DEBUG_akkana
  printf("Looking at tag '%s'\n",
         NS_LossyConvertUTF16toASCII(tagName).get());
#endif
  while (mCurAttrIndex < mAttributeCnt)
  {
    nsCOMPtr<nsIDOMNode> attrNode;
    rv = mAttributes->Item(mCurAttrIndex++, getter_AddRefs(attrNode));
      // XXX Does Item() addref, or not?
      // The comparable code in nsEditor assumes it doesn't.
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_ARG_POINTER(attrNode);
    nsCOMPtr<nsIDOMAttr> curAttrNode (do_QueryInterface(attrNode));
    NS_ENSURE_ARG_POINTER(curAttrNode);
    nsString curAttr;
    rv = curAttrNode->GetName(curAttr);
    NS_ENSURE_SUCCESS(rv, rv);

    // href >> A, AREA, BASE, LINK
#ifdef DEBUG_akkana
    printf("Trying to match attribute '%s'\n",
           NS_LossyConvertUTF16toASCII(curAttr).get());
#endif
    if (MATCHES(curAttr, "href"))
    {
      if (!MATCHES(tagName, "a") && !MATCHES(tagName, "area")
          && !MATCHES(tagName, "base") && !MATCHES(tagName, "link"))
        continue;
      rv = curAttrNode->GetValue(aURI);
      NS_ENSURE_SUCCESS(rv, rv);
      nsString uri (aURI);
      // href pointing to a named anchor doesn't count
      if (aURI.First() != PRUnichar('#'))
        return NS_OK;
      aURI.Truncate();
      return NS_ERROR_INVALID_ARG;
    }
    // src >> FRAME, IFRAME, IMG, INPUT, SCRIPT
    else if (MATCHES(curAttr, "src"))
    {
      if (!MATCHES(tagName, "img")
          && !MATCHES(tagName, "frame") && !MATCHES(tagName, "iframe")
          && !MATCHES(tagName, "input") && !MATCHES(tagName, "script"))
        continue;
      return curAttrNode->GetValue(aURI);
    }
    //<META http-equiv="refresh" content="3,http://www.acme.com/intro.html">
    else if (MATCHES(curAttr, "content"))
    {
      if (!MATCHES(tagName, "meta"))
        continue;
    }
    // longdesc >> FRAME, IFRAME, IMG
    else if (MATCHES(curAttr, "longdesc"))
    {
      if (!MATCHES(tagName, "img")
          && !MATCHES(tagName, "frame") && !MATCHES(tagName, "iframe"))
        continue;
    }
    // usemap >> IMG, INPUT, OBJECT
    else if (MATCHES(curAttr, "usemap"))
    {
      if (!MATCHES(tagName, "img")
          && !MATCHES(tagName, "input") && !MATCHES(tagName, "object"))
        continue;
    }
    // action >> FORM
    else if (MATCHES(curAttr, "action"))
    {
      if (!MATCHES(tagName, "form"))
        continue;
    }
    // background >> BODY
    else if (MATCHES(curAttr, "background"))
    {
      if (!MATCHES(tagName, "body"))
        continue;
    }
    // codebase >> OBJECT, APPLET
    else if (MATCHES(curAttr, "codebase"))
    {
      if (!MATCHES(tagName, "meta"))
        continue;
    }
    // classid >> OBJECT
    else if (MATCHES(curAttr, "classid"))
    {
      if (!MATCHES(tagName, "object"))
        continue;
    }
    // data >> OBJECT
    else if (MATCHES(curAttr, "data"))
    {
      if (!MATCHES(tagName, "object"))
        continue;
    }
    // cite >> BLOCKQUOTE, DEL, INS, Q
    else if (MATCHES(curAttr, "cite"))
    {
      if (!MATCHES(tagName, "blockquote") && !MATCHES(tagName, "q")
          && !MATCHES(tagName, "del") && !MATCHES(tagName, "ins"))
        continue;
    }
    // profile >> HEAD
    else if (MATCHES(curAttr, "profile"))
    {
      if (!MATCHES(tagName, "head"))
        continue;
    }
    // archive attribute on APPLET; warning, it contains a list of URIs.
    else if (MATCHES(curAttr, "archive"))
    {
      if (!MATCHES(tagName, "applet"))
        continue;
    }
  }
  // Return a code to indicate that there are no more,
  // to distinguish that case from real errors.
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsHTMLURIRefObject::RewriteAllURIs(const nsAString & aOldPat,
                            const nsAString & aNewPat,
                            bool aMakeRel)
{
#ifdef DEBUG_akkana
  printf("Can't rewrite URIs yet\n");
#endif
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTMLURIRefObject::GetNode(nsIDOMNode** aNode)
{
  NS_ENSURE_TRUE(mNode, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_TRUE(aNode, NS_ERROR_NULL_POINTER);
  *aNode = mNode.get();
  NS_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLURIRefObject::SetNode(nsIDOMNode *aNode)
{
  mNode = aNode;
  nsAutoString dummyURI;
  if (NS_SUCCEEDED(GetNextURI(dummyURI)))
  {
    mCurAttrIndex = 0;    // Reset so we'll get the first node next time
    return NS_OK;
  }

  // If there weren't any URIs in the attributes,
  // then don't accept this node.
  mNode = 0;
  return NS_ERROR_INVALID_ARG;
}

nsresult NS_NewHTMLURIRefObject(nsIURIRefObject** aResult, nsIDOMNode* aNode)
{
  nsHTMLURIRefObject* refObject = new nsHTMLURIRefObject();
  NS_ENSURE_TRUE(refObject, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = refObject->SetNode(aNode);
  if (NS_FAILED(rv)) {
    *aResult = 0;
    delete refObject;
    return rv;
  }
  return refObject->QueryInterface(NS_GET_IID(nsIURIRefObject),
                                   (void**)aResult);
}

