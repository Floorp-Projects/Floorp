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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 */

#include "nsCOMPtr.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIDOMEventReceiver.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContent.h"
#include "nsIXULContent.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsSupportsArray.h"
#include "nsINameSpace.h"
#include "nsXBLService.h"
#include "nsXBLPrototypeBinding.h"
#include "nsFixedSizeAllocator.h"

// Helper Classes =====================================================================

// nsIXBLAttributeEntry and helpers.  This class is used to efficiently handle
// attribute changes in anonymous content.

// {A2892B81-CED9-11d3-97FB-00400553EEF0}
#define NS_IXBLATTR_IID \
{ 0xa2892b81, 0xced9, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIXBLAttributeEntry : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLATTR_IID; return iid; }

  NS_IMETHOD GetSrcAttribute(nsIAtom** aResult) = 0;
  NS_IMETHOD GetDstAttribute(nsIAtom** aResult) = 0;
  NS_IMETHOD GetElement(nsIContent** aResult) = 0;
  NS_IMETHOD GetNext(nsIXBLAttributeEntry** aResult) = 0;
  NS_IMETHOD SetNext(nsIXBLAttributeEntry* aEntry) = 0;
};
  
class nsXBLAttributeEntry : public nsIXBLAttributeEntry {
public:
  NS_IMETHOD GetSrcAttribute(nsIAtom** aResult) { *aResult = mSrcAttribute; NS_IF_ADDREF(*aResult); return NS_OK; };
  NS_IMETHOD GetDstAttribute(nsIAtom** aResult) { *aResult = mDstAttribute; NS_IF_ADDREF(*aResult); return NS_OK; };
  
  NS_IMETHOD GetElement(nsIContent** aResult) { *aResult = mElement; NS_IF_ADDREF(*aResult); return NS_OK; };

  NS_IMETHOD GetNext(nsIXBLAttributeEntry** aResult) { NS_IF_ADDREF(*aResult = mNext); return NS_OK; }
  NS_IMETHOD SetNext(nsIXBLAttributeEntry* aEntry) { mNext = aEntry; return NS_OK; }

  nsIContent* mElement;

  nsCOMPtr<nsIAtom> mSrcAttribute;
  nsCOMPtr<nsIAtom> mDstAttribute;
  nsCOMPtr<nsIXBLAttributeEntry> mNext;

  static void* operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
    return aAllocator.Alloc(aSize);
  }

  static void operator delete(void* aPtr, size_t aSize) {
    nsFixedSizeAllocator::Free(aPtr, aSize);
  }

  nsXBLAttributeEntry(nsIAtom* aSrcAtom, nsIAtom* aDstAtom, nsIContent* aContent) {
    NS_INIT_REFCNT(); mSrcAttribute = aSrcAtom; mDstAttribute = aDstAtom; mElement = aContent;
  };

  virtual ~nsXBLAttributeEntry() {};

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS1(nsXBLAttributeEntry, nsIXBLAttributeEntry)

// =============================================================================

// Static initialization
PRUint32 nsXBLPrototypeBinding::gRefCnt = 0;
nsIAtom* nsXBLPrototypeBinding::kInheritStyleAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kHandlersAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kChildrenAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kIncludesAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kContentAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kInheritsAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kHTMLAtom = nsnull;
nsIAtom* nsXBLPrototypeBinding::kValueAtom = nsnull;

nsFixedSizeAllocator nsXBLPrototypeBinding::kPool;

static const size_t kBucketSizes[] = {
  sizeof(nsXBLAttributeEntry)
};

static const PRInt32 kNumBuckets = sizeof(kBucketSizes)/sizeof(size_t);
static const PRInt32 kNumElements = 128;
static const PRInt32 kInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLAttributeEntry))) * kNumElements;

// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLPrototypeBinding, nsIXBLPrototypeBinding)

// Constructors/Destructors
nsXBLPrototypeBinding::nsXBLPrototypeBinding(const nsCString& aID, nsIContent* aElement,
                                             nsIXBLDocumentInfo* aInfo)
