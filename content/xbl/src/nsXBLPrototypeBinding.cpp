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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIDOMEventReceiver.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsSupportsArray.h"
#include "nsINameSpace.h"
#include "nsXBLService.h"
#include "nsXBLBinding.h"
#include "nsXBLInsertionPoint.h"
#include "nsXBLPrototypeBinding.h"
#include "nsFixedSizeAllocator.h"
#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIPresShell.h"
#include "nsIDocumentObserver.h"
#include "nsHTMLAtoms.h"
#include "nsXULAtoms.h"
#include "nsXBLAtoms.h"
#include "nsXBLProtoImpl.h"
#include "nsCRT.h"

#include "nsIScriptContext.h"

#include "nsICSSLoader.h"
#include "nsIStyleRuleProcessor.h"

// Helper Classes =====================================================================

// nsXBLAttributeEntry and helpers.  This class is used to efficiently handle
// attribute changes in anonymous content.

class nsXBLAttributeEntry {
public:
  nsIAtom* GetSrcAttribute() { return mSrcAttribute; }
  nsIAtom* GetDstAttribute() { return mDstAttribute; }
  
  nsIContent* GetElement() { return mElement; }

  nsXBLAttributeEntry* GetNext() { return mNext; }
  void SetNext(nsXBLAttributeEntry* aEntry) { mNext = aEntry; }

  static nsXBLAttributeEntry*
  Create(nsIAtom* aSrcAtom, nsIAtom* aDstAtom, nsIContent* aContent) {
    void* place = nsXBLPrototypeBinding::kAttrPool->Alloc(sizeof(nsXBLAttributeEntry));
    return place ? ::new (place) nsXBLAttributeEntry(aSrcAtom, aDstAtom, aContent) : nsnull;
  }

  static void
  Destroy(nsXBLAttributeEntry* aSelf) {
    aSelf->~nsXBLAttributeEntry();
    nsXBLPrototypeBinding::kAttrPool->Free(aSelf, sizeof(*aSelf));
  }

protected:
  nsIContent* mElement;

  nsCOMPtr<nsIAtom> mSrcAttribute;
  nsCOMPtr<nsIAtom> mDstAttribute;
  nsXBLAttributeEntry* mNext;

  nsXBLAttributeEntry(nsIAtom* aSrcAtom, nsIAtom* aDstAtom,
                      nsIContent* aContent)
    : mElement(aContent),
      mSrcAttribute(aSrcAtom),
      mDstAttribute(aDstAtom),
      mNext(nsnull) { }

  ~nsXBLAttributeEntry() { delete mNext; }

private:
  // Hide so that only Create() and Destroy() can be used to
  // allocate and deallocate from the heap
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
  static void operator delete(void*, size_t) {}
};

// nsXBLInsertionPointEntry and helpers.  This class stores all the necessary
// info to figure out the position of an insertion point.
// The same insertion point may be in the insertion point table for multiple
// keys, so we refcount the entries.

class nsXBLInsertionPointEntry {
public:
  nsIContent* GetInsertionParent() { return mInsertionParent; }
  PRUint32 GetInsertionIndex() { return mInsertionIndex; }
  void SetInsertionIndex(PRUint32 aIndex) { mInsertionIndex = aIndex; }

  nsIContent* GetDefaultContent() { return mDefaultContent; }
  void SetDefaultContent(nsIContent* aChildren) { mDefaultContent = aChildren; }


  static nsXBLInsertionPointEntry*
  Create(nsIContent* aParent) {
    void* place = nsXBLPrototypeBinding::kInsPool->Alloc(sizeof(nsXBLInsertionPointEntry));
    return place ? ::new (place) nsXBLInsertionPointEntry(aParent) : nsnull;
  }

  static void
  Destroy(nsXBLInsertionPointEntry* aSelf) {
    aSelf->~nsXBLInsertionPointEntry();
    nsXBLPrototypeBinding::kInsPool->Free(aSelf, sizeof(*aSelf));
  }

  nsrefcnt AddRef() {
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "nsXBLInsertionPointEntry", sizeof(nsXBLInsertionPointEntry));
    return mRefCnt;
  }

  nsrefcnt Release() {
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "nsXBLInsertionPointEntry");
    if (mRefCnt == 0) {
      Destroy(this);
      return 0;
    }
    return mRefCnt;
  }

protected:
  nsCOMPtr<nsIContent> mInsertionParent;
  nsCOMPtr<nsIContent> mDefaultContent;
  PRUint32 mInsertionIndex;
  nsrefcnt mRefCnt;

  nsXBLInsertionPointEntry(nsIContent* aParent)
    : mInsertionParent(aParent),
      mInsertionIndex(0),
      mRefCnt(0) { }

private:
  // Hide so that only Create() and Destroy() can be used to
  // allocate and deallocate from the heap
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
  static void operator delete(void*, size_t) {}
};

// =============================================================================

// Static initialization
PRUint32 nsXBLPrototypeBinding::gRefCnt = 0;

