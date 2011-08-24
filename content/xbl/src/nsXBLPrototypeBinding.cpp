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
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIDOMEventTarget.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsContentCreatorFunctions.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "nsXMLDocument.h"
#include "nsXBLService.h"
#include "nsXBLBinding.h"
#include "nsXBLInsertionPoint.h"
#include "nsXBLPrototypeBinding.h"
#include "nsFixedSizeAllocator.h"
#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIDocumentObserver.h"
#include "nsGkAtoms.h"
#include "nsXBLProtoImpl.h"
#include "nsCRT.h"
#include "nsContentUtils.h"

#include "nsIScriptContext.h"

#include "nsIStyleRuleProcessor.h"
#include "nsXBLResourceLoader.h"
#include "mozilla/dom/Element.h"

// Helper Classes =====================================================================

// nsXBLAttributeEntry and helpers.  This class is used to efficiently handle
// attribute changes in anonymous content.

class nsXBLAttributeEntry {
public:
  nsIAtom* GetSrcAttribute() { return mSrcAttribute; }
  nsIAtom* GetDstAttribute() { return mDstAttribute; }
  PRInt32 GetDstNameSpace() { return mDstNameSpace; }
  
  nsIContent* GetElement() { return mElement; }

  nsXBLAttributeEntry* GetNext() { return mNext; }
  void SetNext(nsXBLAttributeEntry* aEntry) { mNext = aEntry; }

  static nsXBLAttributeEntry*
  Create(nsIAtom* aSrcAtom, nsIAtom* aDstAtom, PRInt32 aDstNameSpace, nsIContent* aContent) {
    void* place = nsXBLPrototypeBinding::kAttrPool->Alloc(sizeof(nsXBLAttributeEntry));
    return place ? ::new (place) nsXBLAttributeEntry(aSrcAtom, aDstAtom, aDstNameSpace, 
                                                     aContent) : nsnull;
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
  PRInt32 mDstNameSpace;
  nsXBLAttributeEntry* mNext;

  nsXBLAttributeEntry(nsIAtom* aSrcAtom, nsIAtom* aDstAtom, PRInt32 aDstNameSpace,
                      nsIContent* aContent)
    : mElement(aContent),
      mSrcAttribute(aSrcAtom),
      mDstAttribute(aDstAtom),
      mDstNameSpace(aDstNameSpace),
      mNext(nsnull) { }

  ~nsXBLAttributeEntry() {
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLAttributeEntry, this, mNext);
  }

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
  ~nsXBLInsertionPointEntry() {
    if (mDefaultContent) {
      nsAutoScriptBlocker scriptBlocker;
      // mDefaultContent is a sort of anonymous content within the XBL
      // document, and we own and manage it.  Unhook it here, since we're going
      // away.
      mDefaultContent->UnbindFromTree();
    }      
  }

  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(nsXBLInsertionPointEntry)

  nsIContent* GetInsertionParent() { return mInsertionParent; }
  PRUint32 GetInsertionIndex() { return mInsertionIndex; }
  void SetInsertionIndex(PRUint32 aIndex) { mInsertionIndex = aIndex; }

  nsIContent* GetDefaultContent() { return mDefaultContent; }
  void SetDefaultContent(nsIContent* aChildren) { mDefaultContent = aChildren; }


  // We keep kPool alive as long as there is at least either a
  // nsXBLPrototypeBinding or a nsXBLInsertionPointEntry alive.
  // nsXBLPrototypeBinding has its own global refcount so it only adds 1 to
  // nsXBLInsertionPointEntry::gRefCnt as long as there's at least one
  // nsXBLPrototypeBinding alive.

  static void InitPool(PRInt32 aInitialSize)
  {
    if (++gRefCnt == 1) {
      kPool = new nsFixedSizeAllocator();
      if (kPool) {
        static const size_t kBucketSizes[] = {
          sizeof(nsXBLInsertionPointEntry)
        };
        kPool->Init("XBL Insertion Point Entries", kBucketSizes,
                    NS_ARRAY_LENGTH(kBucketSizes), aInitialSize);
      }
    }
  }
  static PRBool PoolInited()
  {
    return kPool != nsnull;
  }
  static void ReleasePool()
  {
    if (--gRefCnt == 0) {
      delete kPool;
    }
  }

  static nsXBLInsertionPointEntry*
  Create(nsIContent* aParent) {
    void* place = kPool->Alloc(sizeof(nsXBLInsertionPointEntry));
    if (!place) {
      return nsnull;
    }
    ++gRefCnt;
    return ::new (place) nsXBLInsertionPointEntry(aParent);
  }

