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
 */


#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIStyleSheet.h"
#include "nsIURI.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIXMLContent.h"

#include "nsNetUtil.h"


class nsXMLProcessingInstruction : public nsIDOMProcessingInstruction,
                                   public nsIDOMLinkStyle,
                                   public nsIContent
{
public:
  nsXMLProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData);
  virtual ~nsXMLProcessingInstruction();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMProcessingInstruction
  NS_DECL_NSIDOMPROCESSINGINSTRUCTION

  // nsIDOMLinkStyle
  NS_DECL_NSIDOMLINKSTYLE

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  PRBool GetAttrValue(const char *aAttr, nsString& aValue);

  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mTarget;
};

nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               const nsAReadableString& aTarget,
                               const nsAReadableString& aData)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLProcessingInstruction(aTarget, aData);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsXMLProcessingInstruction::nsXMLProcessingInstruction(const nsAReadableString& aTarget,
                                                       const nsAReadableString& aData) :
  mTarget(aTarget)
{
  NS_INIT_REFCNT();
  mInner.SetData(this, aData);
}

nsXMLProcessingInstruction::~nsXMLProcessingInstruction()
{
}


// XPConnect interface list for nsXMLProcessingInstruction
NS_CLASSINFO_MAP_BEGIN(ProcessingInstruction)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMProcessingInstruction)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMLinkStyle)
  NS_CLASSINFO_MAP_ENTRY(nsIDOMEventTarget)
NS_CLASSINFO_MAP_END


// QueryInterface implementation for nsXMLProcessingInstruction
NS_INTERFACE_MAP_BEGIN(nsXMLProcessingInstruction)
  NS_INTERFACE_MAP_ENTRY_DOM_DATA()
  NS_INTERFACE_MAP_ENTRY(nsIDOMProcessingInstruction)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ProcessingInstruction)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLProcessingInstruction)
NS_IMPL_RELEASE(nsXMLProcessingInstruction)


NS_IMETHODIMP
nsXMLProcessingInstruction::GetTarget(nsAWritableString& aTarget)
{
  aTarget.Assign(mTarget);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetData(nsAWritableString& aData)
{
  return mInner.GetData(aData);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::SetData(const nsAReadableString& aData)
{
  // XXX Check if this is a stylesheet PI. If so, we may need
  // to parse the contents and see if anything has changed.
  return mInner.SetData(this, aData);
}

PRBool
nsXMLProcessingInstruction::GetAttrValue(const char *aAttr, nsString& aValue)
{
  nsAutoString data;

  mInner.GetData(data);

  while (1) {
    aValue.Truncate();

    PRInt32 pos = data.Find(aAttr);

    if (pos < 0)
      return PR_FALSE;

    // Cut off data up to the end of the attr string
    data.Cut(0, pos + nsCRT::strlen(aAttr));
    data.CompressWhitespace();

    if (data.First() != '=')
      continue;

    // Cut off the '='
    data.Cut(0, 1);
    data.CompressWhitespace();

    PRUnichar q = data.First();

    if (q != '"' && q != '\'')
      continue;

    // Cut off the first quote character
    data.Cut(0, 1);

    pos = data.FindChar(q);

    if (pos < 0)
      return PR_FALSE;

    data.Left(aValue, pos);

    return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetSheet(nsIDOMStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);
  *aSheet = nsnull;

  if (!mInner.mDocument)
    return NS_OK;

  nsAutoString data;
  GetData(data);

  if (!mTarget.EqualsWithConversion("xml-stylesheet")) {
    return NS_OK;
  }

  if (!GetAttrValue("href", data))
    return NS_OK;

  nsCOMPtr<nsIURI> baseURI;

  mInner.mDocument->GetBaseURL(*getter_AddRefs(baseURI));

  nsString href;

  NS_MakeAbsoluteURI(href, data, baseURI);

  PRInt32 i, count;

  count = mInner.mDocument->GetNumberOfStyleSheets();

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsIStyleSheet> sheet;
    sheet = dont_AddRef(mInner.mDocument->GetStyleSheetAt(i));

    if (!sheet)
      continue;

    nsCOMPtr<nsIURI> url;

    sheet->GetURL(*getter_AddRefs(url));

    if (!url)
      continue;

    nsXPIDLCString urlspec;

    url->GetSpec(getter_Copies(urlspec));

    if (href.EqualsWithConversion(urlspec)) {
      return sheet->QueryInterface(NS_GET_IID(nsIDOMStyleSheet),
                                   (void **)aSheet);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::processingInstructionTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::PROCESSING_INSTRUCTION_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsString data;
  mInner.GetData(data);

  nsXMLProcessingInstruction* it = new nsXMLProcessingInstruction(mTarget,
                                                                  data);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  tmp.Insert(mTarget.GetUnicode(), 0);
  fputs(tmp, out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::HandleDOMEvent(nsIPresContext* aPresContext,
                                           nsEvent* aEvent,
                                           nsIDOMEvent** aDOMEvent,
                                           PRUint32 aFlags,
                                           nsEventStatus* aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for processing instruction");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetContentID(PRUint32* aID)
{
  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::SetContentID(PRUint32 aID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXMLProcessingInstruction::SizeOf(nsISizeOfHandler* aSizer,
                                   PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  PRUint32 ssize;
  mTarget.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mTarget) + ssize;
#endif
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsXMLProcessingInstruction::IsContentOfType(PRUint32 aFlags)
{
  return PR_FALSE;
}