: mID(aID), 
  mInheritStyle(PR_TRUE), 
  mHasBaseProto(PR_TRUE),
  mAttributeTable(nsnull), 
  mInsertionPointTable(nsnull)
{
  NS_INIT_REFCNT();
  
  mXBLDocInfoWeak = getter_AddRefs(NS_GetWeakReference(aInfo));
  
  gRefCnt++;
  //  printf("REF COUNT UP: %d %s\n", gRefCnt, (const char*)mID);

  if (gRefCnt == 1) {
    kPool.Init("XBL Attribute Entries", kBucketSizes, kNumBuckets, kInitialSize);

    kInheritStyleAtom = NS_NewAtom("inheritstyle");
    kHandlersAtom = NS_NewAtom("handlers");
    kChildrenAtom = NS_NewAtom("children");
    kContentAtom = NS_NewAtom("content");
    kIncludesAtom = NS_NewAtom("includes");
    kInheritsAtom = NS_NewAtom("inherits");
    kHTMLAtom = NS_NewAtom("html");
    kValueAtom = NS_NewAtom("value");
  }

  // These all use atoms, so we have to do these ops last to ensure
  // the atoms exist.
  SetBindingElement(aElement);

  PRBool allowScripts;
  aInfo->GetScriptAccess(&allowScripts);
  if (allowScripts) {
    ConstructHandlers();
  }

  nsCOMPtr<nsIContent> content;
  GetImmediateChild(kContentAtom, getter_AddRefs(content));
  if (content) {
    ConstructAttributeTable(content);
    ConstructInsertionTable(content);
  }
}


nsXBLPrototypeBinding::~nsXBLPrototypeBinding(void)
{
  delete mAttributeTable;
  delete mInsertionPointTable;
  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kInheritStyleAtom);
    NS_RELEASE(kHandlersAtom);
    NS_RELEASE(kChildrenAtom);
    NS_RELEASE(kContentAtom);
    NS_RELEASE(kIncludesAtom);
    NS_RELEASE(kInheritsAtom);
    NS_RELEASE(kHTMLAtom);
    NS_RELEASE(kValueAtom);
  }
}

