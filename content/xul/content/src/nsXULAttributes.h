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

#ifndef nsXULAttributes_h__
#define nsXULAttributes_h__

#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"

class nsIURL;

struct nsClassList {
  nsClassList(nsIAtom* aAtom)
    : mAtom(aAtom),
      mNext(nsnull)
  {
  }
  nsClassList(const nsClassList& aCopy)
    : mAtom(aCopy.mAtom),
      mNext(nsnull)
  {
    NS_ADDREF(mAtom);
    if (nsnull != aCopy.mNext) {
      mNext = new nsClassList(*(aCopy.mNext));
    }
  }
  ~nsClassList(void)
  {
    NS_RELEASE(mAtom);
    if (nsnull != mNext) {
      delete mNext;
    }
  }

  nsIAtom*      mAtom;
  nsClassList*  mNext;
};

struct nsXULAttribute
{
    nsXULAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, const nsString& aValue)
    {
      mNameSpaceID = aNameSpaceID;
      NS_IF_ADDREF(aName);
      mName = aName;
      mValue = aValue;
    }

    ~nsXULAttribute()
    {
      NS_IF_RELEASE(mName);
    }

    PRInt32 mNameSpaceID;
    nsIAtom* mName;
    nsString mValue;
};

class nsXULAttributes
{
public:
    nsXULAttributes()
        : mClassList(nsnull), mStyleRule(nsnull)
    {
        mAttributes = new nsVoidArray();
    }

    ~nsXULAttributes(void)
    {
        if (nsnull != mAttributes) {
          PRInt32 count = mAttributes->Count();
          PRInt32 index;
          for (index = 0; index < count; index++) {
              nsXULAttribute* attr = (nsXULAttribute*)mAttributes->ElementAt(index);
              delete attr;
          }
        }
        delete mAttributes;
    }

    // VoidArray Helpers
    PRInt32 Count() { return mAttributes->Count(); };
    nsXULAttribute* ElementAt(PRInt32 i) { return (nsXULAttribute*)mAttributes->ElementAt(i); };
    void AppendElement(nsXULAttribute* aElement) { mAttributes->AppendElement((void*)aElement); };
    void RemoveElementAt(PRInt32 index) { mAttributes->RemoveElementAt(index); };

    // Style Helpers
    nsresult GetClasses(nsVoidArray& aArray) const;
    nsresult HasClass(nsIAtom* aClass) const;

    nsresult UpdateClassList(const nsString& aValue);
    nsresult UpdateStyleRule(nsIURL* aDocURL, const nsString& aValue);
    nsresult GetInlineStyleRule(nsIStyleRule*& aRule);

public:
    nsClassList* mClassList;
    nsIStyleRule* mStyleRule;
    nsVoidArray* mAttributes;
};


#endif // nsXULAttributes_h__