nsFixedSizeAllocator* nsXBLPrototypeBinding::kAttrPool;
nsFixedSizeAllocator* nsXBLPrototypeBinding::kInsPool;

static const PRInt32 kNumElements = 128;

static const size_t kAttrBucketSizes[] = {
  sizeof(nsXBLAttributeEntry)
};

static const PRInt32 kAttrNumBuckets = sizeof(kAttrBucketSizes)/sizeof(size_t);
static const PRInt32 kAttrInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLAttributeEntry))) * kNumElements;

static const size_t kInsBucketSizes[] = {
  sizeof(nsXBLInsertionPointEntry)
};

static const PRInt32 kInsNumBuckets = sizeof(kInsBucketSizes)/sizeof(size_t);
static const PRInt32 kInsInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLInsertionPointEntry))) * kNumElements;

// Implementation /////////////////////////////////////////////////////////////////

MOZ_DECL_CTOR_COUNTER(nsXBLPrototypeBinding)

// Constructors/Destructors
nsXBLPrototypeBinding::nsXBLPrototypeBinding()
: mImplementation(nsnull),
  mBaseBinding(nsnull),
  mInheritStyle(PR_TRUE), 
  mHasBaseProto(PR_TRUE),
  mKeyHandlersRegistered(PR_FALSE),
  mResources(nsnull),
  mAttributeTable(nsnull), 
  mInsertionPointTable(nsnull),
  mInterfaceTable(nsnull)
{
  MOZ_COUNT_CTOR(nsXBLPrototypeBinding);
  gRefCnt++;

  if (gRefCnt == 1) {
    kAttrPool = new nsFixedSizeAllocator();
    if (kAttrPool) {
      kAttrPool->Init("XBL Attribute Entries", kAttrBucketSizes, kAttrNumBuckets, kAttrInitialSize);
    }
    kInsPool = new nsFixedSizeAllocator();
    if (kInsPool) {
      kInsPool->Init("XBL Insertion Point Entries", kInsBucketSizes, kInsNumBuckets, kInsInitialSize);
    }
  }
}

nsresult
nsXBLPrototypeBinding::Init(const nsACString& aID,
                            nsIXBLDocumentInfo* aInfo,
                            nsIContent* aElement)
{
  if (!kAttrPool || !kInsPool) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri),
                          NS_LITERAL_CSTRING("#") + aID,
                          nsnull,
                          aInfo->DocumentURI());
  NS_ENSURE_SUCCESS(rv, rv);

  mBindingURI = do_QueryInterface(uri, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mXBLDocInfoWeak = aInfo;

  SetBindingElement(aElement);
  return NS_OK;
}

void
nsXBLPrototypeBinding::Initialize()
{
  nsCOMPtr<nsIContent> content = GetImmediateChild(nsXBLAtoms::content);
  if (content) {
    ConstructAttributeTable(content);
    ConstructInsertionTable(content);
  }
}

nsXBLPrototypeBinding::~nsXBLPrototypeBinding(void)
{
  delete mResources;
  delete mAttributeTable;
  delete mInsertionPointTable;
  delete mInterfaceTable;
  delete mImplementation;
  gRefCnt--;
  if (gRefCnt == 0) {
    delete kAttrPool;
    delete kInsPool;
  }
  MOZ_COUNT_DTOR(nsXBLPrototypeBinding);
}

void
nsXBLPrototypeBinding::SetBasePrototype(nsXBLPrototypeBinding* aBinding)
{
  if (mBaseBinding == aBinding)
    return;

  if (mBaseBinding) {
    NS_ERROR("Base XBL prototype binding is already defined!");
    return;
  }

  mBaseBinding = aBinding;
}

already_AddRefed<nsIContent>
nsXBLPrototypeBinding::GetBindingElement()
{
  nsIContent* result = mBinding;
  NS_IF_ADDREF(result);
  return result;
}

void
nsXBLPrototypeBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  nsAutoString inheritStyle;
  mBinding->GetAttr(kNameSpaceID_None, nsXBLAtoms::inheritstyle, inheritStyle);
  if (inheritStyle.EqualsLiteral("false"))
    mInheritStyle = PR_FALSE;
}

nsresult
nsXBLPrototypeBinding::GetAllowScripts(PRBool* aResult)
{
  return mXBLDocInfoWeak->GetScriptAccess(aResult);
}

PRBool
nsXBLPrototypeBinding::LoadResources()
{
  if (mResources) {
    PRBool result;
    mResources->LoadResources(&result);
    return result;
  }

  return PR_TRUE;
}

