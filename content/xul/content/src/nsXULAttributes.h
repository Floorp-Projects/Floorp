/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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


/*

  A set of helper classes used to implement attributes.

*/

#ifndef nsXULAttributes_h__
#define nsXULAttributes_h__

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"
#include "nsXULAttributeValue.h"

class nsIURI;
class nsINodeInfo;

//----------------------------------------------------------------------

class nsClassList {
public:
    nsClassList(nsIAtom* aAtom)
        : mAtom(getter_AddRefs(aAtom)), mNext(nsnull)
    {
        MOZ_COUNT_CTOR(nsClassList);
    }


    nsClassList(const nsClassList& aCopy)
        : mAtom(aCopy.mAtom), mNext(nsnull)
    {
        MOZ_COUNT_CTOR(nsClassList);
        if (aCopy.mNext) mNext = new nsClassList(*(aCopy.mNext));
    }

    nsClassList& operator=(const nsClassList& aClassList)
    {
        if (this != &aClassList) {
            delete mNext;
            mNext = nsnull;
            mAtom = aClassList.mAtom;

            if (aClassList.mNext) {
                mNext = new nsClassList(*(aClassList.mNext));
            }
        }
        return *this;
    }

    ~nsClassList(void)
    {
        MOZ_COUNT_DTOR(nsClassList);
        delete mNext;
    }

    nsCOMPtr<nsIAtom> mAtom;
    nsClassList*      mNext;

    static PRBool
    HasClass(nsClassList* aList, nsIAtom* aClass);

    static nsresult
    GetClasses(nsClassList* aList, nsVoidArray& aArray);

    static nsresult
    ParseClasses(nsClassList** aList, const nsAReadableString& aValue);
};

////////////////////////////////////////////////////////////////////////

class nsXULAttribute : public nsIDOMAttr,
                       public nsIDOM3Node
{
protected:
    static PRInt32 gRefCnt;
    static nsIAtom* kIdAtom;

    // A global fixed-size allocator for nsXULAttribute
    // objects. |kBlockSize| is the number of nsXULAttribute objects
    // the live together in a contiguous block of memory.
    static const PRInt32 kBlockSize;

    // The head of the free list for free nsXULAttribute objects.
    static nsXULAttribute* gFreeList;

    static void* operator new(size_t aSize);
    static void  operator delete(void* aObject, size_t aSize);

    nsXULAttribute(nsIContent* aContent,
                   nsINodeInfo* aNodeInfo,
                   const nsAReadableString& aValue);

    virtual ~nsXULAttribute();

public:
    static nsresult
    Create(nsIContent* aContent,
           nsINodeInfo* aNodeInfo,
           const nsAReadableString& aValue,
           nsXULAttribute** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMNode interface
    NS_DECL_NSIDOMNODE

    // nsIDOM3Node interface
    NS_DECL_NSIDOM3NODE

    // nsIDOMAttr interface
    NS_DECL_NSIDOMATTR

    // Implementation methods
    void GetQualifiedName(nsAWritableString& aAttributeName);

    nsINodeInfo* GetNodeInfo() const { return mNodeInfo; }
    nsresult     SetValueInternal(const nsAReadableString& aValue);
    nsresult     GetValueAsAtom(nsIAtom** aResult);

protected:
    union {
        nsIContent* mContent;   // The content object that owns the attribute
        nsXULAttribute* mNext;  // For objects on the freelist
    };

    nsINodeInfo* mNodeInfo;     // The attribute name
    nsXULAttributeValue        mValue;        // The attribute value; either an nsIAtom* or PRUnichar*,
                                // with the low-order bit tagging its type
};


////////////////////////////////////////////////////////////////////////

class nsXULAttributes : public nsIDOMNamedNodeMap
{
public:
    static nsresult
    Create(nsIContent* aElement, nsXULAttributes** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMNamedNodeMap interface
    NS_DECL_NSIDOMNAMEDNODEMAP

    // Implementation methods
    // VoidArray Helpers
    PRInt32 Count() { return mAttributes.Count(); };
    nsXULAttribute* ElementAt(PRInt32 i) { return (nsXULAttribute*)mAttributes.ElementAt(i); };
    void AppendElement(nsXULAttribute* aElement) { mAttributes.AppendElement((void*)aElement); };
    void RemoveElementAt(PRInt32 aIndex) { mAttributes.RemoveElementAt(aIndex); };

    // Style Helpers
    nsresult GetClasses(nsVoidArray& aArray) const;
    nsresult HasClass(nsIAtom* aClass) const;

    nsresult SetClassList(nsClassList* aClassList);
    nsresult UpdateClassList(const nsAReadableString& aValue);
    nsresult UpdateStyleRule(nsIURI* aDocURL, const nsAReadableString& aValue);

    nsresult SetInlineStyleRule(nsIStyleRule* aRule);
    nsresult GetInlineStyleRule(nsIStyleRule*& aRule);

protected:
    nsXULAttributes(nsIContent* aContent);
    virtual ~nsXULAttributes();

    nsIContent*            mContent;
    nsClassList*           mClassList;
    nsCOMPtr<nsIStyleRule> mStyleRule;
    nsAutoVoidArray        mAttributes;
};


#endif // nsXULAttributes_h__

