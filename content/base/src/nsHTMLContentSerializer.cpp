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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsHTMLContentSerializer.h"

#include "nsIDOMElement.h"
#include "nsIContent.h"
#include "nsString.h"
#include "nsParserCIID.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kParserServiceCID, NS_PARSERSERVICE_CID);

nsHTMLContentSerializer::nsHTMLContentSerializer()
{
  mParserService = do_GetService(kParserServiceCID);
}

nsHTMLContentSerializer::~nsHTMLContentSerializer()
{
}

void
nsHTMLContentSerializer::ReplaceCharacterEntities(nsAWritableString& aStr,
                                                 PRUint32 aOffset)
{
  if (!mParserService) return;

  nsReadingIterator<PRUnichar> done_reading;
  aStr.EndReading(done_reading);

  // for each chunk of |aString|...
  PRUint32 fragmentLength = 0;
  PRUint32 offset = aOffset;
  nsReadingIterator<PRUnichar> iter;
  aStr.BeginReading(iter);

  nsCAutoString entityStr;

  for (iter.advance(PRInt32(offset)); 
       iter != done_reading; 
       iter.advance(PRInt32(fragmentLength))) {
    fragmentLength = iter.size_forward();
    const PRUnichar* c = iter.get();
    const PRUnichar* fragmentEnd = c + fragmentLength;
    
    // for each character in this chunk...
    for (; c < fragmentEnd; c++, offset++) {
      PRUnichar val = *c;
      mParserService->HTMLConvertUnicodeToEntity(PRInt32(val), entityStr);
      if (entityStr.Length()) {
        aStr.Cut(offset, 1);
        aStr.Insert(NS_ConvertASCIItoUCS2(entityStr.GetBuffer()), offset);
        aStr.EndReading(done_reading);
        aStr.BeginReading(iter);
        fragmentLength = offset + entityStr.Length();
        break;
      }
    }
  }
}

NS_IMETHODIMP
nsHTMLContentSerializer::AppendElementStart(nsIDOMElement *aElement,
					    nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> name;
  content->GetTag(*getter_AddRefs(name));

  aStr.Append(NS_LITERAL_STRING("<"));
  // The string is shared
  const PRUnichar* sharedName;
  name->GetUnicode(&sharedName);
  aStr.Append(sharedName);

  PRInt32 index, count;
  nsAutoString nameStr, valueStr;
  PRInt32 namespaceID;
  nsCOMPtr<nsIAtom> attrName, attrPrefix;

  for (index = 0; index < count; index++) {
    content->GetAttributeNameAt(index, 
                                namespaceID,
                                *getter_AddRefs(attrName),
                                *getter_AddRefs(attrPrefix));

    content->GetAttribute(namespaceID, attrName, valueStr);
    attrName->ToString(nameStr);
    
    SerializeAttr(nsAutoString(), nameStr, valueStr, aStr);
  }

  aStr.Append(NS_LITERAL_STRING(">"));    
  
  return NS_OK;
}
  
NS_IMETHODIMP 
nsHTMLContentSerializer::AppendElementEnd(nsIDOMElement *aElement,
					  nsAWritableString& aStr)
{
  NS_ENSURE_ARG(aElement);

  nsCOMPtr<nsIContent> content = do_QueryInterface(aElement);
  if (!content) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> name;
  content->GetTag(*getter_AddRefs(name));

  // The string is shared
  const PRUnichar* sharedName;
  name->GetUnicode(&sharedName);

  if (mParserService) {
    nsAutoString nameStr(sharedName);
    PRBool isContainer;

    mParserService->IsContainer(nameStr, isContainer);
    if (!isContainer) return NS_OK;
  }

  aStr.Append(NS_LITERAL_STRING("</"));
  aStr.Append(sharedName);
  aStr.Append(NS_LITERAL_STRING(">"));

  return NS_OK;
}