nsresult
nsXBLPrototypeBinding::AddResource(nsIAtom* aResourceType, const nsAString& aSrc)
{
  if (!mResources) {
    mResources = new nsXBLPrototypeResources(this);
    if (!mResources)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mResources->AddResource(aResourceType, aSrc);
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::FlushSkinSheets()
{
  if (mResources)
    return mResources->FlushSkinSheets();
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::BindingAttached(nsIDOMEventReceiver* aReceiver)
{
  if (mImplementation && mImplementation->mConstructor)
    return mImplementation->mConstructor->BindingAttached(aReceiver);
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::BindingDetached(nsIDOMEventReceiver* aReceiver)
{
  if (mImplementation && mImplementation->mDestructor)
    return mImplementation->mDestructor->BindingDetached(aReceiver);
  return NS_OK;
}

nsXBLPrototypeHandler*
nsXBLPrototypeBinding::GetConstructor()
{
  if (mImplementation)
    return mImplementation->mConstructor;

  return nsnull;
}

nsXBLPrototypeHandler*
nsXBLPrototypeBinding::GetDestructor()
{
  if (mImplementation)
    return mImplementation->mDestructor;

  return nsnull;
}

nsresult
nsXBLPrototypeBinding::SetConstructor(nsXBLPrototypeHandler* aHandler)
{
  if (!mImplementation)
    return NS_ERROR_FAILURE;
  mImplementation->mConstructor = aHandler;
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::SetDestructor(nsXBLPrototypeHandler* aHandler)
{
  if (!mImplementation)
    return NS_ERROR_FAILURE;
  mImplementation->mDestructor = aHandler;
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::InstallImplementation(nsIContent* aBoundElement)
{
  if (mImplementation)
    return mImplementation->InstallImplementation(this, aBoundElement);
  return NS_OK;
}

void
nsXBLPrototypeBinding::AttributeChanged(nsIAtom* aAttribute,
                                        PRInt32 aNameSpaceID,
                                        PRBool aRemoveFlag, 
                                        nsIContent* aChangedElement,
                                        nsIContent* aAnonymousContent,
                                        PRBool aNotify)
{
  if (!mAttributeTable)
    return;

  nsISupportsKey key(aAttribute);
  nsXBLAttributeEntry* xblAttr = NS_STATIC_CAST(nsXBLAttributeEntry*, mAttributeTable->Get(&key));
  if (!xblAttr)
    return;

  // Iterate over the elements in the array.
  nsCOMPtr<nsIContent> content = GetImmediateChild(nsXBLAtoms::content);
  while (xblAttr) {
    nsIContent* element = xblAttr->GetElement();

    nsCOMPtr<nsIContent> realElement;
    realElement = LocateInstance(aChangedElement, content, aAnonymousContent,
                                 element);

    if (realElement) {
      nsIAtom* dstAttr = xblAttr->GetDstAttribute();

      if (aRemoveFlag)
        realElement->UnsetAttr(aNameSpaceID, dstAttr, aNotify);
      else {
        PRBool attrPresent = PR_TRUE;
        nsAutoString value;
        // Check to see if the src attribute is xbl:text.  If so, then we need to obtain the 
        // children of the real element and get the text nodes' values.
        if (aAttribute == nsXBLAtoms::xbltext) {
          nsXBLBinding::GetTextData(aChangedElement, value);
          value.StripChar(PRUnichar('\n'));
          value.StripChar(PRUnichar('\r'));
          nsAutoString stripVal(value);
          stripVal.StripWhitespace();
          if (stripVal.IsEmpty()) 
            attrPresent = PR_FALSE;
        }    
        else {
          nsresult result = aChangedElement->GetAttr(aNameSpaceID, aAttribute, value);
          attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                         result == NS_CONTENT_ATTR_HAS_VALUE);
        }

        if (attrPresent)
          realElement->SetAttr(aNameSpaceID, dstAttr, value, aNotify);
      }

      // See if we're the <html> tag in XUL, and see if value is being
      // set or unset on us.  We may also be a tag that is having
      // xbl:text set on us.

      if (dstAttr == nsXBLAtoms::xbltext ||
          realElement->GetNodeInfo()->Equals(nsHTMLAtoms::html,
                                             kNameSpaceID_XUL) &&
          dstAttr == nsHTMLAtoms::value) {
        // Flush out all our kids.
        PRUint32 childCount = realElement->GetChildCount();
        for (PRUint32 i = 0; i < childCount; i++)
          realElement->RemoveChildAt(0, aNotify);

        if (!aRemoveFlag) {
          // Construct a new text node and insert it.
          nsAutoString value;
          aChangedElement->GetAttr(aNameSpaceID, aAttribute, value);
          if (!value.IsEmpty()) {
            nsCOMPtr<nsIDOMText> textNode;
            nsCOMPtr<nsIDOMDocument> domDoc(
                     do_QueryInterface(aChangedElement->GetDocument()));
            domDoc->CreateTextNode(value, getter_AddRefs(textNode));
            nsCOMPtr<nsIDOMNode> dummy;
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(realElement));
            domElement->AppendChild(textNode, getter_AddRefs(dummy));
          }
        }
      }
    }

    xblAttr = xblAttr->GetNext();
  }
}

struct InsertionData {
  nsIXBLBinding* mBinding;
  nsXBLPrototypeBinding* mPrototype;

  InsertionData(nsIXBLBinding* aBinding,
                nsXBLPrototypeBinding* aPrototype) 
    :mBinding(aBinding), mPrototype(aPrototype) {};
};

PRBool PR_CALLBACK InstantiateInsertionPoint(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLInsertionPointEntry* entry = NS_STATIC_CAST(nsXBLInsertionPointEntry*, aData);
  InsertionData* data = NS_STATIC_CAST(InsertionData*, aClosure);
  nsIXBLBinding* binding = data->mBinding;
  nsXBLPrototypeBinding* proto = data->mPrototype;

  // Get the insertion parent.
  nsIContent* content = entry->GetInsertionParent();
  PRUint32 index = entry->GetInsertionIndex();
  nsIContent* defContent = entry->GetDefaultContent();

  // Locate the real content.
  nsCOMPtr<nsIContent> realContent;
  nsCOMPtr<nsIContent> instanceRoot;
  binding->GetAnonymousContent(getter_AddRefs(instanceRoot));
  nsCOMPtr<nsIContent> templRoot = proto->GetImmediateChild(nsXBLAtoms::content);
  realContent = proto->LocateInstance(nsnull, templRoot, instanceRoot, content);
  if (!realContent)
    binding->GetBoundElement(getter_AddRefs(realContent));

  // Now that we have the real content, look it up in our table.
  nsVoidArray* points;
  binding->GetInsertionPointsFor(realContent, &points);
  nsXBLInsertionPoint* insertionPoint = nsnull;
  PRInt32 count = points->Count();
  PRInt32 i = 0;
  PRInt32 currIndex = 0;  
  
  for ( ; i < count; i++) {
    nsXBLInsertionPoint* currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, points->ElementAt(i));
    currIndex = currPoint->GetInsertionIndex();
    if (currIndex == (PRInt32)index) {
      // This is a match. Break out of the loop and set our variable.
      insertionPoint = currPoint;
      break;
    }
    
    if (currIndex > (PRInt32)index)
      // There was no match. Break.
      break;
  }

  if (!insertionPoint) {
    // We need to make a new insertion point.
    insertionPoint = new nsXBLInsertionPoint(realContent, index, defContent);
    points->InsertElementAt(insertionPoint, i);
  }

  return PR_TRUE;
}

void
nsXBLPrototypeBinding::InstantiateInsertionPoints(nsIXBLBinding* aBinding)
{
  InsertionData data(aBinding, this);
  if (mInsertionPointTable)
    mInsertionPointTable->Enumerate(InstantiateInsertionPoint, &data);
}

void
nsXBLPrototypeBinding::GetInsertionPoint(nsIContent* aBoundElement,
                                         nsIContent* aCopyRoot,
                                         nsIContent* aChild,
                                         nsIContent** aResult,
                                         PRUint32* aIndex,
                                         nsIContent** aDefaultContent)
{
  if (mInsertionPointTable) {
    nsISupportsKey key(aChild->Tag());
    nsXBLInsertionPointEntry* entry = NS_STATIC_CAST(nsXBLInsertionPointEntry*, mInsertionPointTable->Get(&key));
    if (!entry) {
      nsISupportsKey key2(nsXBLAtoms::children);
      entry = NS_STATIC_CAST(nsXBLInsertionPointEntry*, mInsertionPointTable->Get(&key2));
    }

    nsCOMPtr<nsIContent> realContent;
    if (entry) {
      nsIContent* content = entry->GetInsertionParent();
      *aIndex = entry->GetInsertionIndex();
      NS_IF_ADDREF(*aDefaultContent = entry->GetDefaultContent());
      nsCOMPtr<nsIContent> templContent;
      templContent = GetImmediateChild(nsXBLAtoms::content);
      realContent = LocateInstance(nsnull, templContent, aCopyRoot, content);
    }
    else {
      // We got nothin'.  Bail.
      *aResult = nsnull;
      return;
    }

    if (realContent)
      *aResult = realContent;
    else
      *aResult = aBoundElement;

    NS_IF_ADDREF(*aResult);
  }
}

void
nsXBLPrototypeBinding::GetSingleInsertionPoint(nsIContent* aBoundElement,
                                               nsIContent* aCopyRoot,
                                               nsIContent** aResult,
                                               PRUint32* aIndex,
                                               PRBool* aMultipleInsertionPoints,
                                               nsIContent** aDefaultContent)
{ 
  if (mInsertionPointTable) {
    if(mInsertionPointTable->Count() == 1) {
      nsISupportsKey key(nsXBLAtoms::children);
      nsXBLInsertionPointEntry* entry = NS_STATIC_CAST(nsXBLInsertionPointEntry*, mInsertionPointTable->Get(&key));

      nsCOMPtr<nsIContent> realContent;
      if (entry) {
        nsIContent* content = entry->GetInsertionParent();
        *aIndex = entry->GetInsertionIndex();
        NS_IF_ADDREF(*aDefaultContent = entry->GetDefaultContent());
        nsCOMPtr<nsIContent> templContent;
        templContent = GetImmediateChild(nsXBLAtoms::content);
        realContent = LocateInstance(nsnull, templContent, aCopyRoot, content);
      }
      else {
        // The only insertion point specified was actually a filtered insertion point.
        // This means (strictly speaking) that we actually have multiple insertion
        // points: the filtered one and a generic insertion point (content that doesn't
        // match the filter will just go right underneath the bound element).
        *aMultipleInsertionPoints = PR_TRUE;
        *aResult = nsnull;
        *aIndex = 0;
        return;
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
}

void
nsXBLPrototypeBinding::SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag)
{
  mBaseNameSpaceID = aNamespaceID;
  mBaseTag = aTag;
}

void
nsXBLPrototypeBinding::GetBaseTag(PRInt32* aNamespaceID, nsIAtom** aResult)
{
  if (mBaseTag) {
    *aResult = mBaseTag;
    NS_ADDREF(*aResult);
    *aNamespaceID = mBaseNameSpaceID;
  }
  else *aResult = nsnull;
}

PRBool
nsXBLPrototypeBinding::ImplementsInterface(REFNSIID aIID)
{
  // Check our IID table.
  if (mInterfaceTable) {
    nsIIDKey key(aIID);
    nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                   mInterfaceTable->Get(&key)));
    return supports != nsnull;
  }

  return PR_FALSE;
}

// Internal helpers ///////////////////////////////////////////////////////////////////////

already_AddRefed<nsIContent>
nsXBLPrototypeBinding::GetImmediateChild(nsIAtom* aTag)
{
  PRUint32 childCount = mBinding->GetChildCount();

  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *child = mBinding->GetChildAt(i);

    if (aTag == child->Tag()) {
      NS_ADDREF(child);
      return child;
    }
  }

  return nsnull;
}
 
nsresult
nsXBLPrototypeBinding::InitClass(const nsCString& aClassName,
                                 nsIScriptContext * aContext,
                                 void * aScriptObject, void ** aClassObject)
{
  NS_ENSURE_ARG_POINTER(aClassObject); 

  *aClassObject = nsnull;

  JSContext* cx = (JSContext*)aContext->GetNativeContext();
  JSObject* scriptObject = (JSObject*) aScriptObject;

  return nsXBLBinding::DoInitJSClass(cx, ::JS_GetGlobalObject(cx),
                                     scriptObject, aClassName, aClassObject);
}

already_AddRefed<nsIContent>
nsXBLPrototypeBinding::LocateInstance(nsIContent* aBoundElement,
                                      nsIContent* aTemplRoot,
                                      nsIContent* aCopyRoot, 
                                      nsIContent* aTemplChild)
{
  // XXX We will get in trouble if the binding instantiation deviates from the template
  // in the prototype.
  if (aTemplChild == aTemplRoot || !aTemplChild)
    return nsnull;

  nsCOMPtr<nsIContent> templParent = aTemplChild->GetParent();
  nsCOMPtr<nsIContent> copyParent;
  nsCOMPtr<nsIContent> childPoint;
  
  if (aBoundElement) {
    nsINodeInfo *ni = templParent->GetNodeInfo();

    if (ni->Equals(nsXBLAtoms::children, kNameSpaceID_XBL)) {
      childPoint = templParent;
      templParent = childPoint->GetParent();
    }
  }

  if (!templParent)
    return nsnull;

  nsIContent* result = nsnull;

  if (templParent == aTemplRoot)
    copyParent = aCopyRoot;
  else
    copyParent = LocateInstance(aBoundElement, aTemplRoot, aCopyRoot, templParent);
  
  if (childPoint && aBoundElement) {
    // First we have to locate this insertion point and use its index and its
    // count to detemine our precise position within the template.
    nsIDocument* doc = aBoundElement->GetDocument();
    nsCOMPtr<nsIXBLBinding> binding;
    doc->GetBindingManager()->GetBinding(aBoundElement,
                                         getter_AddRefs(binding));
    
    nsCOMPtr<nsIXBLBinding> currBinding = binding;
    nsCOMPtr<nsIContent> anonContent;
    while (currBinding) {
      currBinding->GetAnonymousContent(getter_AddRefs(anonContent));
      if (anonContent)
        break;
      nsCOMPtr<nsIXBLBinding> tempBinding = currBinding;
      tempBinding->GetBaseBinding(getter_AddRefs(currBinding));
    }

    nsVoidArray* points;
    if (anonContent == copyParent)
      currBinding->GetInsertionPointsFor(aBoundElement, &points);
    else
      currBinding->GetInsertionPointsFor(copyParent, &points);
    PRInt32 count = points->Count();
    for (PRInt32 i = 0; i < count; i++) {
      // Next we have to find the real insertion point for this proto insertion
      // point.  If it does not contain any default content, then we should 
      // return null, since the content is not in the clone.
      nsXBLInsertionPoint* currPoint = NS_STATIC_CAST(nsXBLInsertionPoint*, points->ElementAt(i));
      nsCOMPtr<nsIContent> defContent = currPoint->GetDefaultContentTemplate();
      if (defContent == childPoint) {
        // Now check to see if we even built default content at this
        // insertion point.
        defContent = currPoint->GetDefaultContent();
        if (defContent) {
          // Find out the index of the template element within the <children> elt.
          PRInt32 index = childPoint->IndexOf(aTemplChild);
          
          // Now we just have to find the corresponding elt underneath the cloned
          // default content.
          result = defContent->GetChildAt(index);
        } 
        break;
      }
    }
  }
  else if (copyParent)
  {
    PRInt32 index = templParent->IndexOf(aTemplChild);
    result = copyParent->GetChildAt(index);
  }

  NS_IF_ADDREF(result);

  return result;
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
  nsXBLAttributeEntry* entry = NS_STATIC_CAST(nsXBLAttributeEntry*, aData);
  nsXBLAttrChangeData* changeData = NS_STATIC_CAST(nsXBLAttrChangeData*, aClosure);

  nsIAtom* src = entry->GetSrcAttribute();
  nsAutoString value;
  PRBool attrPresent = PR_TRUE;

  if (src == nsXBLAtoms::xbltext) {
    nsXBLBinding::GetTextData(changeData->mBoundElement, value);
    value.StripChar(PRUnichar('\n'));
    value.StripChar(PRUnichar('\r'));
    nsAutoString stripVal(value);
    stripVal.StripWhitespace();

    if (stripVal.IsEmpty()) 
      attrPresent = PR_FALSE;
  }
  else {
    nsresult result = changeData->mBoundElement->GetAttr(kNameSpaceID_None, src, value);
    attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                   result == NS_CONTENT_ATTR_HAS_VALUE);
  }

  if (attrPresent) {
    nsCOMPtr<nsIContent> content;
    content = changeData->mProto->GetImmediateChild(nsXBLAtoms::content);

    nsXBLAttributeEntry* curr = entry;
    while (curr) {
      nsIAtom* dst = curr->GetDstAttribute();
      nsIContent* element = curr->GetElement();

      nsCOMPtr<nsIContent> realElement;
      realElement = changeData->mProto->LocateInstance(changeData->mBoundElement,
                                                       content,
                                                       changeData->mContent,
                                                       element);
      if (realElement) {
        realElement->SetAttr(kNameSpaceID_None, dst, value, PR_FALSE);

        if (dst == nsXBLAtoms::xbltext ||
            (realElement->GetNodeInfo()->Equals(nsHTMLAtoms::html,
                                                kNameSpaceID_XUL) &&
             dst == nsHTMLAtoms::value && !value.IsEmpty())) {
          nsCOMPtr<nsIDOMText> textNode;
          nsCOMPtr<nsIDOMDocument> domDoc =
            do_QueryInterface(changeData->mBoundElement->GetDocument());
          domDoc->CreateTextNode(value, getter_AddRefs(textNode));
          nsCOMPtr<nsIDOMNode> dummy;
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(realElement));
          domElement->AppendChild(textNode, getter_AddRefs(dummy));
        }
      }

      curr = curr->GetNext();
    }
  }

  return PR_TRUE;
}

