/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsContentCreatorFunctions.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsContentCID.h"
#include "mozilla/dom/XMLDocument.h"
#include "nsXBLService.h"
#include "nsXBLBinding.h"
#include "nsXBLInsertionPoint.h"
#include "nsXBLPrototypeBinding.h"
#include "nsXBLContentSink.h"
#include "xptinfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIDocumentObserver.h"
#include "nsGkAtoms.h"
#include "nsXBLProtoImpl.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsTextFragment.h"
#include "nsTextNode.h"

#include "nsIScriptContext.h"
#include "nsIScriptError.h"

#include "nsIStyleRuleProcessor.h"
#include "nsXBLResourceLoader.h"
#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/Element.h"

#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

// Helper Classes =====================================================================

// Internal helper class for managing our IID table.
class nsIIDKey : public nsHashKey {
  public:
    nsIID mKey;

  public:
    nsIIDKey(REFNSIID key) : mKey(key) {}
    ~nsIIDKey(void) {}

    uint32_t HashCode(void) const {
      // Just use the 32-bit m0 field.
      return mKey.m0;
    }

    bool Equals(const nsHashKey *aKey) const {
      return mKey.Equals( ((nsIIDKey*) aKey)->mKey);
    }

    nsHashKey *Clone(void) const {
      return new nsIIDKey(mKey);
    }
};

// nsXBLAttributeEntry and helpers.  This class is used to efficiently handle
// attribute changes in anonymous content.

class nsXBLAttributeEntry {
public:
  nsXBLAttributeEntry(nsIAtom* aSrcAtom, nsIAtom* aDstAtom,
                      int32_t aDstNameSpace, nsIContent* aContent)
    : mElement(aContent),
      mSrcAttribute(aSrcAtom),
      mDstAttribute(aDstAtom),
      mDstNameSpace(aDstNameSpace),
      mNext(nullptr) { }

  ~nsXBLAttributeEntry() {
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLAttributeEntry, this, mNext);
  }

  nsIAtom* GetSrcAttribute() { return mSrcAttribute; }
  nsIAtom* GetDstAttribute() { return mDstAttribute; }
  int32_t GetDstNameSpace() { return mDstNameSpace; }

  nsIContent* GetElement() { return mElement; }

  nsXBLAttributeEntry* GetNext() { return mNext; }
  void SetNext(nsXBLAttributeEntry* aEntry) { mNext = aEntry; }

protected:
  nsIContent* mElement;

  nsCOMPtr<nsIAtom> mSrcAttribute;
  nsCOMPtr<nsIAtom> mDstAttribute;
  int32_t mDstNameSpace;
  nsXBLAttributeEntry* mNext;
};

// nsXBLInsertionPointEntry and helpers.  This class stores all the necessary
// info to figure out the position of an insertion point.
// The same insertion point may be in the insertion point table for multiple
// keys, so we refcount the entries.

class nsXBLInsertionPointEntry {
public:
  nsXBLInsertionPointEntry(nsIContent* aParent)
    : mInsertionParent(aParent),
      mInsertionIndex(0)
  {}

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
  uint32_t GetInsertionIndex() { return mInsertionIndex; }
  void SetInsertionIndex(uint32_t aIndex) { mInsertionIndex = aIndex; }

  nsIContent* GetDefaultContent() { return mDefaultContent; }
  void SetDefaultContent(nsIContent* aChildren) { mDefaultContent = aChildren; }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(nsXBLInsertionPointEntry)

protected:
  nsCOMPtr<nsIContent> mInsertionParent;
  nsCOMPtr<nsIContent> mDefaultContent;
  uint32_t mInsertionIndex;
};

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXBLInsertionPointEntry)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mInsertionParent)
  if (tmp->mDefaultContent) {
    nsAutoScriptBlocker scriptBlocker;
    // mDefaultContent is a sort of anonymous content within the XBL
    // document, and we own and manage it.  Unhook it here, since we're going
    // away.
    tmp->mDefaultContent->UnbindFromTree();
    tmp->mDefaultContent = nullptr;
  }      
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXBLInsertionPointEntry)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mInsertionParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDefaultContent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsXBLInsertionPointEntry, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsXBLInsertionPointEntry, Release)

// =============================================================================

// Implementation /////////////////////////////////////////////////////////////////

// Constructors/Destructors
nsXBLPrototypeBinding::nsXBLPrototypeBinding()
: mImplementation(nullptr),
  mBaseBinding(nullptr),
  mInheritStyle(true),
  mCheckedBaseProto(false),
  mKeyHandlersRegistered(false),
  mChromeOnlyContent(false),
  mResources(nullptr),
  mAttributeTable(nullptr),
  mInsertionPointTable(nullptr),
  mInterfaceTable(nullptr),
  mBaseNameSpaceID(kNameSpaceID_None)
{
  MOZ_COUNT_CTOR(nsXBLPrototypeBinding);
}

nsresult
nsXBLPrototypeBinding::Init(const nsACString& aID,
                            nsXBLDocumentInfo* aInfo,
                            nsIContent* aElement,
                            bool aFirstBinding)
{
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

  // aElement will be null when reading from the cache, but the element will
  // still be set later.
  if (aElement) {
    SetBindingElement(aElement);
  }
  return NS_OK;
}

bool nsXBLPrototypeBinding::CompareBindingURI(nsIURI* aURI) const
{
  bool equal = false;
  mBindingURI->Equals(aURI, &equal);
  if (!equal && mAlternateBindingURI) {
    mAlternateBindingURI->Equals(aURI, &equal);
  }
  return equal;
}

static bool
TraverseInsertionPoint(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsCycleCollectionTraversalCallback &cb = 
    *static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  nsXBLInsertionPointEntry* entry =
    static_cast<nsXBLInsertionPointEntry*>(aData);
  CycleCollectionNoteChild(cb, entry, "[insertion point table] value");
  return kHashEnumerateNext;
}

static bool
TraverseBinding(nsHashKey *aKey, void *aData, void* aClosure)
{
  nsCycleCollectionTraversalCallback *cb = 
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME((*cb), "proto mInterfaceTable data");
  cb->NoteXPCOMChild(static_cast<nsISupports*>(aData));
  return kHashEnumerateNext;
}

void
nsXBLPrototypeBinding::Traverse(nsCycleCollectionTraversalCallback &cb) const
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "proto mBinding");
  cb.NoteXPCOMChild(mBinding);
  if (mResources) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "proto mResources mLoader");
    cb.NoteXPCOMChild(mResources->mLoader);
  }
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
nsXBLPrototypeBinding::Trace(const TraceCallbacks& aCallbacks, void *aClosure) const
{
  if (mImplementation)
    mImplementation->Trace(aCallbacks, aClosure);
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
  nsCOMPtr<nsIContent> result = mBinding;
  return result.forget();
}

void
nsXBLPrototypeBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  if (mBinding->AttrValueIs(kNameSpaceID_None, nsGkAtoms::inheritstyle,
                            nsGkAtoms::_false, eCaseMatters))
    mInheritStyle = false;

  mChromeOnlyContent = IsChrome() &&
                       mBinding->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::chromeOnlyContent,
                                             nsGkAtoms::_true, eCaseMatters);
}