  static void
  Destroy(nsXBLInsertionPointEntry* aSelf) {
    aSelf->~nsXBLInsertionPointEntry();
    kPool->Free(aSelf, sizeof(*aSelf));
    nsXBLInsertionPointEntry::ReleasePool();
  }

  NS_INLINE_DECL_REFCOUNTING(nsXBLInsertionPointEntry)

protected:
  nsCOMPtr<nsIContent> mInsertionParent;
  nsCOMPtr<nsIContent> mDefaultContent;
  PRUint32 mInsertionIndex;

  nsXBLInsertionPointEntry(nsIContent* aParent)
    : mInsertionParent(aParent),
      mInsertionIndex(0) { }

private:
  // Hide so that only Create() and Destroy() can be used to
  // allocate and deallocate from the heap
  static void* operator new(size_t) CPP_THROW_NEW { return 0; }
  static void operator delete(void*, size_t) {}

  static nsFixedSizeAllocator* kPool;
  static PRUint32 gRefCnt;
};

PRUint32 nsXBLInsertionPointEntry::gRefCnt = 0;
nsFixedSizeAllocator* nsXBLInsertionPointEntry::kPool;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXBLInsertionPointEntry)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(nsXBLInsertionPointEntry)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mInsertionParent)
  if (tmp->mDefaultContent) {
    nsAutoScriptBlocker scriptBlocker;
    // mDefaultContent is a sort of anonymous content within the XBL
    // document, and we own and manage it.  Unhook it here, since we're going
    // away.
    tmp->mDefaultContent->UnbindFromTree();
    tmp->mDefaultContent = nsnull;
  }      
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(nsXBLInsertionPointEntry)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mInsertionParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDefaultContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXBLInsertionPointEntry, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXBLInsertionPointEntry, Release)

// =============================================================================

// Static initialization
PRUint32 nsXBLPrototypeBinding::gRefCnt = 0;

nsFixedSizeAllocator* nsXBLPrototypeBinding::kAttrPool;

static const PRInt32 kNumElements = 128;

static const size_t kAttrBucketSizes[] = {
  sizeof(nsXBLAttributeEntry)
};

static const PRInt32 kAttrNumBuckets = sizeof(kAttrBucketSizes)/sizeof(size_t);
static const PRInt32 kAttrInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLAttributeEntry))) * kNumElements;

static const PRInt32 kInsInitialSize = (NS_SIZE_IN_HEAP(sizeof(nsXBLInsertionPointEntry))) * kNumElements;

// Implementation /////////////////////////////////////////////////////////////////

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
    nsXBLInsertionPointEntry::InitPool(kInsInitialSize);
  }
}