void
nsXBLPrototypeBinding::SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent)
{
  if (mAttributeTable) {
    nsXBLAttrChangeData data(this, aBoundElement, aAnonymousContent);
    mAttributeTable->Enumerate(SetAttrs, (void*)&data);
  }
}

nsIStyleRuleProcessor*
nsXBLPrototypeBinding::GetRuleProcessor()
{
  if (mResources) {
    return mResources->mRuleProcessor;
  }
  
  return nsnull;
}

nsCOMArray<nsICSSStyleSheet>*
nsXBLPrototypeBinding::GetStyleSheets()
{
  if (mResources) {
    return &mResources->mStyleSheetList;
  }

  return nsnull;
}

PRBool
nsXBLPrototypeBinding::ShouldBuildChildFrames()
{
  if (mAttributeTable) {
    nsISupportsKey key(nsXBLAtoms::xbltext);
    void* entry = mAttributeTable->Get(&key);
    return !entry;
  }

  return PR_TRUE;
}

static PRBool PR_CALLBACK
DeleteAttributeEntry(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLAttributeEntry::Destroy(NS_STATIC_CAST(nsXBLAttributeEntry*, aData));
  return PR_TRUE;
}

void
nsXBLPrototypeBinding::ConstructAttributeTable(nsIContent* aElement)
{
  nsAutoString inherits;
  aElement->GetAttr(kNameSpaceID_XBL, nsXBLAtoms::inherits, inherits);

  if (!inherits.IsEmpty()) {
    if (!mAttributeTable) {
      mAttributeTable = new nsObjectHashtable(nsnull, nsnull,
                                              DeleteAttributeEntry, nsnull, 4);
      if (!mAttributeTable)
        return;
    }

    // The user specified at least one attribute.
    char* str = ToNewCString(inherits);
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

        atom = do_GetAtom(right);
        attribute = do_GetAtom(left);
      }
      else {
        nsAutoString tok;
        tok.AssignWithConversion(token);
        atom = do_GetAtom(tok);
        attribute = atom;
      }
      
      // Create an XBL attribute entry.
      nsXBLAttributeEntry* xblAttr = nsXBLAttributeEntry::Create(atom, attribute, aElement);

      // Now we should see if some element within our anonymous
      // content is already observing this attribute.
      nsISupportsKey key(atom);
      nsXBLAttributeEntry* entry = NS_STATIC_CAST(nsXBLAttributeEntry*, mAttributeTable->Get(&key));

      if (!entry) {
        // Put it in the table.
        mAttributeTable->Put(&key, xblAttr);
      } else {
        while (entry->GetNext())
          entry = entry->GetNext();

        entry->SetNext(xblAttr);
      }

      // Now remove the inherits attribute from the element so that it doesn't
      // show up on clones of the element.  It is used
      // by the template only, and we don't need it anymore.
      // XXXdwh Don't do this for XUL elements, since it faults them into heavyweight
      // elements. Should nuke from the prototype instead.
      // aElement->UnsetAttr(kNameSpaceID_XBL, nsXBLAtoms::inherits, PR_FALSE);

      token = nsCRT::strtok( newStr, ", ", &newStr );
    }

    nsMemory::Free(str);
  }

  // Recur into our children.
  PRUint32 childCount = aElement->GetChildCount();
  for (PRUint32 i = 0; i < childCount; i++) {
    ConstructAttributeTable(aElement->GetChildAt(i));
  }
}

