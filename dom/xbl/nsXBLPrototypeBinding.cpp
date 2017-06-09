/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsCOMPtr.h"
#include "nsIAtom.h"
#include "nsIInputStream.h"
#include "nsNameSpaceManager.h"
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
#include "nsIInterfaceInfo.h"
#include "nsIScriptError.h"

#include "nsCSSRuleProcessor.h"
#include "nsXBLResourceLoader.h"
#include "mozilla/AddonPathService.h"
#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/Element.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

// Helper Classes =====================================================================

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
  mBindToUntrustedContent(false),
  mResources(nullptr),
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

void
nsXBLPrototypeBinding::Traverse(nsCycleCollectionTraversalCallback &cb) const
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "proto mBinding");
  cb.NoteXPCOMChild(mBinding);
  if (mResources) {
    mResources->Traverse(cb);
  }
  ImplCycleCollectionTraverse(cb, mInterfaceTable, "proto mInterfaceTable");
}

void
nsXBLPrototypeBinding::Unlink()
{
  if (mImplementation) {
    mImplementation->UnlinkJSObjects();
  }

  if (mResources) {
    mResources->Unlink();
  }
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
    ConstructAttributeTable(content);
  }
}

nsXBLPrototypeBinding::~nsXBLPrototypeBinding(void)
{
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

void
nsXBLPrototypeBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  if (mBinding->AttrValueIs(kNameSpaceID_None, nsGkAtoms::inheritstyle,
                            nsGkAtoms::_false, eCaseMatters))
    mInheritStyle = false;

  mChromeOnlyContent = mBinding->AttrValueIs(kNameSpaceID_None,
                                             nsGkAtoms::chromeOnlyContent,
                                             nsGkAtoms::_true, eCaseMatters);

  mBindToUntrustedContent = mBinding->AttrValueIs(kNameSpaceID_None,
                                                  nsGkAtoms::bindToUntrustedContent,
                                                  nsGkAtoms::_true, eCaseMatters);
}

bool
nsXBLPrototypeBinding::GetAllowScripts() const
{
  return mXBLDocInfoWeak->GetScriptAccess();
}

bool
nsXBLPrototypeBinding::LoadResources(nsIContent* aBoundElement)
{
  if (mResources) {
    return mResources->LoadResources(aBoundElement);
  }

  return true;
}

nsresult
nsXBLPrototypeBinding::AddResource(nsIAtom* aResourceType, const nsAString& aSrc)
{
  EnsureResources();

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
    return mImplementation->mConstructor->Execute(aBoundElement, MapURIToAddonID(mBindingURI));
  return NS_OK;
}

