/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*

  A helper class used to implement attributes.

*/

#include "nsXULAttributes.h"
#include "nsICSSParser.h"
#include "nsIURL.h"

const PRUnichar kNullCh = PRUnichar('\0');

static void ParseClasses(const nsString& aClassString, nsClassList** aClassList)
{
  NS_ASSERTION(nsnull == *aClassList, "non null start list");

  nsAutoString  classStr(aClassString);  // copy to work buffer
  classStr.Append(kNullCh);  // put an extra null at the end

  PRUnichar* start = (PRUnichar*)(const PRUnichar*)classStr;
  PRUnichar* end   = start;

  while (kNullCh != *start) {
    while ((kNullCh != *start) && nsString::IsSpace(*start)) {  // skip leading space
      start++;
    }
    end = start;

    while ((kNullCh != *end) && (PR_FALSE == nsString::IsSpace(*end))) { // look for space or end
      end++;
    }
    *end = kNullCh; // end string here

    if (start < end) {
      *aClassList = new nsClassList(NS_NewAtom(start));
      aClassList = &((*aClassList)->mNext);
    }

    start = ++end;
  }
}

nsresult 
nsXULAttributes::GetClasses(nsVoidArray& aArray) const
{
  aArray.Clear();
  const nsClassList* classList = mClassList;
  while (nsnull != classList) {
    aArray.AppendElement(classList->mAtom); // NOTE atom is not addrefed
    classList = classList->mNext;
  }
  return NS_OK;
}

nsresult 
nsXULAttributes::HasClass(nsIAtom* aClass) const
{
  const nsClassList* classList = mClassList;
  while (nsnull != classList) {
    if (classList->mAtom == aClass) {
      return NS_OK;
    }
    classList = classList->mNext;
  }
  return NS_COMFALSE;
}

nsresult nsXULAttributes::UpdateClassList(const nsString& aValue)
{
  if (mClassList != nsnull)
  {
    delete mClassList;
    mClassList = nsnull;
  }

  if (aValue != "")
    ParseClasses(aValue, &mClassList);
  
  return NS_OK;
}

nsresult nsXULAttributes::UpdateStyleRule(nsIURL* aDocURL, const nsString& aValue)
{
    if (aValue == "")
    {
      // XXX: Removing the rule. Is this sufficient?
      NS_IF_RELEASE(mStyleRule);
      mStyleRule = nsnull;
      return NS_OK;
    }

    nsICSSParser* css;
    nsresult result = NS_NewCSSParser(&css);
    if (NS_OK != result) {
      return result;
    }

    nsIStyleRule* rule;
    result = css->ParseDeclarations(aValue, aDocURL, rule);
    //NS_IF_RELEASE(docURL);
    
    if ((NS_OK == result) && (nsnull != rule)) {
      mStyleRule = rule; //Addrefed already during parse, so don't need to addref again.
      //result = SetHTMLAttribute(aAttribute, nsHTMLValue(rule), aNotify);
    }
    //else {
    //  result = SetHTMLAttribute(aAttribute, nsHTMLValue(aValue), aNotify);
    //}
    NS_RELEASE(css);
    return NS_OK;
}

nsresult nsXULAttributes::GetInlineStyleRule(nsIStyleRule*& aRule)
{
  nsresult result = NS_ERROR_NULL_POINTER;
  if (mStyleRule != nsnull)
  {
    aRule = mStyleRule;
    NS_ADDREF(aRule);
    result = NS_OK;
  }

  return result;
}