static PRBool PR_CALLBACK
DeleteInsertionPointEntry(nsHashKey* aKey, void* aData, void* aClosure)
{
  NS_STATIC_CAST(nsXBLInsertionPointEntry*, aData)->Release();
  return PR_TRUE;
}

void 
nsXBLPrototypeBinding::ConstructInsertionTable(nsIContent* aContent)
{
  nsCOMPtr<nsISupportsArray> childrenElements;
  GetNestedChildren(nsXBLAtoms::children, aContent, getter_AddRefs(childrenElements));

  if (!childrenElements)
    return;

  mInsertionPointTable = new nsObjectHashtable(nsnull, nsnull,
                                               DeleteInsertionPointEntry,
                                               nsnull, 4);
  if (!mInsertionPointTable)
    return;

  PRUint32 count;
  childrenElements->Count(&count);
  PRUint32 i;
  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> supp;
    childrenElements->GetElementAt(i, getter_AddRefs(supp));
    nsCOMPtr<nsIContent> child(do_QueryInterface(supp));
    if (child) {
      nsCOMPtr<nsIContent> parent = child->GetParent(); 

      // Create an XBL insertion point entry.
      nsXBLInsertionPointEntry* xblIns = nsXBLInsertionPointEntry::Create(parent);

      nsAutoString includes;
      child->GetAttr(kNameSpaceID_None, nsXBLAtoms::includes, includes);
      if (includes.IsEmpty()) {
        nsISupportsKey key(nsXBLAtoms::children);
        xblIns->AddRef();
        mInsertionPointTable->Put(&key, xblIns);
      }
      else {
        // The user specified at least one attribute.
        char* str = ToNewCString(includes);
        char* newStr;
        // XXX We should use a strtok function that tokenizes PRUnichar's
        // so that we don't have to convert from Unicode to ASCII and then back

        char* token = nsCRT::strtok( str, "| ", &newStr );
        while( token != NULL ) {
          nsAutoString tok;
          tok.AssignWithConversion(token);

          // Build an atom out of this string.
          nsCOMPtr<nsIAtom> atom = do_GetAtom(tok);
           
          nsISupportsKey key(atom);
          xblIns->AddRef();
          mInsertionPointTable->Put(&key, xblIns);
          
          token = nsCRT::strtok( newStr, "| ", &newStr );
        }

        nsMemory::Free(str);
      }

      // Compute the index of the <children> element.  This index is
      // equal to the index of the <children> in the template minus the #
      // of previous insertion point siblings removed.  Because our childrenElements
      // array was built in a DFS that went from left-to-right through siblings,
      // if we dynamically obtain our index each time, then the removals of previous
      // siblings will cause the index to adjust (and we won't have to take that into
      // account explicitly).
      PRInt32 index = parent->IndexOf(child);
      xblIns->SetInsertionIndex((PRUint32)index);

      // Now remove the <children> element from the template.  This ensures that the
      // binding instantiation will not contain a clone of the <children> element when
      // it clones the binding template.
      parent->RemoveChildAt(index, PR_FALSE);

      // See if the insertion point contains default content.  Default content must
      // be cached in our insertion point entry, since it will need to be cloned
      // in situations where no content ends up being placed at the insertion point.
      PRUint32 defaultCount = child->GetChildCount();
      if (defaultCount > 0) {
        // Annotate the insertion point with our default content.
        xblIns->SetDefaultContent(child);

        // Reconnect back to our parent for access later.  This makes "inherits" easier
        // to work with on default content.
        child->SetParent(parent);
      }
    }
  }
}