nsresult
nsXBLPrototypeBinding::Init(const nsACString& aID,
                            nsXBLDocumentInfo* aInfo,
                            nsIContent* aElement,
                            PRBool aFirstBinding)
{
  if (!kAttrPool || !nsXBLInsertionPointEntry::PoolInited()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = aInfo->DocumentURI()->Clone(getter_AddRefs(mBindingURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // The binding URI might be an immutable URI (e.g. for about: URIs). In that case,
  // we'll fail in SetRef below, but that doesn't matter much for now.
  if (aFirstBinding) {
    rv = mBindingURI->Clone(getter_AddRefs(mAlternateBindingURI));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mBindingURI->SetRef(aID);

  mXBLDocInfoWeak = aInfo;

  SetBindingElement(aElement);
  return NS_OK;
}

PRBool nsXBLPrototypeBinding::CompareBindingURI(nsIURI* aURI) const
{
  PRBool equal = PR_FALSE;
  mBindingURI->Equals(aURI, &equal);
  if (!equal && mAlternateBindingURI) {
    mAlternateBindingURI->Equals(aURI, &equal);
  }
  return equal;
}

static PRBool
TraverseInsertionPoint(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsCycleCollectionTraversalCallback &cb = 
    *static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  nsXBLInsertionPointEntry* entry =
    static_cast<nsXBLInsertionPointEntry*>(aData);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_PTR(entry,
                                               nsXBLInsertionPointEntry,
                                               "[insertion point table] value")
  return kHashEnumerateNext;
}

static PRBool
TraverseBinding(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  cb->NoteXPCOMChild(static_cast<nsISupports*>(aData));
  return kHashEnumerateNext;
}

void
nsXBLPrototypeBinding::Traverse(nsCycleCollectionTraversalCallback &cb) const
{
  cb.NoteXPCOMChild(mBinding);
  if (mResources)
    cb.NoteXPCOMChild(mResources->mLoader);
  if (mInsertionPointTable)
    mInsertionPointTable->Enumerate(TraverseInsertionPoint, &cb);
  if (mInterfaceTable)
    mInterfaceTable->Enumerate(TraverseBinding, &cb);
}

void
nsXBLPrototypeBinding::UnlinkJSObjects()
{
  if (mImplementation)
    mImplementation->UnlinkJSObjects();
}

void
nsXBLPrototypeBinding::Trace(TraceCallback aCallback, void *aClosure) const
{
  if (mImplementation)
    mImplementation->Trace(aCallback, aClosure);
}

void
nsXBLPrototypeBinding::Initialize()
{
  nsIContent* content = GetImmediateChild(nsGkAtoms::content);
  if (content) {
    // Make sure to construct the attribute table first, since constructing the
    // insertion point table removes some of the subtrees, which makes them
    // unreachable by walking our DOM.
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
    nsXBLInsertionPointEntry::ReleasePool();
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
  if (mBinding->AttrValueIs(kNameSpaceID_None, nsGkAtoms::inheritstyle,
                            nsGkAtoms::_false, eCaseMatters))
    mInheritStyle = PR_FALSE;
}

PRBool
nsXBLPrototypeBinding::GetAllowScripts()
{
  return mXBLDocInfoWeak->GetScriptAccess();
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
nsXBLPrototypeBinding::BindingAttached(nsIContent* aBoundElement)
{
  if (mImplementation && mImplementation->CompiledMembers() &&
      mImplementation->mConstructor)
    return mImplementation->mConstructor->Execute(aBoundElement);
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::BindingDetached(nsIContent* aBoundElement)
{
  if (mImplementation && mImplementation->CompiledMembers() &&
      mImplementation->mDestructor)
    return mImplementation->mDestructor->Execute(aBoundElement);
  return NS_OK;
}

nsXBLProtoImplAnonymousMethod*
nsXBLPrototypeBinding::GetConstructor()
{
  if (mImplementation)
    return mImplementation->mConstructor;

  return nsnull;
}

nsXBLProtoImplAnonymousMethod*
nsXBLPrototypeBinding::GetDestructor()
{
  if (mImplementation)
    return mImplementation->mDestructor;

  return nsnull;
}

nsresult
nsXBLPrototypeBinding::SetConstructor(nsXBLProtoImplAnonymousMethod* aMethod)
{
  if (!mImplementation)
    return NS_ERROR_FAILURE;
  mImplementation->mConstructor = aMethod;
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::SetDestructor(nsXBLProtoImplAnonymousMethod* aMethod)
{
  if (!mImplementation)
    return NS_ERROR_FAILURE;
  mImplementation->mDestructor = aMethod;
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::InstallImplementation(nsIContent* aBoundElement)
{
  if (mImplementation)
    return mImplementation->InstallImplementation(this, aBoundElement);
  return NS_OK;
}

// XXXbz this duplicates lots of SetAttrs
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
  nsPRUint32Key nskey(aNameSpaceID);
  nsObjectHashtable *attributesNS = static_cast<nsObjectHashtable*>(mAttributeTable->Get(&nskey));
  if (!attributesNS)
    return;

  nsISupportsKey key(aAttribute);
  nsXBLAttributeEntry* xblAttr = static_cast<nsXBLAttributeEntry*>
                                            (attributesNS->Get(&key));
  if (!xblAttr)
    return;

  // Iterate over the elements in the array.
  nsCOMPtr<nsIContent> content = GetImmediateChild(nsGkAtoms::content);
  while (xblAttr) {
    nsIContent* element = xblAttr->GetElement();

    nsCOMPtr<nsIContent> realElement = LocateInstance(aChangedElement, content,
                                                      aAnonymousContent,
                                                      element);

    if (realElement) {
      // Hold a strong reference here so that the atom doesn't go away during
      // UnsetAttr.
      nsCOMPtr<nsIAtom> dstAttr = xblAttr->GetDstAttribute();
      PRInt32 dstNs = xblAttr->GetDstNameSpace();

      if (aRemoveFlag)
        realElement->UnsetAttr(dstNs, dstAttr, aNotify);
      else {
        PRBool attrPresent = PR_TRUE;
        nsAutoString value;
        // Check to see if the src attribute is xbl:text.  If so, then we need to obtain the 
        // children of the real element and get the text nodes' values.
        if (aAttribute == nsGkAtoms::text && aNameSpaceID == kNameSpaceID_XBL) {
          nsContentUtils::GetNodeTextContent(aChangedElement, PR_FALSE, value);
          value.StripChar(PRUnichar('\n'));
          value.StripChar(PRUnichar('\r'));
          nsAutoString stripVal(value);
          stripVal.StripWhitespace();
          if (stripVal.IsEmpty()) 
            attrPresent = PR_FALSE;
        }    
        else {
          attrPresent = aChangedElement->GetAttr(aNameSpaceID, aAttribute, value);
        }

        if (attrPresent)
          realElement->SetAttr(dstNs, dstAttr, value, aNotify);
      }

      // See if we're the <html> tag in XUL, and see if value is being
      // set or unset on us.  We may also be a tag that is having
      // xbl:text set on us.

      if ((dstAttr == nsGkAtoms::text && dstNs == kNameSpaceID_XBL) ||
          realElement->NodeInfo()->Equals(nsGkAtoms::html,
                                          kNameSpaceID_XUL) &&
          dstAttr == nsGkAtoms::value) {
        // Flush out all our kids.
        PRUint32 childCount = realElement->GetChildCount();
        for (PRUint32 i = 0; i < childCount; i++)
          realElement->RemoveChildAt(0, aNotify);

        if (!aRemoveFlag) {
          // Construct a new text node and insert it.
          nsAutoString value;
          aChangedElement->GetAttr(aNameSpaceID, aAttribute, value);
          if (!value.IsEmpty()) {
            nsCOMPtr<nsIContent> textContent;
            NS_NewTextNode(getter_AddRefs(textContent),
                           realElement->NodeInfo()->NodeInfoManager());
            if (!textContent) {
              continue;
            }

            textContent->SetText(value, PR_TRUE);
            realElement->AppendChildTo(textContent, PR_TRUE);
          }
        }
      }
    }

    xblAttr = xblAttr->GetNext();
  }
}

struct InsertionData {
  nsXBLBinding* mBinding;
  nsXBLPrototypeBinding* mPrototype;

  InsertionData(nsXBLBinding* aBinding,
                nsXBLPrototypeBinding* aPrototype) 
    :mBinding(aBinding), mPrototype(aPrototype) {}
};

PRBool InstantiateInsertionPoint(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLInsertionPointEntry* entry = static_cast<nsXBLInsertionPointEntry*>(aData);
  InsertionData* data = static_cast<InsertionData*>(aClosure);
  nsXBLBinding* binding = data->mBinding;
  nsXBLPrototypeBinding* proto = data->mPrototype;

  // Get the insertion parent.
  nsIContent* content = entry->GetInsertionParent();
  PRUint32 index = entry->GetInsertionIndex();
  nsIContent* defContent = entry->GetDefaultContent();

  // Locate the real content.
  nsIContent *instanceRoot = binding->GetAnonymousContent();
  nsIContent *templRoot = proto->GetImmediateChild(nsGkAtoms::content);
  nsIContent *realContent = proto->LocateInstance(nsnull, templRoot,
                                                  instanceRoot, content);
  if (!realContent)
    realContent = binding->GetBoundElement();

  // Now that we have the real content, look it up in our table.
  nsInsertionPointList* points = nsnull;
  binding->GetInsertionPointsFor(realContent, &points);
  nsXBLInsertionPoint* insertionPoint = nsnull;
  PRInt32 count = points->Length();
  PRInt32 i = 0;
  PRInt32 currIndex = 0;  
  
  for ( ; i < count; i++) {
    nsXBLInsertionPoint* currPoint = points->ElementAt(i);
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
    if (insertionPoint) {
      points->InsertElementAt(i, insertionPoint);
    }
  }

  return PR_TRUE;
}

void
nsXBLPrototypeBinding::InstantiateInsertionPoints(nsXBLBinding* aBinding)
{
  InsertionData data(aBinding, this);
  if (mInsertionPointTable)
    mInsertionPointTable->Enumerate(InstantiateInsertionPoint, &data);
}

nsIContent*
nsXBLPrototypeBinding::GetInsertionPoint(nsIContent* aBoundElement,
                                         nsIContent* aCopyRoot,
                                         const nsIContent* aChild,
                                         PRUint32* aIndex)
{
  if (!mInsertionPointTable)
    return nsnull;

  nsISupportsKey key(aChild->Tag());
  nsXBLInsertionPointEntry* entry = static_cast<nsXBLInsertionPointEntry*>(mInsertionPointTable->Get(&key));
  if (!entry) {
    nsISupportsKey key2(nsGkAtoms::children);
    entry = static_cast<nsXBLInsertionPointEntry*>(mInsertionPointTable->Get(&key2));
  }

  nsIContent *realContent = nsnull;
  if (entry) {
    nsIContent* content = entry->GetInsertionParent();
    *aIndex = entry->GetInsertionIndex();
    nsIContent* templContent = GetImmediateChild(nsGkAtoms::content);
    realContent = LocateInstance(nsnull, templContent, aCopyRoot, content);
  }
  else {
    // We got nothin'.  Bail.
    return nsnull;
  }

  return realContent ? realContent : aBoundElement;
}

nsIContent*
nsXBLPrototypeBinding::GetSingleInsertionPoint(nsIContent* aBoundElement,
                                               nsIContent* aCopyRoot,
                                               PRUint32* aIndex,
                                               PRBool* aMultipleInsertionPoints)
{ 
  *aMultipleInsertionPoints = PR_FALSE;
  *aIndex = 0;

  if (!mInsertionPointTable)
    return nsnull;

  if (mInsertionPointTable->Count() != 1) {
    *aMultipleInsertionPoints = PR_TRUE;
    return nsnull;
  }

  nsISupportsKey key(nsGkAtoms::children);
  nsXBLInsertionPointEntry* entry =
    static_cast<nsXBLInsertionPointEntry*>(mInsertionPointTable->Get(&key));

  if (!entry) {
    // The only insertion point specified was actually a filtered insertion
    // point. This means (strictly speaking) that we actually have multiple
    // insertion points: the filtered one and a generic insertion point
    // (content that doesn't match the filter will just go right underneath the
    // bound element).

    *aMultipleInsertionPoints = PR_TRUE;
    *aIndex = 0;
    return nsnull;
  }

  *aMultipleInsertionPoints = PR_FALSE;
  *aIndex = entry->GetInsertionIndex();

  nsIContent* templContent = GetImmediateChild(nsGkAtoms::content);
  nsIContent* content = entry->GetInsertionParent();
  nsIContent *realContent = LocateInstance(nsnull, templContent, aCopyRoot,
                                           content);

  return realContent ? realContent : aBoundElement;
}

void
nsXBLPrototypeBinding::SetBaseTag(PRInt32 aNamespaceID, nsIAtom* aTag)
{
  mBaseNameSpaceID = aNamespaceID;
  mBaseTag = aTag;
}

nsIAtom*
nsXBLPrototypeBinding::GetBaseTag(PRInt32* aNamespaceID)
{
  if (mBaseTag) {
    *aNamespaceID = mBaseNameSpaceID;
    return mBaseTag;
  }

  return nsnull;
}

PRBool
nsXBLPrototypeBinding::ImplementsInterface(REFNSIID aIID) const
{
  // Check our IID table.
  if (mInterfaceTable) {
    nsIIDKey key(aIID);
    nsCOMPtr<nsISupports> supports = getter_AddRefs(static_cast<nsISupports*>(mInterfaceTable->Get(&key)));
    return supports != nsnull;
  }

  return PR_FALSE;
}

// Internal helpers ///////////////////////////////////////////////////////////////////////

nsIContent*
nsXBLPrototypeBinding::GetImmediateChild(nsIAtom* aTag)
{
  PRUint32 childCount = mBinding->GetChildCount();

  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent* child = mBinding->GetChildAt(i);
    if (child->NodeInfo()->Equals(aTag, kNameSpaceID_XBL)) {
      return child;
    }
  }

  return nsnull;
}
 
nsresult
nsXBLPrototypeBinding::InitClass(const nsCString& aClassName,
                                 JSContext * aContext, JSObject * aGlobal,
                                 JSObject * aScriptObject,
                                 void ** aClassObject)
{
  NS_ENSURE_ARG_POINTER(aClassObject); 

  *aClassObject = nsnull;

  return nsXBLBinding::DoInitJSClass(aContext, aGlobal, aScriptObject,
                                     aClassName, this, aClassObject);
}

nsIContent*
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
  nsCOMPtr<nsIContent> childPoint;

  // We may be disconnected from our parent during cycle collection.
  if (!templParent)
    return nsnull;
  
  if (aBoundElement) {
    if (templParent->NodeInfo()->Equals(nsGkAtoms::children,
                                        kNameSpaceID_XBL)) {
      childPoint = templParent;
      templParent = childPoint->GetParent();
    }
  }

  if (!templParent)
    return nsnull;

  nsIContent* result = nsnull;
  nsIContent *copyParent;

  if (templParent == aTemplRoot)
    copyParent = aCopyRoot;
  else
    copyParent = LocateInstance(aBoundElement, aTemplRoot, aCopyRoot, templParent);
  
  if (childPoint && aBoundElement) {
    // First we have to locate this insertion point and use its index and its
    // count to detemine our precise position within the template.
    nsIDocument* doc = aBoundElement->GetOwnerDoc();
    nsXBLBinding *binding = doc->BindingManager()->GetBinding(aBoundElement);
    nsIContent *anonContent = nsnull;

    while (binding) {
      anonContent = binding->GetAnonymousContent();
      if (anonContent)
        break;

      binding = binding->GetBaseBinding();
    }

    NS_ABORT_IF_FALSE(binding, "Bug 620181 this is unexpected");
    if (!binding)
      return nsnull;

    nsInsertionPointList* points = nsnull;
    if (anonContent == copyParent)
      binding->GetInsertionPointsFor(aBoundElement, &points);
    else
      binding->GetInsertionPointsFor(copyParent, &points);
    PRInt32 count = points->Length();
    for (PRInt32 i = 0; i < count; i++) {
      // Next we have to find the real insertion point for this proto insertion
      // point.  If it does not contain any default content, then we should 
      // return null, since the content is not in the clone.
      nsXBLInsertionPoint* currPoint = static_cast<nsXBLInsertionPoint*>(points->ElementAt(i));
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

  return result;
}

struct nsXBLAttrChangeData
{
  nsXBLPrototypeBinding* mProto;
  nsIContent* mBoundElement;
  nsIContent* mContent;
  PRInt32 mSrcNamespace;

  nsXBLAttrChangeData(nsXBLPrototypeBinding* aProto,
                      nsIContent* aElt, nsIContent* aContent) 
  :mProto(aProto), mBoundElement(aElt), mContent(aContent) {}
};

// XXXbz this duplicates lots of AttributeChanged
PRBool SetAttrs(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLAttributeEntry* entry = static_cast<nsXBLAttributeEntry*>(aData);
  nsXBLAttrChangeData* changeData = static_cast<nsXBLAttrChangeData*>(aClosure);

  nsIAtom* src = entry->GetSrcAttribute();
  PRInt32 srcNs = changeData->mSrcNamespace;
  nsAutoString value;
  PRBool attrPresent = PR_TRUE;

  if (src == nsGkAtoms::text && srcNs == kNameSpaceID_XBL) {
    nsContentUtils::GetNodeTextContent(changeData->mBoundElement, PR_FALSE,
                                       value);
    value.StripChar(PRUnichar('\n'));
    value.StripChar(PRUnichar('\r'));
    nsAutoString stripVal(value);
    stripVal.StripWhitespace();

    if (stripVal.IsEmpty()) 
      attrPresent = PR_FALSE;
  }
  else {
    attrPresent = changeData->mBoundElement->GetAttr(srcNs, src, value);
  }

  if (attrPresent) {
    nsIContent* content =
      changeData->mProto->GetImmediateChild(nsGkAtoms::content);

    nsXBLAttributeEntry* curr = entry;
    while (curr) {
      nsIAtom* dst = curr->GetDstAttribute();
      PRInt32 dstNs = curr->GetDstNameSpace();
      nsIContent* element = curr->GetElement();

      nsIContent *realElement =
        changeData->mProto->LocateInstance(changeData->mBoundElement, content,
                                           changeData->mContent, element);

      if (realElement) {
        realElement->SetAttr(dstNs, dst, value, PR_FALSE);

        if ((dst == nsGkAtoms::text && dstNs == kNameSpaceID_XBL) ||
            (realElement->NodeInfo()->Equals(nsGkAtoms::html,
                                             kNameSpaceID_XUL) &&
             dst == nsGkAtoms::value && !value.IsEmpty())) {

          nsCOMPtr<nsIContent> textContent;
          NS_NewTextNode(getter_AddRefs(textContent),
                         realElement->NodeInfo()->NodeInfoManager());
          if (!textContent) {
            continue;
          }

          textContent->SetText(value, PR_FALSE);
          realElement->AppendChildTo(textContent, PR_FALSE);
        }
      }

      curr = curr->GetNext();
    }
  }

  return PR_TRUE;
}

PRBool SetAttrsNS(nsHashKey* aKey, void* aData, void* aClosure)
{
  if (aData && aClosure) {
    nsPRUint32Key * key = static_cast<nsPRUint32Key*>(aKey);
    nsObjectHashtable* xblAttributes =
      static_cast<nsObjectHashtable*>(aData);
    nsXBLAttrChangeData * changeData = static_cast<nsXBLAttrChangeData *>
                                                  (aClosure);
    changeData->mSrcNamespace = key->GetValue();
    xblAttributes->Enumerate(SetAttrs, (void*)changeData);
  }
  return PR_TRUE;
}

void
nsXBLPrototypeBinding::SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent)
{
  if (mAttributeTable) {
    nsXBLAttrChangeData data(this, aBoundElement, aAnonymousContent);
    mAttributeTable->Enumerate(SetAttrsNS, (void*)&data);
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

nsXBLPrototypeResources::sheet_array_type*
nsXBLPrototypeBinding::GetStyleSheets()
{
  if (mResources) {
    return &mResources->mStyleSheetList;
  }

  return nsnull;
}

static PRBool
DeleteAttributeEntry(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLAttributeEntry::Destroy(static_cast<nsXBLAttributeEntry*>(aData));
  return PR_TRUE;
}

static PRBool
DeleteAttributeTable(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete static_cast<nsObjectHashtable*>(aData);
  return PR_TRUE;
}

void
nsXBLPrototypeBinding::ConstructAttributeTable(nsIContent* aElement)
{
  // Don't add entries for <children> elements, since those will get
  // removed from the DOM when we construct the insertion point table.
  if (!aElement->NodeInfo()->Equals(nsGkAtoms::children, kNameSpaceID_XBL)) {
    nsAutoString inherits;
    aElement->GetAttr(kNameSpaceID_XBL, nsGkAtoms::inherits, inherits);

    if (!inherits.IsEmpty()) {
      if (!mAttributeTable) {
        mAttributeTable = new nsObjectHashtable(nsnull, nsnull,
                                                DeleteAttributeTable,
                                                nsnull, 4);
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
        PRInt32 atomNsID = kNameSpaceID_None;
        nsCOMPtr<nsIAtom> attribute;
        PRInt32 attributeNsID = kNameSpaceID_None;

        // Figure out if this token contains a :.
        nsAutoString attrTok; attrTok.AssignWithConversion(token);
        PRInt32 index = attrTok.Find("=", PR_TRUE);
        nsresult rv;
        if (index != -1) {
          // This attribute maps to something different.
          nsAutoString left, right;
          attrTok.Left(left, index);
          attrTok.Right(right, attrTok.Length()-index-1);

          rv = nsContentUtils::SplitQName(aElement, left, &attributeNsID,
                                          getter_AddRefs(attribute));
          if (NS_FAILED(rv))
            return;

          rv = nsContentUtils::SplitQName(aElement, right, &atomNsID,
                                          getter_AddRefs(atom));
          if (NS_FAILED(rv))
            return;
        }
        else {
          nsAutoString tok;
          tok.AssignWithConversion(token);
          rv = nsContentUtils::SplitQName(aElement, tok, &atomNsID, 
                                          getter_AddRefs(atom));
          if (NS_FAILED(rv))
            return;
          attribute = atom;
          attributeNsID = atomNsID;
        }

        nsPRUint32Key nskey(atomNsID);
        nsObjectHashtable* attributesNS =
          static_cast<nsObjectHashtable*>(mAttributeTable->Get(&nskey));
        if (!attributesNS) {
          attributesNS = new nsObjectHashtable(nsnull, nsnull,
                                               DeleteAttributeEntry,
                                               nsnull, 4);
          if (!attributesNS)
            return;

          mAttributeTable->Put(&nskey, attributesNS);
        }
      
        // Create an XBL attribute entry.
        nsXBLAttributeEntry* xblAttr =
          nsXBLAttributeEntry::Create(atom, attribute, attributeNsID, aElement);

        // Now we should see if some element within our anonymous
        // content is already observing this attribute.
        nsISupportsKey key(atom);
        nsXBLAttributeEntry* entry = static_cast<nsXBLAttributeEntry*>
                                                (attributesNS->Get(&key));

        if (!entry) {
          // Put it in the table.
          attributesNS->Put(&key, xblAttr);
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
        // aElement->UnsetAttr(kNameSpaceID_XBL, nsGkAtoms::inherits, PR_FALSE);

        token = nsCRT::strtok( newStr, ", ", &newStr );
      }

      nsMemory::Free(str);
    }
  }

  // Recur into our children.
  PRUint32 childCount = aElement->GetChildCount();
  for (PRUint32 i = 0; i < childCount; i++) {
    ConstructAttributeTable(aElement->GetChildAt(i));
  }
}

static PRBool
DeleteInsertionPointEntry(nsHashKey* aKey, void* aData, void* aClosure)
{
  static_cast<nsXBLInsertionPointEntry*>(aData)->Release();
  return PR_TRUE;
}

void 
nsXBLPrototypeBinding::ConstructInsertionTable(nsIContent* aContent)
{
  nsCOMArray<nsIContent> childrenElements;
  GetNestedChildren(nsGkAtoms::children, kNameSpaceID_XBL, aContent,
                    childrenElements);

  PRInt32 count = childrenElements.Count();
  if (count == 0)
    return;

  mInsertionPointTable = new nsObjectHashtable(nsnull, nsnull,
                                               DeleteInsertionPointEntry,
                                               nsnull, 4);
  if (!mInsertionPointTable)
    return;

  PRInt32 i;
  for (i = 0; i < count; i++) {
    nsIContent* child = childrenElements[i];
    nsCOMPtr<nsIContent> parent = child->GetParent(); 

    // Create an XBL insertion point entry.
    nsXBLInsertionPointEntry* xblIns = nsXBLInsertionPointEntry::Create(parent);

    nsAutoString includes;
    child->GetAttr(kNameSpaceID_None, nsGkAtoms::includes, includes);
    if (includes.IsEmpty()) {
      nsISupportsKey key(nsGkAtoms::children);
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
      nsAutoScriptBlocker scriptBlocker;
      // Annotate the insertion point with our default content.
      xblIns->SetDefaultContent(child);

      // Reconnect back to our parent for access later.  This makes "inherits" easier
      // to work with on default content.
      // XXXbz this is somewhat screwed up, since it's sort of like anonymous
      // content... but not.
      nsresult rv =
        child->BindToTree(parent->GetCurrentDoc(), parent, nsnull, PR_FALSE);
      if (NS_FAILED(rv)) {
        // Well... now what?  Just unbind and bail out, I guess...
        // XXXbz This really shouldn't be a void method!
        child->UnbindFromTree();
        return;
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
    nsCOMPtr<nsIInterfaceInfoManager>
      infoManager(do_GetService(NS_INTERFACEINFOMANAGER_SERVICE_CONTRACTID));
    if (!infoManager)
      return NS_ERROR_FAILURE;

    // Create the table.
    if (!mInterfaceTable)
      mInterfaceTable = new nsSupportsHashtable(4);

    // The user specified at least one attribute.
    NS_ConvertUTF16toUTF8 utf8impl(aImpls);
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
        const nsIID* iid = nsnull;
        iinfo->GetIIDShared(&iid);

        if (iid) {
          // We found a valid iid.  Add it to our table.
          nsIIDKey key(*iid);
          mInterfaceTable->Put(&key, mBinding);

          // this block adds the parent interfaces of each interface
          // defined in the xbl definition (implements="nsI...")
          nsCOMPtr<nsIInterfaceInfo> parentInfo;
          // if it has a parent, add it to the table
          while (NS_SUCCEEDED(iinfo->GetParent(getter_AddRefs(parentInfo))) && parentInfo) {
            // get the iid
            parentInfo->GetIIDShared(&iid);

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
      }

      token = nsCRT::strtok( newStr, ", ", &newStr );
    }
  }

  return NS_OK;
}

void
nsXBLPrototypeBinding::GetNestedChildren(nsIAtom* aTag, PRInt32 aNamespace,
                                         nsIContent* aContent,
                                         nsCOMArray<nsIContent> & aList)
{
  PRUint32 childCount = aContent->GetChildCount();

  for (PRUint32 i = 0; i < childCount; i++) {
    nsIContent *child = aContent->GetChildAt(i);

    if (child->NodeInfo()->Equals(aTag, aNamespace)) {
      aList.AppendObject(child);
    }
    else
      GetNestedChildren(aTag, aNamespace, child, aList);
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
    if (eventAtom == nsGkAtoms::keyup ||
        eventAtom == nsGkAtoms::keydown ||
        eventAtom == nsGkAtoms::keypress) {
      PRUint8 phase = curr->GetPhase();
      PRUint8 type = curr->GetType();

      PRInt32 count = mKeyHandlers.Count();
      PRInt32 i;
      nsXBLKeyEventHandler* handler = nsnull;
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