bool
nsXBLPrototypeBinding::GetAllowScripts()
{
  return mXBLDocInfoWeak->GetScriptAccess();
}

bool
nsXBLPrototypeBinding::LoadResources()
{
  if (mResources) {
    bool result;
    mResources->LoadResources(&result);
    return result;
  }

  return true;
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

  return nullptr;
}

nsXBLProtoImplAnonymousMethod*
nsXBLPrototypeBinding::GetDestructor()
{
  if (mImplementation)
    return mImplementation->mDestructor;

  return nullptr;
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
nsXBLPrototypeBinding::InstallImplementation(nsXBLBinding* aBinding)
{
  if (mImplementation)
    return mImplementation->InstallImplementation(this, aBinding);
  return NS_OK;
}

// XXXbz this duplicates lots of SetAttrs
void
nsXBLPrototypeBinding::AttributeChanged(nsIAtom* aAttribute,
                                        int32_t aNameSpaceID,
                                        bool aRemoveFlag, 
                                        nsIContent* aChangedElement,
                                        nsIContent* aAnonymousContent,
                                        bool aNotify)
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
      int32_t dstNs = xblAttr->GetDstNameSpace();

      if (aRemoveFlag)
        realElement->UnsetAttr(dstNs, dstAttr, aNotify);
      else {
        bool attrPresent = true;
        nsAutoString value;
        // Check to see if the src attribute is xbl:text.  If so, then we need to obtain the 
        // children of the real element and get the text nodes' values.
        if (aAttribute == nsGkAtoms::text && aNameSpaceID == kNameSpaceID_XBL) {
          nsContentUtils::GetNodeTextContent(aChangedElement, false, value);
          value.StripChar(PRUnichar('\n'));
          value.StripChar(PRUnichar('\r'));
          nsAutoString stripVal(value);
          stripVal.StripWhitespace();
          if (stripVal.IsEmpty()) 
            attrPresent = false;
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
          (realElement->NodeInfo()->Equals(nsGkAtoms::html,
                                           kNameSpaceID_XUL) &&
           dstAttr == nsGkAtoms::value)) {
        // Flush out all our kids.
        uint32_t childCount = realElement->GetChildCount();
        for (uint32_t i = 0; i < childCount; i++)
          realElement->RemoveChildAt(0, aNotify);

        if (!aRemoveFlag) {
          // Construct a new text node and insert it.
          nsAutoString value;
          aChangedElement->GetAttr(aNameSpaceID, aAttribute, value);
          if (!value.IsEmpty()) {
            nsRefPtr<nsTextNode> textContent =
              new nsTextNode(realElement->NodeInfo()->NodeInfoManager());

            textContent->SetText(value, true);
            realElement->AppendChildTo(textContent, true);
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

bool InstantiateInsertionPoint(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLInsertionPointEntry* entry = static_cast<nsXBLInsertionPointEntry*>(aData);
  InsertionData* data = static_cast<InsertionData*>(aClosure);
  nsXBLBinding* binding = data->mBinding;
  nsXBLPrototypeBinding* proto = data->mPrototype;

  // Get the insertion parent.
  nsIContent* content = entry->GetInsertionParent();
  uint32_t index = entry->GetInsertionIndex();
  nsIContent* defContent = entry->GetDefaultContent();

  // Locate the real content.
  nsIContent *instanceRoot = binding->GetAnonymousContent();
  nsIContent *templRoot = proto->GetImmediateChild(nsGkAtoms::content);
  nsIContent *realContent = proto->LocateInstance(nullptr, templRoot,
                                                  instanceRoot, content);
  if (!realContent)
    realContent = binding->GetBoundElement();

  // Now that we have the real content, look it up in our table.
  nsInsertionPointList* points = nullptr;
  binding->GetInsertionPointsFor(realContent, &points);
  nsXBLInsertionPoint* insertionPoint = nullptr;
  int32_t count = points->Length();
  int32_t i = 0;
  int32_t currIndex = 0;  
  
  for ( ; i < count; i++) {
    nsXBLInsertionPoint* currPoint = points->ElementAt(i);
    currIndex = currPoint->GetInsertionIndex();
    if (currIndex == (int32_t)index) {
      // This is a match. Break out of the loop and set our variable.
      insertionPoint = currPoint;
      break;
    }
    
    if (currIndex > (int32_t)index)
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

  return true;
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
                                         uint32_t* aIndex)
{
  if (!mInsertionPointTable)
    return nullptr;

  nsISupportsKey key(aChild->Tag());
  nsXBLInsertionPointEntry* entry = static_cast<nsXBLInsertionPointEntry*>(mInsertionPointTable->Get(&key));
  if (!entry) {
    nsISupportsKey key2(nsGkAtoms::children);
    entry = static_cast<nsXBLInsertionPointEntry*>(mInsertionPointTable->Get(&key2));
  }

  nsIContent *realContent = nullptr;
  if (entry) {
    nsIContent* content = entry->GetInsertionParent();
    *aIndex = entry->GetInsertionIndex();
    nsIContent* templContent = GetImmediateChild(nsGkAtoms::content);
    realContent = LocateInstance(nullptr, templContent, aCopyRoot, content);
  }
  else {
    // We got nothin'.  Bail.
    return nullptr;
  }

  return realContent ? realContent : aBoundElement;
}

nsIContent*
nsXBLPrototypeBinding::GetSingleInsertionPoint(nsIContent* aBoundElement,
                                               nsIContent* aCopyRoot,
                                               uint32_t* aIndex,
                                               bool* aMultipleInsertionPoints)
{ 
  *aMultipleInsertionPoints = false;
  *aIndex = 0;

  if (!mInsertionPointTable)
    return nullptr;

  if (mInsertionPointTable->Count() != 1) {
    *aMultipleInsertionPoints = true;
    return nullptr;
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

    *aMultipleInsertionPoints = true;
    *aIndex = 0;
    return nullptr;
  }

  *aMultipleInsertionPoints = false;
  *aIndex = entry->GetInsertionIndex();

  nsIContent* templContent = GetImmediateChild(nsGkAtoms::content);
  nsIContent* content = entry->GetInsertionParent();
  nsIContent *realContent = LocateInstance(nullptr, templContent, aCopyRoot,
                                           content);

  return realContent ? realContent : aBoundElement;
}

void
nsXBLPrototypeBinding::SetBaseTag(int32_t aNamespaceID, nsIAtom* aTag)
{
  mBaseNameSpaceID = aNamespaceID;
  mBaseTag = aTag;
}

nsIAtom*
nsXBLPrototypeBinding::GetBaseTag(int32_t* aNamespaceID)
{
  if (mBaseTag) {
    *aNamespaceID = mBaseNameSpaceID;
    return mBaseTag;
  }

  return nullptr;
}

bool
nsXBLPrototypeBinding::ImplementsInterface(REFNSIID aIID) const
{
  // Check our IID table.
  if (mInterfaceTable) {
    nsIIDKey key(aIID);
    nsCOMPtr<nsISupports> supports = getter_AddRefs(static_cast<nsISupports*>(mInterfaceTable->Get(&key)));
    return supports != nullptr;
  }

  return false;
}

// Internal helpers ///////////////////////////////////////////////////////////////////////

nsIContent*
nsXBLPrototypeBinding::GetImmediateChild(nsIAtom* aTag)
{
  for (nsIContent* child = mBinding->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    if (child->NodeInfo()->Equals(aTag, kNameSpaceID_XBL)) {
      return child;
    }
  }

  return nullptr;
}
 
nsresult
nsXBLPrototypeBinding::InitClass(const nsCString& aClassName,
                                 JSContext * aContext,
                                 JS::Handle<JSObject*> aGlobal,
                                 JS::Handle<JSObject*> aScriptObject,
                                 JS::MutableHandle<JSObject*> aClassObject,
                                 bool* aNew)
{
  return nsXBLBinding::DoInitJSClass(aContext, aGlobal, aScriptObject,
                                     aClassName, this, aClassObject, aNew);
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
    return nullptr;

  nsCOMPtr<nsIContent> templParent = aTemplChild->GetParent();
  nsCOMPtr<nsIContent> childPoint;

  // We may be disconnected from our parent during cycle collection.
  if (!templParent)
    return nullptr;
  
  if (aBoundElement) {
    if (templParent->NodeInfo()->Equals(nsGkAtoms::children,
                                        kNameSpaceID_XBL)) {
      childPoint = templParent;
      templParent = childPoint->GetParent();
    }
  }

  if (!templParent)
    return nullptr;

  nsIContent* result = nullptr;
  nsIContent *copyParent;

  if (templParent == aTemplRoot)
    copyParent = aCopyRoot;
  else
    copyParent = LocateInstance(aBoundElement, aTemplRoot, aCopyRoot, templParent);
  
  if (childPoint && aBoundElement) {
    // First we have to locate this insertion point and use its index and its
    // count to detemine our precise position within the template.
    nsIDocument* doc = aBoundElement->OwnerDoc();
    nsXBLBinding *binding = doc->BindingManager()->GetBinding(aBoundElement);
    nsIContent *anonContent = nullptr;

    while (binding) {
      anonContent = binding->GetAnonymousContent();
      if (anonContent)
        break;

      binding = binding->GetBaseBinding();
    }

    NS_ABORT_IF_FALSE(binding, "Bug 620181 this is unexpected");
    if (!binding)
      return nullptr;

    nsInsertionPointList* points = nullptr;
    if (anonContent == copyParent)
      binding->GetInsertionPointsFor(aBoundElement, &points);
    else
      binding->GetInsertionPointsFor(copyParent, &points);
    int32_t count = points->Length();
    for (int32_t i = 0; i < count; i++) {
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
          int32_t index = childPoint->IndexOf(aTemplChild);
          
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
    int32_t index = templParent->IndexOf(aTemplChild);
    result = copyParent->GetChildAt(index);
  }

  return result;
}

struct nsXBLAttrChangeData
{
  nsXBLPrototypeBinding* mProto;
  nsIContent* mBoundElement;
  nsIContent* mContent;
  int32_t mSrcNamespace;

  nsXBLAttrChangeData(nsXBLPrototypeBinding* aProto,
                      nsIContent* aElt, nsIContent* aContent) 
  :mProto(aProto), mBoundElement(aElt), mContent(aContent) {}
};

// XXXbz this duplicates lots of AttributeChanged
bool SetAttrs(nsHashKey* aKey, void* aData, void* aClosure)
{
  nsXBLAttributeEntry* entry = static_cast<nsXBLAttributeEntry*>(aData);
  nsXBLAttrChangeData* changeData = static_cast<nsXBLAttrChangeData*>(aClosure);

  nsIAtom* src = entry->GetSrcAttribute();
  int32_t srcNs = changeData->mSrcNamespace;
  nsAutoString value;
  bool attrPresent = true;

  if (src == nsGkAtoms::text && srcNs == kNameSpaceID_XBL) {
    nsContentUtils::GetNodeTextContent(changeData->mBoundElement, false,
                                       value);
    value.StripChar(PRUnichar('\n'));
    value.StripChar(PRUnichar('\r'));
    nsAutoString stripVal(value);
    stripVal.StripWhitespace();

    if (stripVal.IsEmpty()) 
      attrPresent = false;
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
      int32_t dstNs = curr->GetDstNameSpace();
      nsIContent* element = curr->GetElement();

      nsIContent *realElement =
        changeData->mProto->LocateInstance(changeData->mBoundElement, content,
                                           changeData->mContent, element);

      if (realElement) {
        realElement->SetAttr(dstNs, dst, value, false);

        // XXXndeakin shouldn't this be done in lieu of SetAttr?
        if ((dst == nsGkAtoms::text && dstNs == kNameSpaceID_XBL) ||
            (realElement->NodeInfo()->Equals(nsGkAtoms::html,
                                             kNameSpaceID_XUL) &&
             dst == nsGkAtoms::value && !value.IsEmpty())) {

          nsRefPtr<nsTextNode> textContent =
            new nsTextNode(realElement->NodeInfo()->NodeInfoManager());

          textContent->SetText(value, false);
          realElement->AppendChildTo(textContent, false);
        }
      }

      curr = curr->GetNext();
    }
  }

  return true;
}

bool SetAttrsNS(nsHashKey* aKey, void* aData, void* aClosure)
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
  return true;
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
  
  return nullptr;
}

nsXBLPrototypeResources::sheet_array_type*
nsXBLPrototypeBinding::GetStyleSheets()
{
  if (mResources) {
    return &mResources->mStyleSheetList;
  }

  return nullptr;
}

static bool
DeleteAttributeEntry(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete static_cast<nsXBLAttributeEntry*>(aData);
  return true;
}

static bool
DeleteAttributeTable(nsHashKey* aKey, void* aData, void* aClosure)
{
  delete static_cast<nsObjectHashtable*>(aData);
  return true;
}

void
nsXBLPrototypeBinding::EnsureAttributeTable()
{
  if (!mAttributeTable) {
    mAttributeTable = new nsObjectHashtable(nullptr, nullptr,
                                            DeleteAttributeTable,
                                            nullptr, 4);
  }
}

void
nsXBLPrototypeBinding::AddToAttributeTable(int32_t aSourceNamespaceID, nsIAtom* aSourceTag,
                                           int32_t aDestNamespaceID, nsIAtom* aDestTag,
                                           nsIContent* aContent)
{
    nsPRUint32Key nskey(aSourceNamespaceID);
    nsObjectHashtable* attributesNS =
      static_cast<nsObjectHashtable*>(mAttributeTable->Get(&nskey));
    if (!attributesNS) {
      attributesNS = new nsObjectHashtable(nullptr, nullptr,
                                           DeleteAttributeEntry,
                                           nullptr, 4);
      mAttributeTable->Put(&nskey, attributesNS);
    }

    nsXBLAttributeEntry* xblAttr =
      new nsXBLAttributeEntry(aSourceTag, aDestTag, aDestNamespaceID, aContent);

    nsISupportsKey key(aSourceTag);
    nsXBLAttributeEntry* entry = static_cast<nsXBLAttributeEntry*>
                                            (attributesNS->Get(&key));
    if (!entry) {
      attributesNS->Put(&key, xblAttr);
    } else {
      while (entry->GetNext())
        entry = entry->GetNext();
      entry->SetNext(xblAttr);
    }
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
      EnsureAttributeTable();

      // The user specified at least one attribute.
      char* str = ToNewCString(inherits);
      char* newStr;
      // XXX We should use a strtok function that tokenizes PRUnichars
      // so that we don't have to convert from Unicode to ASCII and then back

      char* token = nsCRT::strtok( str, ", ", &newStr );
      while( token != nullptr ) {
        // Build an atom out of this attribute.
        nsCOMPtr<nsIAtom> atom;
        int32_t atomNsID = kNameSpaceID_None;
        nsCOMPtr<nsIAtom> attribute;
        int32_t attributeNsID = kNameSpaceID_None;

        // Figure out if this token contains a :.
        nsAutoString attrTok; attrTok.AssignWithConversion(token);
        int32_t index = attrTok.Find("=", true);
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

        AddToAttributeTable(atomNsID, atom, attributeNsID, attribute, aElement);

        // Now remove the inherits attribute from the element so that it doesn't
        // show up on clones of the element.  It is used
        // by the template only, and we don't need it anymore.
        // XXXdwh Don't do this for XUL elements, since it faults them into heavyweight
        // elements. Should nuke from the prototype instead.
        // aElement->UnsetAttr(kNameSpaceID_XBL, nsGkAtoms::inherits, false);

        token = nsCRT::strtok( newStr, ", ", &newStr );
      }

      nsMemory::Free(str);
    }
  }

  // Recur into our children.
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    ConstructAttributeTable(child);
  }
}

static bool
DeleteInsertionPointEntry(nsHashKey* aKey, void* aData, void* aClosure)
{
  static_cast<nsXBLInsertionPointEntry*>(aData)->Release();
  return true;
}

void 
nsXBLPrototypeBinding::ConstructInsertionTable(nsIContent* aContent)
{
  nsCOMArray<nsIContent> childrenElements;
  GetNestedChildren(nsGkAtoms::children, kNameSpaceID_XBL, aContent,
                    childrenElements);

  int32_t count = childrenElements.Count();
  if (count == 0)
    return;

  mInsertionPointTable = new nsObjectHashtable(nullptr, nullptr,
                                               DeleteInsertionPointEntry,
                                               nullptr, 4);
  if (!mInsertionPointTable)
    return;

  int32_t i;
  for (i = 0; i < count; i++) {
    nsIContent* child = childrenElements[i];
    nsCOMPtr<nsIContent> parent = child->GetParent();

    // Create an XBL insertion point entry.
    nsXBLInsertionPointEntry* xblIns = new nsXBLInsertionPointEntry(parent);

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
      while( token != nullptr ) {
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
    int32_t index = parent->IndexOf(child);
    xblIns->SetInsertionIndex((uint32_t)index);

    // Now remove the <children> element from the template.  This ensures that the
    // binding instantiation will not contain a clone of the <children> element when
    // it clones the binding template.
    parent->RemoveChildAt(index, false);

    // See if the insertion point contains default content.  Default content must
    // be cached in our insertion point entry, since it will need to be cloned
    // in situations where no content ends up being placed at the insertion point.
    uint32_t defaultCount = child->GetChildCount();
    if (defaultCount > 0) {
      nsAutoScriptBlocker scriptBlocker;
      // Annotate the insertion point with our default content.
      xblIns->SetDefaultContent(child);

      // Reconnect back to our parent for access later.  This makes "inherits" easier
      // to work with on default content.
      // XXXbz this is somewhat screwed up, since it's sort of like anonymous
      // content... but not.
      nsresult rv =
        child->BindToTree(parent->GetCurrentDoc(), parent,
                          parent->GetBindingParent(), false);
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
    while( token != nullptr ) {
      // get the InterfaceInfo for the name
      nsCOMPtr<nsIInterfaceInfo> iinfo;
      infoManager->GetInfoForName(token, getter_AddRefs(iinfo));

      if (iinfo) {
        // obtain an IID.
        const nsIID* iid = nullptr;
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
nsXBLPrototypeBinding::GetNestedChildren(nsIAtom* aTag, int32_t aNamespace,
                                         nsIContent* aContent,
                                         nsCOMArray<nsIContent> & aList)
{
  for (nsIContent* child = aContent->GetFirstChild();
       child;
       child = child->GetNextSibling()) {

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
      uint8_t phase = curr->GetPhase();
      uint8_t type = curr->GetType();

      int32_t count = mKeyHandlers.Count();
      int32_t i;
      nsXBLKeyEventHandler* handler = nullptr;
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

class XBLPrototypeSetupCleanup
{
public:
  XBLPrototypeSetupCleanup(nsXBLDocumentInfo* aDocInfo, const nsACString& aID)
  : mDocInfo(aDocInfo), mID(aID) {}

  ~XBLPrototypeSetupCleanup()
  {
    if (mDocInfo) {
      mDocInfo->RemovePrototypeBinding(mID);
    }
  }

  void Disconnect()
  {
    mDocInfo = nullptr;
  }

  nsXBLDocumentInfo* mDocInfo;
  nsAutoCString mID;
};

nsresult
nsXBLPrototypeBinding::Read(nsIObjectInputStream* aStream,
                            nsXBLDocumentInfo* aDocInfo,
                            nsIDocument* aDocument,
                            uint8_t aFlags)
{
  mInheritStyle = (aFlags & XBLBinding_Serialize_InheritStyle) ? true : false;
  mChromeOnlyContent =
    (aFlags & XBLBinding_Serialize_ChromeOnlyContent) ? true : false;

  // nsXBLContentSink::ConstructBinding doesn't create a binding with an empty
  // id, so we don't here either.
  nsAutoCString id;
  nsresult rv = aStream->ReadCString(id);

  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(!id.IsEmpty(), NS_ERROR_FAILURE);

  nsAutoCString baseBindingURI;
  rv = aStream->ReadCString(baseBindingURI);
  NS_ENSURE_SUCCESS(rv, rv);
  mCheckedBaseProto = true;

  if (!baseBindingURI.IsEmpty()) {
    rv = NS_NewURI(getter_AddRefs(mBaseBindingURI), baseBindingURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = ReadNamespace(aStream, mBaseNameSpaceID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString baseTag;
  rv = aStream->ReadString(baseTag);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!baseTag.IsEmpty()) {
    mBaseTag = do_GetAtom(baseTag);
  }

  aDocument->CreateElem(NS_LITERAL_STRING("binding"), nullptr, kNameSpaceID_XBL,
                        getter_AddRefs(mBinding));

  nsCOMPtr<nsIContent> child;
  rv = ReadContentNode(aStream, aDocument, aDocument->NodeInfoManager(), getter_AddRefs(child));
  NS_ENSURE_SUCCESS(rv, rv);

  Element* rootElement = aDocument->GetRootElement();
  if (rootElement)
    rootElement->AppendChildTo(mBinding, false);

  if (child) {
    mBinding->AppendChildTo(child, false);
  }

  uint32_t interfaceCount;
  rv = aStream->Read32(&interfaceCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (interfaceCount > 0) {
    NS_ASSERTION(!mInterfaceTable, "non-null mInterfaceTable");
    mInterfaceTable = new nsSupportsHashtable(interfaceCount);
    NS_ENSURE_TRUE(mInterfaceTable, NS_ERROR_OUT_OF_MEMORY);

    for (; interfaceCount > 0; interfaceCount--) {
      nsIID iid;
      aStream->ReadID(&iid);
      nsIIDKey key(iid);
      mInterfaceTable->Put(&key, mBinding);
    }
  }

  nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner(do_QueryObject(aDocInfo));
  nsIScriptGlobalObject* globalObject = globalOwner->GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, NS_ERROR_UNEXPECTED);

  nsIScriptContext *context = globalObject->GetContext();
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

  bool isFirstBinding = aFlags & XBLBinding_Serialize_IsFirstBinding;
  rv = Init(id, aDocInfo, nullptr, isFirstBinding);
  NS_ENSURE_SUCCESS(rv, rv);

  // We need to set the prototype binding before reading the nsXBLProtoImpl,
  // as it may be retrieved within.
  rv = aDocInfo->SetPrototypeBinding(id, this);
  NS_ENSURE_SUCCESS(rv, rv);

  XBLPrototypeSetupCleanup cleanup(aDocInfo, id);  

  nsAutoCString className;
  rv = aStream->ReadCString(className);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!className.IsEmpty()) {
    nsXBLProtoImpl* impl; // NS_NewXBLProtoImpl will set mImplementation for us
    NS_NewXBLProtoImpl(this, NS_ConvertUTF8toUTF16(className).get(), &impl);

    // This needs to happen after SetPrototypeBinding as calls are made to
    // retrieve the mapped bindings from within here. However, if an error
    // occurs, the mapping should be removed again so that we don't keep an
    // invalid binding around.
    rv = mImplementation->Read(context, aStream, this, globalObject);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Next read in the handlers.
  nsXBLPrototypeHandler* previousHandler = nullptr;

  do {
    XBLBindingSerializeDetails type;
    rv = aStream->Read8(&type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type == XBLBinding_Serialize_NoMoreItems)
      break;

    NS_ASSERTION((type & XBLBinding_Serialize_Mask) == XBLBinding_Serialize_Handler,
                 "invalid handler type");

    nsXBLPrototypeHandler* handler = new nsXBLPrototypeHandler(this);
    rv = handler->Read(context, aStream);
    if (NS_FAILED(rv)) {
      delete handler;
      return rv;
    }

    if (previousHandler) {
      previousHandler->SetNextHandler(handler);
    }
    else {
      SetPrototypeHandlers(handler);
    }
    previousHandler = handler;
  } while (1);

  // Finally, read in the resources.
  do {
    XBLBindingSerializeDetails type;
    rv = aStream->Read8(&type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type == XBLBinding_Serialize_NoMoreItems)
      break;

    NS_ASSERTION((type & XBLBinding_Serialize_Mask) == XBLBinding_Serialize_Stylesheet ||
                 (type & XBLBinding_Serialize_Mask) == XBLBinding_Serialize_Image, "invalid resource type");

    nsAutoString src;
    rv = aStream->ReadString(src);
    NS_ENSURE_SUCCESS(rv, rv);

    AddResource(type == XBLBinding_Serialize_Stylesheet ? nsGkAtoms::stylesheet :
                                                          nsGkAtoms::image, src);
  } while (1);

  if (isFirstBinding) {
    aDocInfo->SetFirstPrototypeBinding(this);
  }

  cleanup.Disconnect();
  return NS_OK;
}

static
bool
GatherInsertionPoints(nsHashKey *aKey, void *aData, void* aClosure)
{
  ArrayOfInsertionPointsByContent* insertionPointsByContent =
    static_cast<ArrayOfInsertionPointsByContent *>(aClosure);

  nsXBLInsertionPointEntry* entry = static_cast<nsXBLInsertionPointEntry *>(aData);

  // Add a new blank array if it isn't already there.
  nsAutoTArray<InsertionItem, 1>* list;
  if (!insertionPointsByContent->Get(entry->GetInsertionParent(), &list)) {
    list = new nsAutoTArray<InsertionItem, 1>;
    insertionPointsByContent->Put(entry->GetInsertionParent(), list);
  }

  // Add the item to the array and ensure it stays sorted by insertion index.
  nsIAtom* atom = static_cast<nsIAtom *>(
                    static_cast<nsISupportsKey *>(aKey)->GetValue());
  InsertionItem newitem(entry->GetInsertionIndex(), atom, entry->GetDefaultContent());
  list->InsertElementSorted(newitem);

  return kHashEnumerateNext;
}

static
bool
WriteInterfaceID(nsHashKey *aKey, void *aData, void* aClosure)
{
  // We can just write out the ids. The cache will be invalidated when a
  // different build is used, so we don't need to worry about ids changing.
  nsID iid = ((nsIIDKey *)aKey)->mKey;
  static_cast<nsIObjectOutputStream *>(aClosure)->WriteID(iid);
  return kHashEnumerateNext;
}

nsresult
nsXBLPrototypeBinding::Write(nsIObjectOutputStream* aStream)
{
  // This writes out the binding. Note that mCheckedBaseProto,
  // mKeyHandlersRegistered and mKeyHandlers are not serialized as they are
  // computed on demand.

  nsCOMPtr<nsIScriptGlobalObjectOwner> globalOwner(do_QueryObject(mXBLDocInfoWeak));
  nsIScriptGlobalObject* globalObject = globalOwner->GetScriptGlobalObject();
  NS_ENSURE_TRUE(globalObject, NS_ERROR_UNEXPECTED);

  nsIScriptContext *context = globalObject->GetContext();
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

  uint8_t flags = mInheritStyle ? XBLBinding_Serialize_InheritStyle : 0;

  // mAlternateBindingURI is only set on the first binding.
  if (mAlternateBindingURI) {
    flags |= XBLBinding_Serialize_IsFirstBinding;
  }

  if (mChromeOnlyContent) {
    flags |= XBLBinding_Serialize_ChromeOnlyContent;
  }

  nsresult rv = aStream->Write8(flags);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString id;
  mBindingURI->GetRef(id);
  rv = aStream->WriteStringZ(id.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // write out the extends and display attribute values
  nsAutoCString extends;
  ResolveBaseBinding();
  if (mBaseBindingURI)
    mBaseBindingURI->GetSpec(extends);

  rv = aStream->WriteStringZ(extends.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = WriteNamespace(aStream, mBaseNameSpaceID);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString baseTag;
  if (mBaseTag) {
    mBaseTag->ToString(baseTag);
  }
  rv = aStream->WriteWStringZ(baseTag.get());
  NS_ENSURE_SUCCESS(rv, rv);

  // The binding holds insertions points keyed by the tag that is going to be
  // inserted, however, the cache would prefer to know insertion points keyed
  // by where they are in the content hierarchy. GatherInsertionPoints is used
  // to iterate over the insertion points and store them temporarily in this
  // latter hashtable.
  ArrayOfInsertionPointsByContent insertionPointsByContent;
  insertionPointsByContent.Init();
  if (mInsertionPointTable) {
    mInsertionPointTable->Enumerate(GatherInsertionPoints, &insertionPointsByContent);
  }

  nsIContent* content = GetImmediateChild(nsGkAtoms::content);
  if (content) {
    rv = WriteContentNode(aStream, content, insertionPointsByContent);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Write a marker to indicate that there is no content.
    rv = aStream->Write8(XBLBinding_Serialize_NoContent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Enumerate and write out the implemented interfaces.
  if (mInterfaceTable) {
    rv = aStream->Write32(mInterfaceTable->Count());
    NS_ENSURE_SUCCESS(rv, rv);

    mInterfaceTable->Enumerate(WriteInterfaceID, aStream);
  }
  else {
    rv = aStream->Write32(0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out the implementation details.
  if (mImplementation) {
    rv = mImplementation->Write(context, aStream, this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Write out an empty classname. This indicates that the binding does not
    // define an implementation.
    rv = aStream->WriteWStringZ(EmptyString().get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out the handlers.
  nsXBLPrototypeHandler* handler = mPrototypeHandler;
  while (handler) {
    rv = handler->Write(context, aStream);
    NS_ENSURE_SUCCESS(rv, rv);

    handler = handler->GetNextHandler();
  }

  aStream->Write8(XBLBinding_Serialize_NoMoreItems);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write out the resources
  if (mResources) {
    rv = mResources->Write(aStream);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out an end mark at the end.
  return aStream->Write8(XBLBinding_Serialize_NoMoreItems);
}

nsresult
nsXBLPrototypeBinding::ReadContentNode(nsIObjectInputStream* aStream,
                                       nsIDocument* aDocument,
                                       nsNodeInfoManager* aNim,
                                       nsIContent** aContent)
{
  *aContent = nullptr;

  int32_t namespaceID;
  nsresult rv = ReadNamespace(aStream, namespaceID);
  NS_ENSURE_SUCCESS(rv, rv);

  // There is no content to read so just return.
  if (namespaceID == XBLBinding_Serialize_NoContent)
    return NS_OK;

  nsCOMPtr<nsIContent> content;

  // If this is a text type, just read the string and return.
  if (namespaceID == XBLBinding_Serialize_TextNode ||
      namespaceID == XBLBinding_Serialize_CDATANode ||
      namespaceID == XBLBinding_Serialize_CommentNode) {
    switch (namespaceID) {
      case XBLBinding_Serialize_TextNode:
        content = new nsTextNode(aNim);
        break;
      case XBLBinding_Serialize_CDATANode:
        content = new CDATASection(aNim);
        break;
      case XBLBinding_Serialize_CommentNode:
        content = new Comment(aNim);
        break;
      default:
        break;
    }

    nsAutoString text;
    rv = aStream->ReadString(text);
    NS_ENSURE_SUCCESS(rv, rv);

    content->SetText(text, false);
    content.swap(*aContent);
    return NS_OK;
  }

  // Otherwise, it's an element, so read its tag, attributes and children.
  nsAutoString prefix, tag;
  rv = aStream->ReadString(prefix);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAtom> prefixAtom;
  if (!prefix.IsEmpty())
    prefixAtom = do_GetAtom(prefix);

  rv = aStream->ReadString(tag);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAtom> tagAtom = do_GetAtom(tag);
  nsCOMPtr<nsINodeInfo> nodeInfo =
    aNim->GetNodeInfo(tagAtom, prefixAtom, namespaceID, nsIDOMNode::ELEMENT_NODE);

  uint32_t attrCount;
  rv = aStream->Read32(&attrCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create XUL prototype elements, or regular elements for other namespaces.
  // This needs to match the code in nsXBLContentSink::CreateElement.
#ifdef MOZ_XUL
  if (namespaceID == kNameSpaceID_XUL) {
    nsIURI* documentURI = aDocument->GetDocumentURI();

    nsRefPtr<nsXULPrototypeElement> prototype = new nsXULPrototypeElement();
    NS_ENSURE_TRUE(prototype, NS_ERROR_OUT_OF_MEMORY);

    prototype->mNodeInfo = nodeInfo;

    nsXULPrototypeAttribute* attrs = nullptr;
    if (attrCount > 0) {
      attrs = new nsXULPrototypeAttribute[attrCount];
    }

    prototype->mAttributes = attrs;
    prototype->mNumAttributes = attrCount;

    for (uint32_t i = 0; i < attrCount; i++) {
      rv = ReadNamespace(aStream, namespaceID);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString prefix, name, val;
      rv = aStream->ReadString(prefix);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aStream->ReadString(name);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aStream->ReadString(val);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(name);
      if (namespaceID == kNameSpaceID_None) {
        attrs[i].mName.SetTo(nameAtom);
      }
      else {
        nsCOMPtr<nsIAtom> prefixAtom;
        if (!prefix.IsEmpty())
          prefixAtom = do_GetAtom(prefix);

        nsCOMPtr<nsINodeInfo> ni =
          aNim->GetNodeInfo(nameAtom, prefixAtom,
                            namespaceID, nsIDOMNode::ATTRIBUTE_NODE);
        attrs[i].mName.SetTo(ni);
      }

      rv = prototype->SetAttrAt(i, val, documentURI);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<Element> result;
    nsresult rv =
      nsXULElement::Create(prototype, aDocument, false, false, getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);
    content = result;
  }
  else {
#endif
    NS_NewElement(getter_AddRefs(content), nodeInfo.forget(), NOT_FROM_PARSER);

    for (uint32_t i = 0; i < attrCount; i++) {
      rv = ReadNamespace(aStream, namespaceID);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString prefix, name, val;
      rv = aStream->ReadString(prefix);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aStream->ReadString(name);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aStream->ReadString(val);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIAtom> prefixAtom;
      if (!prefix.IsEmpty())
        prefixAtom = do_GetAtom(prefix);

      nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(name);
      content->SetAttr(namespaceID, nameAtom, prefixAtom, val, false);
    }

#ifdef MOZ_XUL
  }
#endif

  // Now read the attribute forwarding entries (xbl:inherits)

  int32_t srcNamespaceID, destNamespaceID;
  rv = ReadNamespace(aStream, srcNamespaceID);
  NS_ENSURE_SUCCESS(rv, rv);

  while (srcNamespaceID != XBLBinding_Serialize_NoMoreAttributes) {
    nsAutoString srcAttribute, destAttribute;
    rv = aStream->ReadString(srcAttribute);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = ReadNamespace(aStream, destNamespaceID);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = aStream->ReadString(destAttribute);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAtom> srcAtom = do_GetAtom(srcAttribute);
    nsCOMPtr<nsIAtom> destAtom = do_GetAtom(destAttribute);

    EnsureAttributeTable();
    AddToAttributeTable(srcNamespaceID, srcAtom, destNamespaceID, destAtom, content);

    rv = ReadNamespace(aStream, srcNamespaceID);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Next, read the insertion points.
  uint32_t insertionPointIndex;
  rv = aStream->Read32(&insertionPointIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  while (insertionPointIndex != XBLBinding_Serialize_NoMoreInsertionPoints) {
    nsRefPtr<nsXBLInsertionPointEntry> xblIns =
      new nsXBLInsertionPointEntry(content);
    xblIns->SetInsertionIndex(insertionPointIndex);

    // If the insertion point has default content, read it.
    nsCOMPtr<nsIContent> defaultContent;
    rv = ReadContentNode(aStream, aDocument, aNim, getter_AddRefs(defaultContent));
    NS_ENSURE_SUCCESS(rv, rv);

    if (defaultContent) {
      xblIns->SetDefaultContent(defaultContent);

      rv = defaultContent->BindToTree(nullptr, content,
                                      content->GetBindingParent(), false);
      if (NS_FAILED(rv)) {
        defaultContent->UnbindFromTree();
        return rv;
      }
    }

    if (!mInsertionPointTable) {
      mInsertionPointTable = new nsObjectHashtable(nullptr, nullptr,
                                                   DeleteInsertionPointEntry,
                                                   nullptr, 4);
    }

    // Now read in the tags that can be inserted at this point, which is
    // specified by the syntax includes="menupopup|panel", and add them to
    // the hash.
    uint32_t count;
    rv = aStream->Read32(&count);
    NS_ENSURE_SUCCESS(rv, rv);

    for (; count > 0; count --) {
      aStream->ReadString(tag);
      nsCOMPtr<nsIAtom> tagAtom = do_GetAtom(tag);

      nsISupportsKey key(tagAtom);
      NS_ADDREF(xblIns.get());
      mInsertionPointTable->Put(&key, xblIns);
    }

    rv = aStream->Read32(&insertionPointIndex);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Finally, read in the child nodes.
  uint32_t childCount;
  rv = aStream->Read32(&childCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (uint32_t i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    ReadContentNode(aStream, aDocument, aNim, getter_AddRefs(child));

    // Child may be null if this was a comment for example and can just be ignored.
    if (child) {
      content->AppendChildTo(child, false);
    }
  }

  content.swap(*aContent);
  return NS_OK;
}

// This structure holds information about a forwarded attribute that needs to be
// written out. This is used because we need several fields passed within the
// enumeration closure.
struct WriteAttributeData
{
  nsXBLPrototypeBinding* binding;
  nsIObjectOutputStream* stream;
  nsIContent* content;
  int32_t srcNamespace;

  WriteAttributeData(nsXBLPrototypeBinding* aBinding,
                     nsIObjectOutputStream* aStream,
                     nsIContent* aContent)
    : binding(aBinding), stream(aStream), content(aContent)
  { }
};

static
bool
WriteAttribute(nsHashKey *aKey, void *aData, void* aClosure)
{
  WriteAttributeData* data = static_cast<WriteAttributeData *>(aClosure);
  nsIObjectOutputStream* stream = data->stream;
  const int32_t srcNamespace = data->srcNamespace;

  nsXBLAttributeEntry* entry = static_cast<nsXBLAttributeEntry *>(aData);
  do {
    if (entry->GetElement() == data->content) {
      data->binding->WriteNamespace(stream, srcNamespace);
      stream->WriteWStringZ(nsDependentAtomString(entry->GetSrcAttribute()).get());
      data->binding->WriteNamespace(stream, entry->GetDstNameSpace());
      stream->WriteWStringZ(nsDependentAtomString(entry->GetDstAttribute()).get());
    }

    entry = entry->GetNext();
  } while (entry);

  return kHashEnumerateNext;
}

// WriteAttributeNS is the callback to enumerate over the attribute
// forwarding entries. Since these are stored in a hash of hashes,
// we need to iterate over the inner hashes, calling WriteAttribute
// to do the actual work.
static
bool
WriteAttributeNS(nsHashKey *aKey, void *aData, void* aClosure)
{
  WriteAttributeData* data = static_cast<WriteAttributeData *>(aClosure);
  data->srcNamespace = static_cast<nsPRUint32Key *>(aKey)->GetValue();

  nsObjectHashtable* attributes = static_cast<nsObjectHashtable*>(aData);
  attributes->Enumerate(WriteAttribute, data);

  return kHashEnumerateNext;
}

nsresult
nsXBLPrototypeBinding::WriteContentNode(nsIObjectOutputStream* aStream,
                                        nsIContent* aNode,
                                        ArrayOfInsertionPointsByContent& aInsertionPointsByContent)
{
  nsresult rv;

  if (!aNode->IsElement()) {
    // Text is writen out as a single byte for the type, followed by the text.
    uint8_t type = XBLBinding_Serialize_NoContent;
    switch (aNode->NodeType()) {
      case nsIDOMNode::TEXT_NODE:
        type = XBLBinding_Serialize_TextNode;
        break;
      case nsIDOMNode::CDATA_SECTION_NODE:
        type = XBLBinding_Serialize_CDATANode;
        break;
      case nsIDOMNode::COMMENT_NODE:
        type = XBLBinding_Serialize_CommentNode;
        break;
      default:
        break;
    }

    rv = aStream->Write8(type);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString content;
    aNode->GetText()->AppendTo(content);
    return aStream->WriteWStringZ(content.get());
  }

  // Otherwise, this is an element.

  // Write the namespace id followed by the tag name
  rv = WriteNamespace(aStream, aNode->GetNameSpaceID());
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString prefixStr;
  aNode->NodeInfo()->GetPrefix(prefixStr);
  rv = aStream->WriteWStringZ(prefixStr.get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aStream->WriteWStringZ(nsDependentAtomString(aNode->Tag()).get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Write attributes
  uint32_t count = aNode->GetAttrCount();
  rv = aStream->Write32(count);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t i;
  for (i = 0; i < count; i++) {
    // Write out the namespace id, the namespace prefix, the local tag name,
    // and the value, in that order.

    const nsAttrName* attr = aNode->GetAttrNameAt(i);

    // XXXndeakin don't write out xbl:inherits?
    int32_t namespaceID = attr->NamespaceID();
    rv = WriteNamespace(aStream, namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString prefixStr;
    nsIAtom* prefix = attr->GetPrefix();
    if (prefix)
      prefix->ToString(prefixStr);
    rv = aStream->WriteWStringZ(prefixStr.get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(nsDependentAtomString(attr->LocalName()).get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString val;
    aNode->GetAttr(attr->NamespaceID(), attr->LocalName(), val);
    rv = aStream->WriteWStringZ(val.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out the attribute fowarding information
  if (mAttributeTable) {
    WriteAttributeData data(this, aStream, aNode);
    mAttributeTable->Enumerate(WriteAttributeNS, &data);
  }
  rv = aStream->Write8(XBLBinding_Serialize_NoMoreAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  // Write out the insertion points.
  nsAutoTArray<InsertionItem, 1>* list;
  if (aInsertionPointsByContent.Get(aNode, &list)) {
    uint32_t lastInsertionIndex = 0xFFFFFFFF;

    // Iterate over the insertion points and see if they match this point
    // in the content tree by comparing insertion indices.
    for (uint32_t l = 0; l < list->Length(); l++) {
      InsertionItem item = list->ElementAt(l);
      // The list is sorted by insertionIndex so all items pertaining to
      // this point will be in the list in order. We only need to write out the
      // default content and the number of tags once for each index.
      if (item.insertionIndex != lastInsertionIndex) {
        lastInsertionIndex = item.insertionIndex;
        aStream->Write32(item.insertionIndex);
        // If the insertion point has default content, write that out, or
        // write out XBLBinding_Serialize_NoContent if there isn't any.
        if (item.defaultContent) {
          rv = WriteContentNode(aStream, item.defaultContent,
                                aInsertionPointsByContent);
        }
        else {
          rv = aStream->Write8(XBLBinding_Serialize_NoContent);
        }
        NS_ENSURE_SUCCESS(rv, rv);

        // Determine how many insertion points share the same index, so that
        // the total count can be written out.
        uint32_t icount = 1;
        for (uint32_t i = l + 1; i < list->Length(); i++) {
          if (list->ElementAt(i).insertionIndex != lastInsertionIndex)
            break;
          icount++;
        }

        rv = aStream->Write32(icount);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = aStream->WriteWStringZ(nsDependentAtomString(list->ElementAt(l).tag).get());
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Write out an end marker after the insertion points. If there weren't
  // any, this will be all that is written out.
  rv = aStream->Write32(XBLBinding_Serialize_NoMoreInsertionPoints);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, write out the child nodes.
  count = aNode->GetChildCount();
  rv = aStream->Write32(count);
  NS_ENSURE_SUCCESS(rv, rv);

  for (i = 0; i < count; i++) {
    rv = WriteContentNode(aStream, aNode->GetChildAt(i), aInsertionPointsByContent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::ReadNamespace(nsIObjectInputStream* aStream,
                                     int32_t& aNameSpaceID)
{
  uint8_t namespaceID;
  nsresult rv = aStream->Read8(&namespaceID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (namespaceID == XBLBinding_Serialize_CustomNamespace) {
    nsAutoString namesp;
    rv = aStream->ReadString(namesp);
    NS_ENSURE_SUCCESS(rv, rv);
  
    nsContentUtils::NameSpaceManager()->RegisterNameSpace(namesp, aNameSpaceID);
  }
  else {
    aNameSpaceID = namespaceID;
  }

  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::WriteNamespace(nsIObjectOutputStream* aStream,
                                      int32_t aNameSpaceID)
{
  // Namespaces are stored as a single byte id for well-known namespaces.
  // This saves time and space as other namespaces aren't very common in
  // XBL. If another namespace is used however, the namespace id will be
  // XBLBinding_Serialize_CustomNamespace and the string namespace written
  // out directly afterwards.
  nsresult rv;

  if (aNameSpaceID <= kNameSpaceID_LastBuiltin) {
    rv = aStream->Write8((int8_t)aNameSpaceID);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    rv = aStream->Write8(XBLBinding_Serialize_CustomNamespace);
    NS_ENSURE_SUCCESS(rv, rv);
  
    nsAutoString namesp;
    nsContentUtils::NameSpaceManager()->GetNameSpaceURI(aNameSpaceID, namesp);
    aStream->WriteWStringZ(namesp.get());
  }

  return NS_OK;
}


bool CheckTagNameWhiteList(int32_t aNameSpaceID, nsIAtom *aTagName)
{
  static nsIContent::AttrValuesArray kValidXULTagNames[] =  {
    &nsGkAtoms::autorepeatbutton, &nsGkAtoms::box, &nsGkAtoms::browser,
    &nsGkAtoms::button, &nsGkAtoms::hbox, &nsGkAtoms::image, &nsGkAtoms::menu,
    &nsGkAtoms::menubar, &nsGkAtoms::menuitem, &nsGkAtoms::menupopup,
    &nsGkAtoms::row, &nsGkAtoms::slider, &nsGkAtoms::spacer,
    &nsGkAtoms::splitter, &nsGkAtoms::text, &nsGkAtoms::tree, nullptr};

  uint32_t i;
  if (aNameSpaceID == kNameSpaceID_XUL) {
    for (i = 0; kValidXULTagNames[i]; ++i) {
      if (aTagName == *(kValidXULTagNames[i])) {
        return true;
      }
    }
  }
  else if (aNameSpaceID == kNameSpaceID_SVG &&
           aTagName == nsGkAtoms::generic_) {
    return true;
  }

  return false;
}

nsresult
nsXBLPrototypeBinding::ResolveBaseBinding()
{
  if (mCheckedBaseProto)
    return NS_OK;
  mCheckedBaseProto = true;

  nsCOMPtr<nsIDocument> doc = mXBLDocInfoWeak->GetDocument();

  // Check for the presence of 'extends' and 'display' attributes
  nsAutoString display, extends;
  mBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::extends, extends);
  if (extends.IsEmpty())
    return NS_OK;

  mBinding->GetAttr(kNameSpaceID_None, nsGkAtoms::display, display);
  bool hasDisplay = !display.IsEmpty();

  nsAutoString value(extends);
       
  // Now slice 'em up to see what we've got.
  nsAutoString prefix;
  int32_t offset;
  if (hasDisplay) {
    offset = display.FindChar(':');
    if (-1 != offset) {
      display.Left(prefix, offset);
      display.Cut(0, offset+1);
    }
  }
  else {
    offset = extends.FindChar(':');
    if (-1 != offset) {
      extends.Left(prefix, offset);
      extends.Cut(0, offset+1);
      display = extends;
    }
  }

  nsAutoString nameSpace;

  if (!prefix.IsEmpty()) {
    mBinding->LookupNamespaceURI(prefix, nameSpace);
    if (!nameSpace.IsEmpty()) {
      int32_t nameSpaceID =
        nsContentUtils::NameSpaceManager()->GetNameSpaceID(nameSpace);

      nsCOMPtr<nsIAtom> tagName = do_GetAtom(display);
      // Check the white list
      if (!CheckTagNameWhiteList(nameSpaceID, tagName)) {
        const PRUnichar* params[] = { display.get() };
        nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                        "XBL", nullptr,
                                        nsContentUtils::eXBL_PROPERTIES,
                                       "InvalidExtendsBinding",
                                        params, ArrayLength(params),
                                        doc->GetDocumentURI());
        NS_ASSERTION(!nsXBLService::IsChromeOrResourceURI(doc->GetDocumentURI()),
                     "Invalid extends value");
        return NS_ERROR_ILLEGAL_VALUE;
      }

      SetBaseTag(nameSpaceID, tagName);
    }
  }

  if (hasDisplay || nameSpace.IsEmpty()) {
    mBinding->UnsetAttr(kNameSpaceID_None, nsGkAtoms::extends, false);
    mBinding->UnsetAttr(kNameSpaceID_None, nsGkAtoms::display, false);

    return NS_NewURI(getter_AddRefs(mBaseBindingURI), value,
                     doc->GetDocumentCharacterSet().get(),
                     doc->GetDocBaseURI());
  }

  return NS_OK;
}