nsresult
nsXBLPrototypeBinding::ConstructInterfaceTable(const nsAString& aImpls)
{
  if (!aImpls.IsEmpty()) {
    // Obtain the interface info manager that can tell us the IID
    // for a given interface name.
    nsCOMPtr<nsIInterfaceInfoManager> infoManager = getter_AddRefs(XPTI_GetInterfaceInfoManager());
    if (!infoManager)
      return NS_ERROR_FAILURE;

    // Create the table.
    if (!mInterfaceTable)
      mInterfaceTable = new nsSupportsHashtable(4);

    // The user specified at least one attribute.
    NS_ConvertUCS2toUTF8 utf8impl(aImpls);
    char* str = utf8impl.BeginWriting();
    char* newStr;
    // XXX We should use a strtok function that tokenizes PRUnichars
    // so that we don't have to convert from Unicode to ASCII and then back

    char* token = nsCRT::strtok( str, ", ", &newStr );
    while( token != NULL ) {
      // get the InterfaceInfo for the name
      nsCOMPtr<nsIInterfaceInfo> iinfo;
      infoManager->GetInfoForName(token, getter_AddRefs(iinfo));

      if (iinfo) {
        // obtain an IID.
        nsIID* iid = nsnull;
        iinfo->GetInterfaceIID(&iid);

        if (iid) {
          // We found a valid iid.  Add it to our table.
          nsIIDKey key(*iid);
          mInterfaceTable->Put(&key, mBinding);

          // this block adds the parent interfaces of each interface
          // defined in the xbl definition (implements="nsI...")
          nsCOMPtr<nsIInterfaceInfo> parentInfo;
          // if it has a parent, add it to the table
          while (NS_SUCCEEDED(iinfo->GetParent(getter_AddRefs(parentInfo))) && parentInfo) {
            // free the nsMemory::Clone()ed iid
            nsMemory::Free(iid);

            // get the iid
            parentInfo->GetInterfaceIID(&iid);

            // don't add nsISupports to the table
            if (!iid || iid->Equals(NS_GET_IID(nsISupports)))
              break;

            // add the iid to the table
            nsIIDKey parentKey(*iid);
            mInterfaceTable->Put(&parentKey, mBinding);

            // look for the next parent
            iinfo = parentInfo;
          }
        }

        // free the nsMemory::Clone()ed iid
        if (iid)
          nsMemory::Free(iid);
      }

      token = nsCRT::strtok( newStr, ", ", &newStr );
    }
  }

  return NS_OK;
}