// nsIXBLPrototypeBinding Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLPrototypeBinding::GetBasePrototype(nsIXBLPrototypeBinding** aResult)
{
  *aResult = mBaseBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::SetBasePrototype(nsIXBLPrototypeBinding* aBinding)
{
  if (mBaseBinding.get() == aBinding)
    return NS_OK;

  if (mBaseBinding) {
    NS_ERROR("Base XBL prototype binding is already defined!");
    return NS_OK;
  }

  mBaseBinding = aBinding; // Comptr handles rel/add
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetBindingElement(nsIContent** aResult)
{
  *aResult = mBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  nsAutoString inheritStyle;
  mBinding->GetAttribute(kNameSpaceID_None, kInheritStyleAtom, inheritStyle);
  if (inheritStyle == NS_LITERAL_STRING("false"))
    mInheritStyle = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP 
nsXBLPrototypeBinding::GetBindingURI(nsCString& aResult)
{
  nsCOMPtr<nsIXBLDocumentInfo> info;
  GetXBLDocumentInfo(nsnull, getter_AddRefs(info));
  
  NS_ASSERTION(info, "The prototype binding has somehow lost its XBLDocInfo! Bad bad bad!!!\n");
  if (!info)
    return NS_ERROR_FAILURE;

  info->GetDocumentURI(aResult);
  aResult += "#";
  aResult += mID;
  return NS_OK;
}

NS_IMETHODIMP 
nsXBLPrototypeBinding::GetDocURI(nsCString& aResult)
{
  nsCOMPtr<nsIXBLDocumentInfo> info;
  GetXBLDocumentInfo(nsnull, getter_AddRefs(info));
  
  NS_ASSERTION(info, "The prototype binding has somehow lost its XBLDocInfo! Bad bad bad!!!\n");
  if (!info)
    return NS_ERROR_FAILURE;

  info->GetDocumentURI(aResult);
  return NS_OK;
}

NS_IMETHODIMP 
nsXBLPrototypeBinding::GetID(nsCString& aResult)
{
  aResult = mID;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetAllowScripts(PRBool* aResult)
{
  nsCOMPtr<nsIXBLDocumentInfo> info;
  GetXBLDocumentInfo(nsnull, getter_AddRefs(info));
  return info->GetScriptAccess(aResult);
}

NS_IMETHODIMP
nsXBLPrototypeBinding::BindingAttached(nsIDOMEventReceiver* aReceiver)
{
  if (mSpecialHandler)
    return mSpecialHandler->BindingAttached(aReceiver);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::BindingDetached(nsIDOMEventReceiver* aReceiver)
{
  if (mSpecialHandler)
    return mSpecialHandler->BindingDetached(aReceiver);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::InheritsStyle(PRBool* aResult)
{
  *aResult = mInheritStyle;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetXBLDocumentInfo(nsIContent* aBoundElement, nsIXBLDocumentInfo** aResult)
{
  nsCOMPtr<nsIXBLDocumentInfo> info(do_QueryReferent(mXBLDocInfoWeak));
  if (info) {
    *aResult = info;
    NS_ADDREF(*aResult);
    return NS_OK;
  }
  else *aResult=nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::HasBasePrototype(PRBool* aResult)
{
  *aResult = mHasBaseProto;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::SetHasBasePrototype(PRBool aHasBase)
{
  mHasBaseProto = aHasBase;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetPrototypeHandlers(nsIXBLPrototypeHandler** aResult,
                                            nsIXBLPrototypeHandler** aSpecialResult)
{
  *aResult = mPrototypeHandler;
  *aSpecialResult = mSpecialHandler;
  NS_IF_ADDREF(*aResult);
  NS_IF_ADDREF(*aSpecialResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::SetPrototypeHandlers(nsIXBLPrototypeHandler* aHandler,
                                            nsIXBLPrototypeHandler* aSpecialHandler)
{
  mPrototypeHandler = aHandler;
  mSpecialHandler = aSpecialHandler;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag, 
                                        nsIContent* aChangedElement, nsIContent* aAnonymousContent)
{
  if (!mAttributeTable)
    return NS_OK;

  nsISupportsKey key(aAttribute);
  nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                 mAttributeTable->Get(&key)));

  nsCOMPtr<nsIXBLAttributeEntry> xblAttr = do_QueryInterface(supports);
  if (!xblAttr)
    return NS_OK;

  // Iterate over the elements in the array.
  nsCOMPtr<nsIContent> content;
  GetImmediateChild(kContentAtom, getter_AddRefs(content));
  while (xblAttr) {
    nsCOMPtr<nsIContent> element;
    nsCOMPtr<nsIAtom> dstAttr;
    xblAttr->GetElement(getter_AddRefs(element));

    nsCOMPtr<nsIContent> realElement;
    LocateInstance(content, aAnonymousContent, element, getter_AddRefs(realElement));

    xblAttr->GetDstAttribute(getter_AddRefs(dstAttr));

    if (aRemoveFlag)
      realElement->UnsetAttribute(aNameSpaceID, dstAttr, PR_TRUE);
    else {
      nsAutoString value;
      nsresult result = aChangedElement->GetAttribute(aNameSpaceID, aAttribute, value);
      PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                            result == NS_CONTENT_ATTR_HAS_VALUE);

      if (attrPresent)
        realElement->SetAttribute(aNameSpaceID, dstAttr, value, PR_TRUE);
    }

    // See if we're the <html> tag in XUL, and see if value is being
    // set or unset on us.
    nsCOMPtr<nsIAtom> tag;
    realElement->GetTag(*getter_AddRefs(tag));
    if ((tag.get() == kHTMLAtom) && (dstAttr.get() == kValueAtom)) {
      // Flush out all our kids.
      PRInt32 childCount;
      realElement->ChildCount(childCount);
      for (PRInt32 i = 0; i < childCount; i++)
        realElement->RemoveChildAt(0, PR_TRUE);
      
      if (!aRemoveFlag) {
        // Construct a new text node and insert it.
        nsAutoString value;
        aChangedElement->GetAttribute(aNameSpaceID, aAttribute, value);
        if (!value.IsEmpty()) {
          nsCOMPtr<nsIDOMText> textNode;
          nsCOMPtr<nsIDocument> doc;
          aChangedElement->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
          domDoc->CreateTextNode(value, getter_AddRefs(textNode));
          nsCOMPtr<nsIDOMNode> dummy;
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(realElement));
          domElement->AppendChild(textNode, getter_AddRefs(dummy));
        }
      }
    }
    nsCOMPtr<nsIXBLAttributeEntry> tmpAttr = xblAttr;
    tmpAttr->GetNext(getter_AddRefs(xblAttr));
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetInsertionPoint(nsIContent* aBoundElement, nsIContent* aCopyRoot,
                                         nsIContent* aChild, nsIContent** aResult)
{
  if (mInsertionPointTable) {
    nsCOMPtr<nsIAtom> tag;
    aChild->GetTag(*getter_AddRefs(tag));
    nsISupportsKey key(tag);
    nsCOMPtr<nsIContent> content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, 
                                                                 mInsertionPointTable->Get(&key)));
    if (!content) {
      nsISupportsKey key2(kChildrenAtom);
      content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, mInsertionPointTable->Get(&key2)));
    }

    nsCOMPtr<nsIContent> realContent;
    if (content) {
      nsCOMPtr<nsIContent> templContent;
      GetImmediateChild(kContentAtom, getter_AddRefs(templContent));
      LocateInstance(templContent, aCopyRoot, content, getter_AddRefs(realContent));
    }
    if (realContent)
      *aResult = realContent;
    else
      *aResult = aBoundElement;
    NS_IF_ADDREF(*aResult);
  }
  return NS_OK;  
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetSingleInsertionPoint(nsIContent* aBoundElement,
                                               nsIContent* aCopyRoot,
                                               nsIContent** aResult, PRBool* aMultipleInsertionPoints)
{ 
  if (mInsertionPointTable) {
    if(mInsertionPointTable->Count() == 1) {
      nsISupportsKey key(kChildrenAtom);
      nsCOMPtr<nsIContent> content = getter_AddRefs(NS_STATIC_CAST(nsIContent*, 
                                                                   mInsertionPointTable->Get(&key)));
      nsCOMPtr<nsIContent> realContent;
      if (content) {
        nsCOMPtr<nsIContent> templContent;
        GetImmediateChild(kContentAtom, getter_AddRefs(templContent));
        LocateInstance(templContent, aCopyRoot, content, getter_AddRefs(realContent));
      }
      else {
        // The only insertion point specified was actually a filtered insertion point.
        // This means (strictly speaking) that we actually have multiple insertion
        // points: the filtered one and a generic insertion point (content that doesn't
        // match the filter will just go right underneath the bound element).
        *aMultipleInsertionPoints = PR_TRUE;
        *aResult = nsnull;
        return NS_OK;
      }

      *aMultipleInsertionPoints = PR_FALSE;
      if (realContent)
        *aResult = realContent;
      else
        *aResult = aBoundElement;
      NS_IF_ADDREF(*aResult);
    }
    else 
      *aMultipleInsertionPoints = PR_TRUE;
  }
  return NS_OK;  
}

NS_IMETHODIMP
nsXBLPrototypeBinding::SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag)
{
  mBaseNameSpaceID = aNamespaceID;
  mBaseTag = aTag;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::GetBaseTag(PRInt32* aNamespaceID, nsIAtom** aResult)
{
  if (mBaseTag) {
    *aResult = mBaseTag;
    NS_ADDREF(*aResult);
    *aNamespaceID = mBaseNameSpaceID;
  }
  else *aResult = nsnull;
  return NS_OK;
}

// Internal helpers ///////////////////////////////////////////////////////////////////////

void
nsXBLPrototypeBinding::GetImmediateChild(nsIAtom* aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  mBinding->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mBinding->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}

void
nsXBLPrototypeBinding::ConstructHandlers()
{
  // See if this binding has a handler elt.
  nsCOMPtr<nsIContent> handlers;
  GetImmediateChild(kHandlersAtom, getter_AddRefs(handlers));
  if (handlers)
    nsXBLService::BuildHandlerChain(handlers, getter_AddRefs(mPrototypeHandler), getter_AddRefs(mSpecialHandler));
}

void
nsXBLPrototypeBinding::LocateInstance(nsIContent* aTemplRoot, nsIContent* aCopyRoot, 
                                      nsIContent* aTemplChild, nsIContent** aCopyResult)
{
  // XXX We will get in trouble if the binding instantiation deviates from the template
  // in the prototype.
  if (aTemplChild == aTemplRoot) {
    *aCopyResult = nsnull;
    return;
  }

  nsCOMPtr<nsIContent> templParent;
  nsCOMPtr<nsIContent> copyParent;

  aTemplChild->GetParent(*getter_AddRefs(templParent));
    
  if (templParent.get() == aTemplRoot)
    copyParent = aCopyRoot;
  else
    LocateInstance(aTemplRoot, aCopyRoot, templParent, getter_AddRefs(copyParent));
  
  PRInt32 index;
  templParent->IndexOf(aTemplChild, index);
  copyParent->ChildAt(index, *aCopyResult); // Addref happens here.
}

struct nsXBLAttrChangeData
{
  nsXBLPrototypeBinding* mProto;
  nsIContent* mBoundElement;
  nsIContent* mContent;

  nsXBLAttrChangeData(nsXBLPrototypeBinding* aProto,
                      nsIContent* aElt, nsIContent* aContent) 
  :mProto(aProto), mBoundElement(aElt), mContent(aContent) {};
};

PRBool PR_CALLBACK SetAttrs(nsHashKey* aKey, void* aData, void* aClosure)
{
  // XXX How to deal with NAMESPACES!!!?
  nsIXBLAttributeEntry* entry = (nsIXBLAttributeEntry*)aData;
  nsXBLAttrChangeData* changeData = (nsXBLAttrChangeData*)aClosure;

  nsCOMPtr<nsIAtom> src;
  entry->GetSrcAttribute(getter_AddRefs(src));

  nsAutoString value;
  nsresult result = changeData->mBoundElement->GetAttribute(kNameSpaceID_None, src, value);
  PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                        result == NS_CONTENT_ATTR_HAS_VALUE);
  if (attrPresent) {
    nsCOMPtr<nsIContent> content;
    changeData->mProto->GetImmediateChild(nsXBLPrototypeBinding::kContentAtom, getter_AddRefs(content));

    nsCOMPtr<nsIXBLAttributeEntry> curr = entry;
    while (curr) {
      nsCOMPtr<nsIAtom> dst;
      nsCOMPtr<nsIContent> element;
      curr->GetDstAttribute(getter_AddRefs(dst));
      curr->GetElement(getter_AddRefs(element));

      nsCOMPtr<nsIContent> realElement;
      changeData->mProto->LocateInstance(content, changeData->mContent, element, getter_AddRefs(realElement));
      if (realElement) {
        realElement->SetAttribute(kNameSpaceID_None, dst, value, PR_FALSE);
        nsCOMPtr<nsIAtom> tag;
        realElement->GetTag(*getter_AddRefs(tag));
        if ((tag.get() == nsXBLPrototypeBinding::kHTMLAtom) && (dst.get() == nsXBLPrototypeBinding::kValueAtom) && !value.IsEmpty()) {
          nsCOMPtr<nsIDOMText> textNode;
          nsCOMPtr<nsIDocument> doc;
          changeData->mBoundElement->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
          domDoc->CreateTextNode(value, getter_AddRefs(textNode));
          nsCOMPtr<nsIDOMNode> dummy;
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(realElement));
          domElement->AppendChild(textNode, getter_AddRefs(dummy));
        }
      }

      nsCOMPtr<nsIXBLAttributeEntry> next = curr;
      curr->GetNext(getter_AddRefs(next));
      curr = next;
    }
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLPrototypeBinding::SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent)
{
  if (mAttributeTable) {
    nsXBLAttrChangeData data(this, aBoundElement, aAnonymousContent);
    mAttributeTable->Enumerate(SetAttrs, (void*)&data);
  }
  return NS_OK;
}

void
nsXBLPrototypeBinding::ConstructAttributeTable(nsIContent* aElement)
{
  nsAutoString inherits;
  aElement->GetAttribute(kNameSpaceID_None, kInheritsAtom, inherits);
  if (!inherits.IsEmpty()) {
    if (!mAttributeTable) {
      mAttributeTable = new nsSupportsHashtable(4);
    }

    // The user specified at least one attribute.
    char* str = inherits.ToNewCString();
    char* newStr;
    // XXX We should use a strtok function that tokenizes PRUnichars
    // so that we don't have to convert from Unicode to ASCII and then back

    char* token = nsCRT::strtok( str, ", ", &newStr );
    while( token != NULL ) {
      // Build an atom out of this attribute.
      nsCOMPtr<nsIAtom> atom;
      nsCOMPtr<nsIAtom> attribute;

      // Figure out if this token contains a :. 
      nsAutoString attrTok; attrTok.AssignWithConversion(token);
      PRInt32 index = attrTok.Find("=", PR_TRUE);
      if (index != -1) {
        // This attribute maps to something different.
        nsAutoString left, right;
        attrTok.Left(left, index);
        attrTok.Right(right, attrTok.Length()-index-1);

        atom = getter_AddRefs(NS_NewAtom(right.GetUnicode()));
        attribute = getter_AddRefs(NS_NewAtom(left.GetUnicode()));
      }
      else {
        nsAutoString tok; tok.AssignWithConversion(token);
        atom = getter_AddRefs(NS_NewAtom(tok.GetUnicode()));
        attribute = atom;
      }
      
      // Create an XBL attribute entry.
      nsXBLAttributeEntry* xblAttr = new (kPool) nsXBLAttributeEntry(atom, attribute, aElement);

      // Now we should see if some element within our anonymous
      // content is already observing this attribute.
      nsISupportsKey key(atom);
      nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                     mAttributeTable->Get(&key)));
  
      nsCOMPtr<nsIXBLAttributeEntry> entry = do_QueryInterface(supports);
      if (!entry) {
        // Put it in the table.
        mAttributeTable->Put(&key, xblAttr);
      } else {
        nsCOMPtr<nsIXBLAttributeEntry> attr = entry;
        nsCOMPtr<nsIXBLAttributeEntry> tmpAttr = entry;
        do {
          attr = tmpAttr;
          attr->GetNext(getter_AddRefs(tmpAttr));
        } while (tmpAttr);
        attr->SetNext(xblAttr);
      }

      // Now remove the inherits attribute from the element so that it doesn't
      // show up on clones of the element.  It is used
      // by the template only, and we don't need it anymore.
      aElement->UnsetAttribute(kNameSpaceID_None, kInheritsAtom, PR_FALSE);

      token = nsCRT::strtok( newStr, ", ", &newStr );
    }

    nsMemory::Free(str);
  }

  // Recur into our children.
  PRInt32 childCount;
  aElement->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aElement->ChildAt(i, *getter_AddRefs(child));
    ConstructAttributeTable(child);
  }
}

void 
nsXBLPrototypeBinding::ConstructInsertionTable(nsIContent* aContent)
{
  nsCOMPtr<nsISupportsArray> childrenElements;
  GetNestedChildren(kChildrenAtom, aContent, getter_AddRefs(childrenElements));

  if (!childrenElements)
    return;

  mInsertionPointTable = new nsSupportsHashtable;

  PRUint32 count;
  childrenElements->Count(&count);
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supp;
    childrenElements->GetElementAt(i, getter_AddRefs(supp));
    nsCOMPtr<nsIContent> child(do_QueryInterface(supp));
    if (child) {
      nsCOMPtr<nsIContent> parent; 
      child->GetParent(*getter_AddRefs(parent));
      nsAutoString includes;
      child->GetAttribute(kNameSpaceID_None, kIncludesAtom, includes);
      if (includes.IsEmpty()) {
        nsISupportsKey key(kChildrenAtom);
        mInsertionPointTable->Put(&key, parent);
      }
      else {
        // The user specified at least one attribute.
        char* str = includes.ToNewCString();
        char* newStr;
        // XXX We should use a strtok function that tokenizes PRUnichar's
        // so that we don't have to convert from Unicode to ASCII and then back

        char* token = nsCRT::strtok( str, "| ", &newStr );
        while( token != NULL ) {
          // Build an atom out of this string.
          nsCOMPtr<nsIAtom> atom;
            
          nsAutoString tok; tok.AssignWithConversion(token);
          atom = getter_AddRefs(NS_NewAtom(tok.GetUnicode()));
           
          nsISupportsKey key(atom);
          mInsertionPointTable->Put(&key, parent);
          
          token = nsCRT::strtok( newStr, ", ", &newStr );
        }

        nsMemory::Free(str);
      }
    }
  }

  // Now remove the <children> elements.
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supp;
    childrenElements->GetElementAt(i, getter_AddRefs(supp));
    nsCOMPtr<nsIContent> child(do_QueryInterface(supp));
    if (child) {
      nsCOMPtr<nsIContent> parent; 
      child->GetParent(*getter_AddRefs(parent));
      PRInt32 index;
      parent->IndexOf(child, index);
      parent->RemoveChildAt(index, PR_FALSE);
    }
  }
}


void
nsXBLPrototypeBinding::GetNestedChildren(nsIAtom* aTag, nsIContent* aContent, nsISupportsArray** aList)
{
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      if (!*aList)
        NS_NewISupportsArray(aList); // Addref happens here.
      (*aList)->AppendElement(child);
    }
    else
      GetNestedChildren(aTag, child, aList);
  }
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLPrototypeBinding(const nsCString& aRef, nsIContent* aElement, 
                          nsIXBLDocumentInfo* aInfo, nsIXBLPrototypeBinding** aResult)
{
  *aResult = new nsXBLPrototypeBinding(aRef, aElement, aInfo);
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