nsresult
nsXBLPrototypeBinding::BindingDetached(nsIContent* aBoundElement)
{
  if (mImplementation && mImplementation->CompiledMembers() &&
      mImplementation->mDestructor)
    return mImplementation->mDestructor->Execute(aBoundElement, MapURIToAddonID(mBindingURI));
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

  InnerAttributeTable *attributesNS = mAttributeTable->Get(aNameSpaceID);
  if (!attributesNS)
    return;

  nsXBLAttributeEntry* xblAttr = attributesNS->Get(aAttribute);
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
          value.StripChar(char16_t('\n'));
          value.StripChar(char16_t('\r'));
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
            RefPtr<nsTextNode> textContent =
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
  return !!mInterfaceTable.GetWeak(aIID);
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
nsXBLPrototypeBinding::InitClass(const nsString& aClassName,
                                 JSContext * aContext,
                                 JS::Handle<JSObject*> aScriptObject,
                                 JS::MutableHandle<JSObject*> aClassObject,
                                 bool* aNew)
{
  return nsXBLBinding::DoInitJSClass(aContext, aScriptObject,
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

  nsIContent* templParent = aTemplChild->GetParent();

  // We may be disconnected from our parent during cycle collection.
  if (!templParent)
    return nullptr;

  nsIContent *copyParent =
    templParent == aTemplRoot ? aCopyRoot :
                   LocateInstance(aBoundElement, aTemplRoot, aCopyRoot, templParent);

  if (!copyParent)
    return nullptr;

  return copyParent->GetChildAt(templParent->IndexOf(aTemplChild));
}

void
nsXBLPrototypeBinding::SetInitialAttributes(nsIContent* aBoundElement, nsIContent* aAnonymousContent)
{
  if (!mAttributeTable) {
    return;
  }

  for (auto iter1 = mAttributeTable->Iter(); !iter1.Done(); iter1.Next()) {
    InnerAttributeTable* xblAttributes = iter1.UserData();
    if (xblAttributes) {
      int32_t srcNamespace = iter1.Key();

      for (auto iter2 = xblAttributes->Iter(); !iter2.Done(); iter2.Next()) {
        // XXXbz this duplicates lots of AttributeChanged
        nsXBLAttributeEntry* entry = iter2.UserData();
        nsIAtom* src = entry->GetSrcAttribute();
        nsAutoString value;
        bool attrPresent = true;

        if (src == nsGkAtoms::text && srcNamespace == kNameSpaceID_XBL) {
          nsContentUtils::GetNodeTextContent(aBoundElement, false, value);
          value.StripChar(char16_t('\n'));
          value.StripChar(char16_t('\r'));
          nsAutoString stripVal(value);
          stripVal.StripWhitespace();

          if (stripVal.IsEmpty()) {
            attrPresent = false;
          }
        } else {
          attrPresent = aBoundElement->GetAttr(srcNamespace, src, value);
        }

        if (attrPresent) {
          nsIContent* content = GetImmediateChild(nsGkAtoms::content);

          nsXBLAttributeEntry* curr = entry;
          while (curr) {
            nsIAtom* dst = curr->GetDstAttribute();
            int32_t dstNs = curr->GetDstNameSpace();
            nsIContent* element = curr->GetElement();

            nsIContent* realElement =
              LocateInstance(aBoundElement, content,
                             aAnonymousContent, element);

            if (realElement) {
              realElement->SetAttr(dstNs, dst, value, false);

              // XXXndeakin shouldn't this be done in lieu of SetAttr?
              if ((dst == nsGkAtoms::text && dstNs == kNameSpaceID_XBL) ||
                  (realElement->NodeInfo()->Equals(nsGkAtoms::html,
                                                   kNameSpaceID_XUL) &&
                   dst == nsGkAtoms::value && !value.IsEmpty())) {

                RefPtr<nsTextNode> textContent =
                  new nsTextNode(realElement->NodeInfo()->NodeInfoManager());

                textContent->SetText(value, false);
                realElement->AppendChildTo(textContent, false);
              }
            }

            curr = curr->GetNext();
          }
        }
      }
    }
  }
}

nsIStyleRuleProcessor*
nsXBLPrototypeBinding::GetRuleProcessor()
{
  if (mResources) {
    return mResources->GetRuleProcessor();
  }

  return nullptr;
}

const ServoStyleSet*
nsXBLPrototypeBinding::GetServoStyleSet() const
{
  return mResources ? mResources->GetServoStyleSet() : nullptr;
}

void
nsXBLPrototypeBinding::EnsureAttributeTable()
{
  if (!mAttributeTable) {
    mAttributeTable =
        new nsClassHashtable<nsUint32HashKey, InnerAttributeTable>(2);
  }
}

void
nsXBLPrototypeBinding::AddToAttributeTable(int32_t aSourceNamespaceID, nsIAtom* aSourceTag,
                                           int32_t aDestNamespaceID, nsIAtom* aDestTag,
                                           nsIContent* aContent)
{
    InnerAttributeTable* attributesNS = mAttributeTable->Get(aSourceNamespaceID);
    if (!attributesNS) {
      attributesNS = new InnerAttributeTable(2);
      mAttributeTable->Put(aSourceNamespaceID, attributesNS);
    }

    nsXBLAttributeEntry* xblAttr =
      new nsXBLAttributeEntry(aSourceTag, aDestTag, aDestNamespaceID, aContent);

    nsXBLAttributeEntry* entry = attributesNS->Get(aSourceTag);
    if (!entry) {
      attributesNS->Put(aSourceTag, xblAttr);
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

      free(str);
    }
  }

  // Recur into our children.
  for (nsIContent* child = aElement->GetFirstChild();
       child;
       child = child->GetNextSibling()) {
    ConstructAttributeTable(child);
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
          mInterfaceTable.Put(*iid, mBinding);

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
            mInterfaceTable.Put(*iid, mBinding);

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
        RefPtr<nsXBLKeyEventHandler> newHandler =
          new nsXBLKeyEventHandler(eventAtom, phase, type);
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
  mBindToUntrustedContent =
    (aFlags & XBLBinding_Serialize_BindToUntrustedContent) ? true : false;

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
    mBaseTag = NS_Atomize(baseTag);
  }

  mBinding = aDocument->CreateElem(NS_LITERAL_STRING("binding"), nullptr,
                                   kNameSpaceID_XBL);

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

  for (; interfaceCount > 0; interfaceCount--) {
    nsIID iid;
    rv = aStream->ReadID(&iid);
    NS_ENSURE_SUCCESS(rv, rv);
    mInterfaceTable.Put(iid, mBinding);
  }

  // We're not directly using this AutoJSAPI here, but callees use it via
  // AutoJSContext.
  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::CompilationScope())) {
    return NS_ERROR_UNEXPECTED;
  }

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
    rv = mImplementation->Read(aStream, this);
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
    rv = handler->Read(aStream);
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

  if (mBinding) {
    while (true) {
      XBLBindingSerializeDetails type;
      rv = aStream->Read8(&type);
      NS_ENSURE_SUCCESS(rv, rv);

      if (type != XBLBinding_Serialize_Attribute) {
        break;
      }

      int32_t attrNamespace;
      rv = ReadNamespace(aStream, attrNamespace);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString attrPrefix, attrName, attrValue;
      rv = aStream->ReadString(attrPrefix);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aStream->ReadString(attrName);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aStream->ReadString(attrValue);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIAtom> atomPrefix = NS_Atomize(attrPrefix);
      nsCOMPtr<nsIAtom> atomName = NS_Atomize(attrName);
      mBinding->SetAttr(attrNamespace, atomName, atomPrefix, attrValue, false);
    }
  }

  // Finally, read in the resources.
  while (true) {
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
  }

  if (isFirstBinding) {
    aDocInfo->SetFirstPrototypeBinding(this);
  }

  cleanup.Disconnect();
  return NS_OK;
}