void
nsXBLPrototypeBinding::GetNestedChildren(nsIAtom* aTag, nsIContent* aContent,
                                         nsISupportsArray** aList)
{
  PRUint32 childCount = aContent->GetChildCount();

  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *child = aContent->GetChildAt(i);

    if (aTag == child->Tag()) {
      if (!*aList)
        NS_NewISupportsArray(aList); // Addref happens here.
      (*aList)->AppendElement(child);
    }
    else
      GetNestedChildren(aTag, child, aList);
  }
}

nsresult
nsXBLPrototypeBinding::AddResourceListener(nsIContent* aBoundElement)
{
  if (!mResources)
    return NS_ERROR_FAILURE; // Makes no sense to add a listener when the binding
                             // has no resources.

  mResources->AddResourceListener(aBoundElement);
  return NS_OK;
}

void
nsXBLPrototypeBinding::CreateKeyHandlers()
{
  nsXBLPrototypeHandler* curr = mPrototypeHandler;
  while (curr) {
    nsCOMPtr<nsIAtom> eventAtom = curr->GetEventName();
    if (eventAtom == nsXBLAtoms::keyup ||
        eventAtom == nsXBLAtoms::keydown ||
        eventAtom == nsXBLAtoms::keypress) {
      PRUint8 phase = curr->GetPhase();
      PRUint8 type = curr->GetType();

      PRInt32 count = mKeyHandlers.Count();
      PRInt32 i;
      nsXBLKeyEventHandler* handler;
      for (i = 0; i < count; ++i) {
        handler = mKeyHandlers[i];
        if (handler->Matches(eventAtom, phase, type))
          break;
      }

      if (i == count) {
        nsRefPtr<nsXBLKeyEventHandler> newHandler;
        NS_NewXBLKeyEventHandler(eventAtom, phase, type,
                                 getter_AddRefs(newHandler));
        if (newHandler)
          mKeyHandlers.AppendObject(newHandler);
        handler = newHandler;
      }

      if (handler)
        handler->AddProtoHandler(curr);
    }

    curr = curr->GetNextHandler();
  }
}
