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

#include "nsIDOMCDATASection.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsIDocument.h"
#include "nsCRT.h"
#include "nsLayoutAtoms.h"
#include "nsIXMLContent.h"
#include "nsString.h"
#include "nsContentUtils.h"


class nsXMLCDATASection : public nsIDOMCDATASection,
                          public nsITextContent
{
public:
  nsXMLCDATASection();
  virtual ~nsXMLCDATASection();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMCharacterData
  NS_IMPL_NSIDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMText
  NS_IMPL_NSIDOMTEXT_USING_GENERIC_DOM_DATA(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    return mInner.SizeOf(aSizer, aResult, sizeof(*this));
  }
#endif

  // nsITextContent
  NS_IMPL_ITEXTCONTENT_USING_GENERIC_DOM_DATA(mInner)

protected:
  nsGenericDOMDataNode mInner;
  PRUint32 mContentID;
};

nsresult
NS_NewXMLCDATASection(nsIContent** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLCDATASection();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsXMLCDATASection::nsXMLCDATASection()
{
  NS_INIT_REFCNT();
  mContentID = 0;
}

nsXMLCDATASection::~nsXMLCDATASection()
{
}


// QueryInterface implementation for nsXMLCDATASection
NS_INTERFACE_MAP_BEGIN(nsXMLCDATASection)
  NS_INTERFACE_MAP_ENTRY_DOM_DATA()
  NS_INTERFACE_MAP_ENTRY(nsIDOMCDATASection)
  NS_INTERFACE_MAP_ENTRY(nsIDOMText)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCharacterData)
  NS_INTERFACE_MAP_ENTRY(nsITextContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CDATASection)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLCDATASection)
NS_IMPL_RELEASE(nsXMLCDATASection)


NS_IMETHODIMP 
nsXMLCDATASection::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::textTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP 
nsXMLCDATASection::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(NS_LITERAL_STRING("#cdata-section"));
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::CDATA_SECTION_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsresult result = NS_OK;
  nsXMLCDATASection* it;
  NS_NEWXPCOM(it, nsXMLCDATASection);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // XXX Increment the ref count before calling any
  // methods. If they do a QI and then a Release()
  // the instance will be deleted.
  result = it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
  if (NS_FAILED(result)) {
    return result;
  }
  nsAutoString data;
  result = GetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  result = it->SetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  return result;
}

NS_IMETHODIMP 
nsXMLCDATASection::CloneContent(PRBool aCloneText, nsITextContent** aReturn)
{
  nsresult result = NS_OK;
  nsXMLCDATASection* it;
  NS_NEWXPCOM(it, nsXMLCDATASection);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  result = it->QueryInterface(NS_GET_IID(nsITextContent), (void**) aReturn);
  if (NS_FAILED(result) || !aCloneText) {
    return result;
  }
  nsAutoString data;
  result = GetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  result = it->SetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  return result;
}

#ifdef DEBUG
NS_IMETHODIMP
nsXMLCDATASection::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "CDATASection refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsXMLCDATASection::HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent,
                                  nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLCDATASection::GetContentID(PRUint32* aID)
{
  *aID = mContentID;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::SetContentID(PRUint32 aID) 
{
  mContentID = aID;
  return NS_OK;
}


NS_IMETHODIMP_(PRBool)
nsXMLCDATASection::IsContentOfType(PRUint32 aFlags)
{
  return PR_FALSE;
}