// static
nsresult
nsXBLPrototypeBinding::ReadNewBinding(nsIObjectInputStream* aStream,
                                      nsXBLDocumentInfo* aDocInfo,
                                      nsIDocument* aDocument,
                                      uint8_t aFlags)
{
  // If the Read() succeeds, |binding| will end up being owned by aDocInfo's
  // binding table. Otherwise, we must manually delete it.
  nsXBLPrototypeBinding* binding = new nsXBLPrototypeBinding();
  nsresult rv = binding->Read(aStream, aDocInfo, aDocument, aFlags);
  if (NS_FAILED(rv)) {
    delete binding;
  }
  return rv;
}

nsresult
nsXBLPrototypeBinding::Write(nsIObjectOutputStream* aStream)
{
  // This writes out the binding. Note that mCheckedBaseProto,
  // mKeyHandlersRegistered and mKeyHandlers are not serialized as they are
  // computed on demand.

  // We're not directly using this AutoJSAPI here, but callees use it via
  // AutoJSContext.
  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::CompilationScope())) {
    return NS_ERROR_UNEXPECTED;
  }

  uint8_t flags = mInheritStyle ? XBLBinding_Serialize_InheritStyle : 0;

  // mAlternateBindingURI is only set on the first binding.
  if (mAlternateBindingURI) {
    flags |= XBLBinding_Serialize_IsFirstBinding;
  }

  if (mChromeOnlyContent) {
    flags |= XBLBinding_Serialize_ChromeOnlyContent;
  }

  if (mBindToUntrustedContent) {
    flags |= XBLBinding_Serialize_BindToUntrustedContent;
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
  if (mBaseBindingURI) {
    rv = mBaseBindingURI->GetSpec(extends);
    NS_ENSURE_SUCCESS(rv, rv);
  }

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

  nsIContent* content = GetImmediateChild(nsGkAtoms::content);
  if (content) {
    rv = WriteContentNode(aStream, content);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Write a marker to indicate that there is no content.
    rv = aStream->Write8(XBLBinding_Serialize_NoContent);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Enumerate and write out the implemented interfaces.
  rv = aStream->Write32(mInterfaceTable.Count());
  NS_ENSURE_SUCCESS(rv, rv);

  for (auto iter = mInterfaceTable.Iter(); !iter.Done(); iter.Next()) {
    // We can just write out the ids. The cache will be invalidated when a
    // different build is used, so we don't need to worry about ids changing.
    aStream->WriteID(iter.Key());
  }

  // Write out the implementation details.
  if (mImplementation) {
    rv = mImplementation->Write(aStream, this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Write out an empty classname. This indicates that the binding does not
    // define an implementation.
    rv = aStream->WriteUtf8Z(EmptyString().get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out the handlers.
  nsXBLPrototypeHandler* handler = mPrototypeHandler;
  while (handler) {
    rv = handler->Write(aStream);
    NS_ENSURE_SUCCESS(rv, rv);

    handler = handler->GetNextHandler();
  }

  aStream->Write8(XBLBinding_Serialize_NoMoreItems);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mBinding) {
    uint32_t attributes = mBinding->GetAttrCount();
    nsAutoString attrValue;
    for (uint32_t i = 0; i < attributes; ++i) {
      BorrowedAttrInfo attrInfo = mBinding->GetAttrInfoAt(i);
      const nsAttrName* name = attrInfo.mName;
      nsDependentAtomString attrName(attrInfo.mName->LocalName());
      attrInfo.mValue->ToString(attrValue);

      rv = aStream->Write8(XBLBinding_Serialize_Attribute);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = WriteNamespace(aStream, name->NamespaceID());
      NS_ENSURE_SUCCESS(rv, rv);

      nsIAtom* prefix = name->GetPrefix();
      nsAutoString prefixString;
      if (prefix) {
        prefix->ToString(prefixString);
      }

      rv = aStream->WriteWStringZ(prefixString.get());
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aStream->WriteWStringZ(attrName.get());
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aStream->WriteWStringZ(attrValue.get());
      NS_ENSURE_SUCCESS(rv, rv);
    }
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
    prefixAtom = NS_Atomize(prefix);

  rv = aStream->ReadString(tag);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAtom> tagAtom = NS_Atomize(tag);
  RefPtr<NodeInfo> nodeInfo =
    aNim->GetNodeInfo(tagAtom, prefixAtom, namespaceID, nsIDOMNode::ELEMENT_NODE);

  uint32_t attrCount;
  rv = aStream->Read32(&attrCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // Create XUL prototype elements, or regular elements for other namespaces.
  // This needs to match the code in nsXBLContentSink::CreateElement.
#ifdef MOZ_XUL
  if (namespaceID == kNameSpaceID_XUL) {
    nsIURI* documentURI = aDocument->GetDocumentURI();

    RefPtr<nsXULPrototypeElement> prototype = new nsXULPrototypeElement();

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

      nsCOMPtr<nsIAtom> nameAtom = NS_Atomize(name);
      if (namespaceID == kNameSpaceID_None) {
        attrs[i].mName.SetTo(nameAtom);
      }
      else {
        nsCOMPtr<nsIAtom> prefixAtom;
        if (!prefix.IsEmpty())
          prefixAtom = NS_Atomize(prefix);

        RefPtr<NodeInfo> ni =
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
    nsCOMPtr<Element> element;
    NS_NewElement(getter_AddRefs(element), nodeInfo.forget(), NOT_FROM_PARSER);
    content = element;

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
        prefixAtom = NS_Atomize(prefix);

      nsCOMPtr<nsIAtom> nameAtom = NS_Atomize(name);
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

    nsCOMPtr<nsIAtom> srcAtom = NS_Atomize(srcAttribute);
    nsCOMPtr<nsIAtom> destAtom = NS_Atomize(destAttribute);

    EnsureAttributeTable();
    AddToAttributeTable(srcNamespaceID, srcAtom, destNamespaceID, destAtom, content);

    rv = ReadNamespace(aStream, srcNamespaceID);
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

nsresult
nsXBLPrototypeBinding::WriteContentNode(nsIObjectOutputStream* aStream,
                                        nsIContent* aNode)
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

  rv = aStream->WriteWStringZ(nsDependentAtomString(aNode->NodeInfo()->NameAtom()).get());
  NS_ENSURE_SUCCESS(rv, rv);

  // Write attributes
  uint32_t count = aNode->GetAttrCount();
  rv = aStream->Write32(count);
  NS_ENSURE_SUCCESS(rv, rv);

  uint32_t i;
  for (i = 0; i < count; i++) {
    // Write out the namespace id, the namespace prefix, the local tag name,
    // and the value, in that order.

    const BorrowedAttrInfo attrInfo = aNode->GetAttrInfoAt(i);
    const nsAttrName* name = attrInfo.mName;

    // XXXndeakin don't write out xbl:inherits?
    int32_t namespaceID = name->NamespaceID();
    rv = WriteNamespace(aStream, namespaceID);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString prefixStr;
    nsIAtom* prefix = name->GetPrefix();
    if (prefix)
      prefix->ToString(prefixStr);
    rv = aStream->WriteWStringZ(prefixStr.get());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(nsDependentAtomString(name->LocalName()).get());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString val;
    attrInfo.mValue->ToString(val);
    rv = aStream->WriteWStringZ(val.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Write out the attribute fowarding information
  if (mAttributeTable) {
    for (auto iter1 = mAttributeTable->Iter(); !iter1.Done(); iter1.Next()) {
      int32_t srcNamespace = iter1.Key();
      InnerAttributeTable* xblAttributes = iter1.UserData();

      for (auto iter2 = xblAttributes->Iter(); !iter2.Done(); iter2.Next()) {
        nsXBLAttributeEntry* entry = iter2.UserData();

        do {
          if (entry->GetElement() == aNode) {
            WriteNamespace(aStream, srcNamespace);
            aStream->WriteWStringZ(
              nsDependentAtomString(entry->GetSrcAttribute()).get());
            WriteNamespace(aStream, entry->GetDstNameSpace());
            aStream->WriteWStringZ(
              nsDependentAtomString(entry->GetDstAttribute()).get());
          }

          entry = entry->GetNext();
        } while (entry);
      }
    }
  }
  rv = aStream->Write8(XBLBinding_Serialize_NoMoreAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally, write out the child nodes.
  count = aNode->GetChildCount();
  rv = aStream->Write32(count);
  NS_ENSURE_SUCCESS(rv, rv);

  for (i = 0; i < count; i++) {
    rv = WriteContentNode(aStream, aNode->GetChildAt(i));
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
        nsContentUtils::NameSpaceManager()->GetNameSpaceID(nameSpace,
                                                           nsContentUtils::IsChromeDoc(doc));

      nsCOMPtr<nsIAtom> tagName = NS_Atomize(display);
      // Check the white list
      if (!CheckTagNameWhiteList(nameSpaceID, tagName)) {
        const char16_t* params[] = { display.get() };
        nsContentUtils::ReportToConsole(nsIScriptError::errorFlag,
                                        NS_LITERAL_CSTRING("XBL"), nullptr,
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

void
nsXBLPrototypeBinding::EnsureResources()
{
  if (!mResources) {
    mResources = new nsXBLPrototypeResources(this);
  }
}

void
nsXBLPrototypeBinding::AppendStyleSheet(StyleSheet* aSheet)
{
  EnsureResources();
  mResources->AppendStyleSheet(aSheet);
}

void
nsXBLPrototypeBinding::RemoveStyleSheet(StyleSheet* aSheet)
{
  if (!mResources) {
    MOZ_ASSERT(false, "Trying to remove a sheet that does not exist.");
    return;
  }

  mResources->RemoveStyleSheet(aSheet);
}
void
nsXBLPrototypeBinding::InsertStyleSheetAt(size_t aIndex, StyleSheet* aSheet)
{
  EnsureResources();
  mResources->InsertStyleSheetAt(aIndex, aSheet);
}

StyleSheet*
nsXBLPrototypeBinding::StyleSheetAt(size_t aIndex) const
{
  MOZ_ASSERT(mResources);
  return mResources->StyleSheetAt(aIndex);
}

size_t
nsXBLPrototypeBinding::SheetCount() const
{
  return mResources ? mResources->SheetCount() : 0;
}

bool
nsXBLPrototypeBinding::HasStyleSheets() const
{
  return mResources && mResources->HasStyleSheets();
}

void
nsXBLPrototypeBinding::AppendStyleSheetsTo(
                                      nsTArray<StyleSheet*>& aResult) const
{
  if (mResources) {
    mResources->AppendStyleSheetsTo(aResult);
  }
}
