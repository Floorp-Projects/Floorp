/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasicEvents.h"

#include "DataTransfer.h"

#include "nsIDOMDocument.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsError.h"
#include "nsIDragService.h"
#include "nsIClipboard.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIBinaryInputStream.h"
#include "nsIBinaryOutputStream.h"
#include "nsIStorageStream.h"
#include "nsStringStream.h"
#include "nsCRT.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsVariant.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DataTransferBinding.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/OSFileSystem.h"
#include "mozilla/dom/Promise.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DataTransfer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DataTransfer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mItems)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDragTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDragImage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DataTransfer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mItems)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDragTarget)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDragImage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(DataTransfer)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DataTransfer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DataTransfer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DataTransfer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::DataTransfer)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDataTransfer)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDataTransfer)
NS_INTERFACE_MAP_END

// the size of the array
const char DataTransfer::sEffects[8][9] = {
  "none", "copy", "move", "copyMove", "link", "copyLink", "linkMove", "all"
};

// Used for custom clipboard types.
enum CustomClipboardTypeId {
  eCustomClipboardTypeId_None,
  eCustomClipboardTypeId_String
};

DataTransfer::DataTransfer(nsISupports* aParent, EventMessage aEventMessage,
                           bool aIsExternal, int32_t aClipboardType)
  : mParent(aParent)
  , mDropEffect(nsIDragService::DRAGDROP_ACTION_NONE)
  , mEffectAllowed(nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
  , mEventMessage(aEventMessage)
  , mCursorState(false)
  , mReadOnly(true)
  , mIsExternal(aIsExternal)
  , mUserCancelled(false)
  , mIsCrossDomainSubFrameDrop(false)
  , mClipboardType(aClipboardType)
  , mDragImageX(0)
  , mDragImageY(0)
{
  mItems = new DataTransferItemList(this, aIsExternal);
  // For these events, we want to be able to add data to the data transfer, so
  // clear the readonly state. Otherwise, the data is already present. For
  // external usage, cache the data from the native clipboard or drag.
  if (aEventMessage == eCut ||
      aEventMessage == eCopy ||
      aEventMessage == eDragStart) {
    mReadOnly = false;
  } else if (mIsExternal) {
    if (aEventMessage == ePaste) {
      CacheExternalClipboardFormats();
    } else if (aEventMessage >= eDragDropEventFirst &&
               aEventMessage <= eDragDropEventLast) {
      CacheExternalDragFormats();
    }
  }
}

DataTransfer::DataTransfer(nsISupports* aParent,
                           EventMessage aEventMessage,
                           const uint32_t aEffectAllowed,
                           bool aCursorState,
                           bool aIsExternal,
                           bool aUserCancelled,
                           bool aIsCrossDomainSubFrameDrop,
                           int32_t aClipboardType,
                           DataTransferItemList* aItems,
                           Element* aDragImage,
                           uint32_t aDragImageX,
                           uint32_t aDragImageY)
  : mParent(aParent)
  , mDropEffect(nsIDragService::DRAGDROP_ACTION_NONE)
  , mEffectAllowed(aEffectAllowed)
  , mEventMessage(aEventMessage)
  , mCursorState(aCursorState)
  , mReadOnly(true)
  , mIsExternal(aIsExternal)
  , mUserCancelled(aUserCancelled)
  , mIsCrossDomainSubFrameDrop(aIsCrossDomainSubFrameDrop)
  , mClipboardType(aClipboardType)
  , mDragImage(aDragImage)
  , mDragImageX(aDragImageX)
  , mDragImageY(aDragImageY)
{
  MOZ_ASSERT(mParent);
  MOZ_ASSERT(aItems);

  // We clone the items array after everything else, so that it has a valid
  // mParent value
  mItems = aItems->Clone(this);
  // The items are copied from aItems into mItems. There is no need to copy
  // the actual data in the items as the data transfer will be read only. The
  // dragstart event is the only time when items are
  // modifiable, but those events should have been using the first constructor
  // above.
  NS_ASSERTION(aEventMessage != eDragStart,
               "invalid event type for DataTransfer constructor");
}

DataTransfer::~DataTransfer()
{}

// static
already_AddRefed<DataTransfer>
DataTransfer::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aEventType, bool aIsExternal,
                          ErrorResult& aRv)
{
  nsAutoCString onEventType("on");
  AppendUTF16toUTF8(aEventType, onEventType);
  nsCOMPtr<nsIAtom> eventTypeAtom = NS_Atomize(onEventType);
  if (!eventTypeAtom) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  EventMessage eventMessage = nsContentUtils::GetEventMessage(eventTypeAtom);
  RefPtr<DataTransfer> transfer = new DataTransfer(aGlobal.GetAsSupports(),
                                                     eventMessage, aIsExternal,
                                                     -1);
  return transfer.forget();
}

JSObject*
DataTransfer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DataTransferBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
DataTransfer::GetDropEffect(nsAString& aDropEffect)
{
  nsString dropEffect;
  GetDropEffect(dropEffect);
  aDropEffect = dropEffect;
  return NS_OK;
}

NS_IMETHODIMP
DataTransfer::SetDropEffect(const nsAString& aDropEffect)
{
  // the drop effect can only be 'none', 'copy', 'move' or 'link'.
  for (uint32_t e = 0; e <= nsIDragService::DRAGDROP_ACTION_LINK; e++) {
    if (aDropEffect.EqualsASCII(sEffects[e])) {
      // don't allow copyMove
      if (e != (nsIDragService::DRAGDROP_ACTION_COPY |
                nsIDragService::DRAGDROP_ACTION_MOVE)) {
        mDropEffect = e;
      }
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DataTransfer::GetEffectAllowed(nsAString& aEffectAllowed)
{
  nsString effectAllowed;
  GetEffectAllowed(effectAllowed);
  aEffectAllowed = effectAllowed;
  return NS_OK;
}

NS_IMETHODIMP
DataTransfer::SetEffectAllowed(const nsAString& aEffectAllowed)
{
  if (aEffectAllowed.EqualsLiteral("uninitialized")) {
    mEffectAllowed = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
    return NS_OK;
  }

  static_assert(nsIDragService::DRAGDROP_ACTION_NONE == 0,
                "DRAGDROP_ACTION_NONE constant is wrong");
  static_assert(nsIDragService::DRAGDROP_ACTION_COPY == 1,
                "DRAGDROP_ACTION_COPY constant is wrong");
  static_assert(nsIDragService::DRAGDROP_ACTION_MOVE == 2,
                "DRAGDROP_ACTION_MOVE constant is wrong");
  static_assert(nsIDragService::DRAGDROP_ACTION_LINK == 4,
                "DRAGDROP_ACTION_LINK constant is wrong");

  for (uint32_t e = 0; e < ArrayLength(sEffects); e++) {
    if (aEffectAllowed.EqualsASCII(sEffects[e])) {
      mEffectAllowed = e;
      break;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
DataTransfer::GetDropEffectInt(uint32_t* aDropEffect)
{
  *aDropEffect = mDropEffect;
  return  NS_OK;
}

NS_IMETHODIMP
DataTransfer::SetDropEffectInt(uint32_t aDropEffect)
{
  mDropEffect = aDropEffect;
  return  NS_OK;
}

NS_IMETHODIMP
DataTransfer::GetEffectAllowedInt(uint32_t* aEffectAllowed)
{
  *aEffectAllowed = mEffectAllowed;
  return  NS_OK;
}

NS_IMETHODIMP
DataTransfer::SetEffectAllowedInt(uint32_t aEffectAllowed)
{
  mEffectAllowed = aEffectAllowed;
  return  NS_OK;
}

NS_IMETHODIMP
DataTransfer::GetMozUserCancelled(bool* aUserCancelled)
{
  *aUserCancelled = MozUserCancelled();
  return NS_OK;
}

already_AddRefed<FileList>
DataTransfer::GetFiles(const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                       ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());
  return mItems->Files(aSubjectPrincipal.value());
}

NS_IMETHODIMP
DataTransfer::GetFiles(nsIDOMFileList** aFileList)
{
  if (!aFileList) {
    return NS_ERROR_FAILURE;
  }

  // The XPCOM interface is only avaliable to system code, and thus we can
  // assume the system principal. This is consistent with the previous behavour
  // of this function, which also assumed the system principal.
  //
  // This code is also called from C++ code, which expects it to have a System
  // Principal, and thus the SubjectPrincipal cannot be used.
  RefPtr<FileList> files = mItems->Files(nsContentUtils::GetSystemPrincipal());

  files.forget(aFileList);
  return NS_OK;
}

void
DataTransfer::GetTypes(nsTArray<nsString>& aTypes) const
{
  // When called from bindings, aTypes will be empty, but since we might have
  // Gecko-internal callers too, clear it to be safe.
  aTypes.Clear();
  
  const nsTArray<RefPtr<DataTransferItem>>* items = mItems->MozItemsAt(0);
  if (NS_WARN_IF(!items)) {
    return;
  }

  for (uint32_t i = 0; i < items->Length(); i++) {
    DataTransferItem* item = items->ElementAt(i);
    MOZ_ASSERT(item);

    if (item->ChromeOnly() && !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
      continue;
    }

    nsAutoString type;
    item->GetType(type);
    if (item->Kind() == DataTransferItem::KIND_STRING || type.EqualsASCII(kFileMime)) {
      // If the entry has kind KIND_STRING, we want to add it to the list.
      aTypes.AppendElement(type);
    }
  }

  for (uint32_t i = 0; i < mItems->Length(); ++i) {
    bool found = false;
    DataTransferItem* item = mItems->IndexedGetter(i, found);
    MOZ_ASSERT(found);
    if (item->Kind() != DataTransferItem::KIND_FILE) {
      continue;
    }
    aTypes.AppendElement(NS_LITERAL_STRING("Files"));
    break;
  }
}

void
DataTransfer::GetData(const nsAString& aFormat, nsAString& aData,
                      const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  // return an empty string if data for the format was not found
  aData.Truncate();

  nsCOMPtr<nsIVariant> data;
  nsresult rv =
    GetDataAtInternal(aFormat, 0, aSubjectPrincipal.value(),
                      getter_AddRefs(data));
  if (NS_FAILED(rv)) {
    if (rv != NS_ERROR_DOM_INDEX_SIZE_ERR) {
      aRv.Throw(rv);
    }
    return;
  }

  if (data) {
    nsAutoString stringdata;
    data->GetAsAString(stringdata);

    // for the URL type, parse out the first URI from the list. The URIs are
    // separated by newlines
    nsAutoString lowercaseFormat;
    nsContentUtils::ASCIIToLower(aFormat, lowercaseFormat);

    if (lowercaseFormat.EqualsLiteral("url")) {
      int32_t lastidx = 0, idx;
      int32_t length = stringdata.Length();
      while (lastidx < length) {
        idx = stringdata.FindChar('\n', lastidx);
        // lines beginning with # are comments
        if (stringdata[lastidx] == '#') {
          if (idx == -1) {
            break;
          }
        }
        else {
          if (idx == -1) {
            aData.Assign(Substring(stringdata, lastidx));
          } else {
            aData.Assign(Substring(stringdata, lastidx, idx - lastidx));
          }
          aData = nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(aData,
                                                                      true);
          return;
        }
        lastidx = idx + 1;
      }
    }
    else {
      aData = stringdata;
    }
  }
}

void
DataTransfer::SetData(const nsAString& aFormat, const nsAString& aData,
                      const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  RefPtr<nsVariantCC> variant = new nsVariantCC();
  variant->SetAsAString(aData);

  aRv = SetDataAtInternal(aFormat, variant, 0, aSubjectPrincipal.value());
}

void
DataTransfer::ClearData(const Optional<nsAString>& aFormat,
                        const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  if (mReadOnly) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (MozItemCount() == 0) {
    return;
  }

  if (aFormat.WasPassed()) {
    MozClearDataAtHelper(aFormat.Value(), 0, aSubjectPrincipal, aRv);
  } else {
    MozClearDataAtHelper(EmptyString(), 0, aSubjectPrincipal, aRv);
  }
}

NS_IMETHODIMP
DataTransfer::GetMozItemCount(uint32_t* aCount)
{
  *aCount = MozItemCount();
  return NS_OK;
}

NS_IMETHODIMP
DataTransfer::GetMozCursor(nsAString& aCursorState)
{
  nsString cursor;
  GetMozCursor(cursor);
  aCursorState = cursor;
  return NS_OK;
}

NS_IMETHODIMP
DataTransfer::SetMozCursor(const nsAString& aCursorState)
{
  // Lock the cursor to an arrow during the drag.
  mCursorState = aCursorState.EqualsLiteral("default");

  return NS_OK;
}

already_AddRefed<nsINode>
DataTransfer::GetMozSourceNode()
{
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMNode> sourceNode;
  dragSession->GetSourceNode(getter_AddRefs(sourceNode));
  nsCOMPtr<nsINode> node = do_QueryInterface(sourceNode);
  if (node && !nsContentUtils::LegacyIsCallerNativeCode()
      && !nsContentUtils::CanCallerAccess(node)) {
    return nullptr;
  }

  return node.forget();
}

NS_IMETHODIMP
DataTransfer::GetMozSourceNode(nsIDOMNode** aSourceNode)
{
  nsCOMPtr<nsINode> sourceNode = GetMozSourceNode();
  if (!sourceNode) {
    *aSourceNode = nullptr;
    return NS_OK;
  }

  return CallQueryInterface(sourceNode, aSourceNode);
}

already_AddRefed<DOMStringList>
DataTransfer::MozTypesAt(uint32_t aIndex, ErrorResult& aRv) const
{
  // Only the first item is valid for clipboard events
  if (aIndex > 0 &&
      (mEventMessage == eCut || mEventMessage == eCopy ||
       mEventMessage == ePaste)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  RefPtr<DOMStringList> types = new DOMStringList();
  if (aIndex < MozItemCount()) {
    // note that you can retrieve the types regardless of their principal
    const nsTArray<RefPtr<DataTransferItem>>& items = *mItems->MozItemsAt(aIndex);

    bool addFile = false;
    for (uint32_t i = 0; i < items.Length(); i++) {
      if (items[i]->ChromeOnly() && !nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
        continue;
      }

      nsAutoString type;
      items[i]->GetType(type);
      if (NS_WARN_IF(!types->Add(type))) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }

      if (items[i]->Kind() == DataTransferItem::KIND_FILE) {
        addFile = true;
      }
    }

    if (addFile) {
      types->Add(NS_LITERAL_STRING("Files"));
    }
  }

  return types.forget();
}

NS_IMETHODIMP
DataTransfer::MozTypesAt(uint32_t aIndex, nsISupports** aTypes)
{
  ErrorResult rv;
  RefPtr<DOMStringList> types = MozTypesAt(aIndex, rv);
  types.forget(aTypes);
  return rv.StealNSResult();
}

nsresult
DataTransfer::GetDataAtNoSecurityCheck(const nsAString& aFormat,
                                       uint32_t aIndex,
                                       nsIVariant** aData)
{
  return GetDataAtInternal(aFormat, aIndex,
                           nsContentUtils::GetSystemPrincipal(), aData);
}

nsresult
DataTransfer::GetDataAtInternal(const nsAString& aFormat, uint32_t aIndex,
                                nsIPrincipal* aSubjectPrincipal,
                                nsIVariant** aData)
{
  *aData = nullptr;

  if (aFormat.IsEmpty()) {
    return NS_OK;
  }

  if (aIndex >= MozItemCount()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Only the first item is valid for clipboard events
  if (aIndex > 0 &&
      (mEventMessage == eCut || mEventMessage == eCopy ||
       mEventMessage == ePaste)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  nsAutoString format;
  GetRealFormat(aFormat, format);

  MOZ_ASSERT(aSubjectPrincipal);

  RefPtr<DataTransferItem> item = mItems->MozItemByTypeAt(format, aIndex);
  if (!item) {
    // The index exists but there's no data for the specified format, in this
    // case we just return undefined
    return NS_OK;
  }

  // If we have chrome only content, and we aren't chrome, don't allow access
  if (!nsContentUtils::IsSystemPrincipal(aSubjectPrincipal) && item->ChromeOnly()) {
    return NS_OK;
  }

  // DataTransferItem::Data() handles the principal checks
  ErrorResult result;
  nsCOMPtr<nsIVariant> data = item->Data(aSubjectPrincipal, result);
  if (NS_WARN_IF(!data || result.Failed())) {
    return result.StealNSResult();
  }

  data.forget(aData);
  return NS_OK;
}

void
DataTransfer::MozGetDataAt(JSContext* aCx, const nsAString& aFormat,
                           uint32_t aIndex,
                           JS::MutableHandle<JS::Value> aRetval,
                           const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                           mozilla::ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  nsCOMPtr<nsIVariant> data;
  aRv = GetDataAtInternal(aFormat, aIndex, aSubjectPrincipal.value(),
                          getter_AddRefs(data));
  if (aRv.Failed()) {
    return;
  }

  if (!data) {
    aRetval.setNull();
    return;
  }

  JS::Rooted<JS::Value> result(aCx);
  if (!VariantToJsval(aCx, data, aRetval)) {
    aRv = NS_ERROR_FAILURE;
    return;
  }
}

/* static */ bool
DataTransfer::PrincipalMaySetData(const nsAString& aType,
                                  nsIVariant* aData,
                                  nsIPrincipal* aPrincipal)
{
  if (!nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    DataTransferItem::eKind kind = DataTransferItem::KindFromData(aData);
    if (kind == DataTransferItem::KIND_OTHER) {
      NS_WARNING("Disallowing adding non string/file types to DataTransfer");
      return false;
    }

    if (aType.EqualsASCII(kFileMime) ||
        aType.EqualsASCII(kFilePromiseMime)) {
      NS_WARNING("Disallowing adding x-moz-file or x-moz-file-promize types to DataTransfer");
      return false;
    }
  }
  return true;
}

void
DataTransfer::TypesListMayHaveChanged()
{
  DataTransferBinding::ClearCachedTypesValue(this);
}

nsresult
DataTransfer::SetDataAtInternal(const nsAString& aFormat, nsIVariant* aData,
                                uint32_t aIndex,
                                nsIPrincipal* aSubjectPrincipal)
{
  if (aFormat.IsEmpty()) {
    return NS_OK;
  }

  if (mReadOnly) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  // Specifying an index less than the current length will replace an existing
  // item. Specifying an index equal to the current length will add a new item.
  if (aIndex > MozItemCount()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Only the first item is valid for clipboard events
  if (aIndex > 0 &&
      (mEventMessage == eCut || mEventMessage == eCopy ||
       mEventMessage == ePaste)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Don't allow the custom type to be assigned.
  if (aFormat.EqualsLiteral(kCustomTypesMime)) {
    return NS_ERROR_TYPE_ERR;
  }

  if (!PrincipalMaySetData(aFormat, aData, aSubjectPrincipal)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return SetDataWithPrincipal(aFormat, aData, aIndex, aSubjectPrincipal);
}

void
DataTransfer::MozSetDataAt(JSContext* aCx, const nsAString& aFormat,
                           JS::Handle<JS::Value> aData, uint32_t aIndex,
                           const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                           ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  nsCOMPtr<nsIVariant> data;
  aRv = nsContentUtils::XPConnect()->JSValToVariant(aCx, aData,
                                                    getter_AddRefs(data));
  if (!aRv.Failed()) {
    aRv = SetDataAtInternal(aFormat, data, aIndex, aSubjectPrincipal.value());
  }
}

void
DataTransfer::MozClearDataAt(const nsAString& aFormat, uint32_t aIndex,
                             const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                             ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  if (mReadOnly) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (aIndex >= MozItemCount()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  // Only the first item is valid for clipboard events
  if (aIndex > 0 &&
      (mEventMessage == eCut || mEventMessage == eCopy ||
       mEventMessage == ePaste)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  MozClearDataAtHelper(aFormat, aIndex, aSubjectPrincipal, aRv);

  // If we just cleared the 0-th index, and there are still more than 1 indexes
  // remaining, MozClearDataAt should cause the 1st index to become the 0th
  // index. This should _only_ happen when the MozClearDataAt function is
  // explicitly called by script, as this behavior is inconsistent with spec.
  // (however, so is the MozClearDataAt API)

  if (aIndex == 0 && mItems->MozItemCount() > 1 &&
      mItems->MozItemsAt(0)->Length() == 0) {
    mItems->PopIndexZero();
  }
}

void
DataTransfer::MozClearDataAtHelper(const nsAString& aFormat, uint32_t aIndex,
                                   const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(!mReadOnly);
  MOZ_ASSERT(aIndex < MozItemCount());
  MOZ_ASSERT(aIndex == 0 ||
             (mEventMessage != eCut && mEventMessage != eCopy &&
              mEventMessage != ePaste));
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  nsAutoString format;
  GetRealFormat(aFormat, format);

  mItems->MozRemoveByTypeAt(format, aIndex, aSubjectPrincipal, aRv);
}

void
DataTransfer::SetDragImage(Element& aImage, int32_t aX, int32_t aY,
                           ErrorResult& aRv)
{
  if (mReadOnly) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  mDragImage = &aImage;
  mDragImageX = aX;
  mDragImageY = aY;
}

NS_IMETHODIMP
DataTransfer::SetDragImage(nsIDOMElement* aImage, int32_t aX, int32_t aY)
{
  ErrorResult rv;
  nsCOMPtr<Element> image = do_QueryInterface(aImage);
  if (image) {
    SetDragImage(*image, aX, aY, rv);
  }
  return rv.StealNSResult();
}

already_AddRefed<Promise>
DataTransfer::GetFilesAndDirectories(const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                                     ErrorResult& aRv)
{
  MOZ_ASSERT(aSubjectPrincipal.isSome());

  nsCOMPtr<nsINode> parentNode = do_QueryInterface(mParent);
  if (!parentNode) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = parentNode->OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<FileList> files = mItems->Files(aSubjectPrincipal.value());
  if (NS_WARN_IF(!files)) {
    return nullptr;
  }

  Sequence<RefPtr<File>> filesSeq;
  files->ToSequence(filesSeq, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  p->MaybeResolve(filesSeq);

  return p.forget();
}

already_AddRefed<Promise>
DataTransfer::GetFiles(bool aRecursiveFlag,
                       const Maybe<nsIPrincipal*>& aSubjectPrincipal,
                       ErrorResult& aRv)
{
  // Currently we don't support directories.
  return GetFilesAndDirectories(aSubjectPrincipal, aRv);
}

void
DataTransfer::AddElement(Element& aElement, ErrorResult& aRv)
{
  if (mReadOnly) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  mDragTarget = &aElement;
}

NS_IMETHODIMP
DataTransfer::AddElement(nsIDOMElement* aElement)
{
  NS_ENSURE_TRUE(aElement, NS_ERROR_NULL_POINTER);

  nsCOMPtr<Element> element = do_QueryInterface(aElement);
  NS_ENSURE_TRUE(element, NS_ERROR_INVALID_ARG);

  ErrorResult rv;
  AddElement(*element, rv);
  return rv.StealNSResult();
}

nsresult
DataTransfer::Clone(nsISupports* aParent, EventMessage aEventMessage,
                    bool aUserCancelled, bool aIsCrossDomainSubFrameDrop,
                    DataTransfer** aNewDataTransfer)
{
  RefPtr<DataTransfer> newDataTransfer =
    new DataTransfer(aParent, aEventMessage, mEffectAllowed, mCursorState,
                     mIsExternal, aUserCancelled, aIsCrossDomainSubFrameDrop,
                     mClipboardType, mItems, mDragImage, mDragImageX,
                     mDragImageY);

  newDataTransfer.forget(aNewDataTransfer);
  return NS_OK;
}

already_AddRefed<nsISupportsArray>
DataTransfer::GetTransferables(nsIDOMNode* aDragTarget)
{
  MOZ_ASSERT(aDragTarget);

  nsCOMPtr<nsINode> dragNode = do_QueryInterface(aDragTarget);
  if (!dragNode) {
    return nullptr;
  }

  nsIDocument* doc = dragNode->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  return GetTransferables(doc->GetLoadContext());
}

already_AddRefed<nsISupportsArray>
DataTransfer::GetTransferables(nsILoadContext* aLoadContext)
{

  nsCOMPtr<nsISupportsArray> transArray =
    do_CreateInstance("@mozilla.org/supports-array;1");
  if (!transArray) {
    return nullptr;
  }

  uint32_t count = MozItemCount();
  for (uint32_t i = 0; i < count; i++) {
    nsCOMPtr<nsITransferable> transferable = GetTransferable(i, aLoadContext);
    if (transferable) {
      transArray->AppendElement(transferable);
    }
  }

  return transArray.forget();
}

already_AddRefed<nsITransferable>
DataTransfer::GetTransferable(uint32_t aIndex, nsILoadContext* aLoadContext)
{
  if (aIndex >= MozItemCount()) {
    return nullptr;
  }

  const nsTArray<RefPtr<DataTransferItem>>& item = *mItems->MozItemsAt(aIndex);
  uint32_t count = item.Length();
  if (!count) {
    return nullptr;
  }

  nsCOMPtr<nsITransferable> transferable =
    do_CreateInstance("@mozilla.org/widget/transferable;1");
  if (!transferable) {
    return nullptr;
  }
  transferable->Init(aLoadContext);

  nsCOMPtr<nsIStorageStream> storageStream;
  nsCOMPtr<nsIBinaryOutputStream> stream;

  bool added = false;
  bool handlingCustomFormats = true;
  uint32_t totalCustomLength = 0;

  const char* knownFormats[] = {
    kTextMime, kHTMLMime, kNativeHTMLMime, kRTFMime,
    kURLMime, kURLDataMime, kURLDescriptionMime, kURLPrivateMime,
    kPNGImageMime, kJPEGImageMime, kGIFImageMime, kNativeImageMime,
    kFileMime, kFilePromiseMime, kFilePromiseURLMime,
    kFilePromiseDestFilename, kFilePromiseDirectoryMime,
    kMozTextInternal, kHTMLContext, kHTMLInfo };

  /*
   * Two passes are made here to iterate over all of the types. First, look for
   * any types that are not in the list of known types. For this pass,
   * handlingCustomFormats will be true. Data that corresponds to unknown types
   * will be pulled out and inserted into a single type (kCustomTypesMime) by
   * writing the data into a stream.
   *
   * The second pass will iterate over the formats looking for known types.
   * These are added as is. The unknown types are all then inserted as a single
   * type (kCustomTypesMime) in the same position of the first custom type. This
   * model is used to maintain the format order as best as possible.
   *
   * The format of the kCustomTypesMime type is one or more of the following
   * stored sequentially:
   *   <32-bit> type (only none or string is supported)
   *   <32-bit> length of format
   *   <wide string> format
   *   <32-bit> length of data
   *   <wide string> data
   * A type of eCustomClipboardTypeId_None ends the list, without any following
   * data.
   */
  do {
    for (uint32_t f = 0; f < count; f++) {
      RefPtr<DataTransferItem> formatitem = item[f];
      nsCOMPtr<nsIVariant> variant = formatitem->DataNoSecurityCheck();
      if (!variant) { // skip empty items
        continue;
      }

      nsAutoString type;
      formatitem->GetType(type);

      // If the data is of one of the well-known formats, use it directly.
      bool isCustomFormat = true;
      for (uint32_t f = 0; f < ArrayLength(knownFormats); f++) {
        if (type.EqualsASCII(knownFormats[f])) {
          isCustomFormat = false;
          break;
        }
      }

      uint32_t lengthInBytes;
      nsCOMPtr<nsISupports> convertedData;

      if (handlingCustomFormats) {
        if (!ConvertFromVariant(variant, getter_AddRefs(convertedData),
                                &lengthInBytes)) {
          continue;
        }

        // When handling custom types, add the data to the stream if this is a
        // custom type.
        if (isCustomFormat) {
          // If it isn't a string, just ignore it. The dataTransfer is cached in
          // the drag sesion during drag-and-drop, so non-strings will be
          // available when dragging locally.
          nsCOMPtr<nsISupportsString> str(do_QueryInterface(convertedData));
          if (str) {
            nsAutoString data;
            str->GetData(data);

            if (!stream) {
              // Create a storage stream to write to.
              NS_NewStorageStream(1024, UINT32_MAX, getter_AddRefs(storageStream));

              nsCOMPtr<nsIOutputStream> outputStream;
              storageStream->GetOutputStream(0, getter_AddRefs(outputStream));

              stream = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
              stream->SetOutputStream(outputStream);
            }

            int32_t formatLength = type.Length() * sizeof(nsString::char_type);

            stream->Write32(eCustomClipboardTypeId_String);
            stream->Write32(formatLength);
            stream->WriteBytes((const char *)type.get(),
                               formatLength);
            stream->Write32(lengthInBytes);
            stream->WriteBytes((const char *)data.get(), lengthInBytes);

            // The total size of the stream is the format length, the data
            // length, two integers to hold the lengths and one integer for the
            // string flag.
            totalCustomLength +=
              formatLength + lengthInBytes + (sizeof(uint32_t) * 3);
          }
        }
      } else if (isCustomFormat && stream) {
        // This is the second pass of the loop (handlingCustomFormats is false).
        // When encountering the first custom format, append all of the stream
        // at this position.

        // Write out a terminator.
        totalCustomLength += sizeof(uint32_t);
        stream->Write32(eCustomClipboardTypeId_None);

        nsCOMPtr<nsIInputStream> inputStream;
        storageStream->NewInputStream(0, getter_AddRefs(inputStream));

        RefPtr<nsStringBuffer> stringBuffer =
          nsStringBuffer::Alloc(totalCustomLength + 1);

        // Read the data from the string and add a null-terminator as ToString
        // needs it.
        uint32_t amountRead;
        inputStream->Read(static_cast<char*>(stringBuffer->Data()),
                          totalCustomLength, &amountRead);
        static_cast<char*>(stringBuffer->Data())[amountRead] = 0;

        nsCString str;
        stringBuffer->ToString(totalCustomLength, str);
        nsCOMPtr<nsISupportsCString>
          strSupports(do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID));
        strSupports->SetData(str);

        nsresult rv = transferable->SetTransferData(kCustomTypesMime,
                                                    strSupports,
                                                    totalCustomLength);
        if (NS_FAILED(rv)) {
          return nullptr;
        }

        added = true;

        // Clear the stream so it doesn't get used again.
        stream = nullptr;
      } else {
        // This is the second pass of the loop and a known type is encountered.
        // Add it as is.
        if (!ConvertFromVariant(variant, getter_AddRefs(convertedData),
                                &lengthInBytes)) {
          continue;
        }

        // The underlying drag code uses text/unicode, so use that instead of
        // text/plain
        const char* format;
        NS_ConvertUTF16toUTF8 utf8format(type);
        if (utf8format.EqualsLiteral(kTextMime)) {
          format = kUnicodeMime;
        } else {
          format = utf8format.get();
        }

        // If a converter is set for a format, set the converter for the
        // transferable and don't add the item
        nsCOMPtr<nsIFormatConverter> converter =
          do_QueryInterface(convertedData);
        if (converter) {
          transferable->AddDataFlavor(format);
          transferable->SetConverter(converter);
          continue;
        }

        nsresult rv = transferable->SetTransferData(format, convertedData,
                                                    lengthInBytes);
        if (NS_FAILED(rv)) {
          return nullptr;
        }

        added = true;
      }
    }

    handlingCustomFormats = !handlingCustomFormats;
  } while (!handlingCustomFormats);

  // only return the transferable if data was successfully added to it
  if (added) {
    return transferable.forget();
  }

  return nullptr;
}

bool
DataTransfer::ConvertFromVariant(nsIVariant* aVariant,
                                 nsISupports** aSupports,
                                 uint32_t* aLength) const
{
  *aSupports = nullptr;
  *aLength = 0;

  uint16_t type;
  aVariant->GetDataType(&type);
  if (type == nsIDataType::VTYPE_INTERFACE ||
      type == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> data;
    if (NS_FAILED(aVariant->GetAsISupports(getter_AddRefs(data)))) {
      return false;
    }

    nsCOMPtr<nsIFlavorDataProvider> fdp = do_QueryInterface(data);
    if (fdp) {
      // for flavour data providers, use kFlavorHasDataProvider (which has the
      // value 0) as the length.
      fdp.forget(aSupports);
      *aLength = nsITransferable::kFlavorHasDataProvider;
    }
    else {
      // wrap the item in an nsISupportsInterfacePointer
      nsCOMPtr<nsISupportsInterfacePointer> ptrSupports =
        do_CreateInstance(NS_SUPPORTS_INTERFACE_POINTER_CONTRACTID);
      if (!ptrSupports) {
        return false;
      }

      ptrSupports->SetData(data);
      ptrSupports.forget(aSupports);

      *aLength = sizeof(nsISupportsInterfacePointer *);
    }

    return true;
  }

  char16_t* chrs;
  uint32_t len = 0;
  nsresult rv = aVariant->GetAsWStringWithSize(&len, &chrs);
  if (NS_FAILED(rv)) {
    return false;
  }

  nsAutoString str;
  str.Adopt(chrs, len);

  nsCOMPtr<nsISupportsString>
    strSupports(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!strSupports) {
    return false;
  }

  strSupports->SetData(str);

  strSupports.forget(aSupports);

  // each character is two bytes
  *aLength = str.Length() << 1;

  return true;
}

void
DataTransfer::ClearAll()
{
  mItems->ClearAllItems();
}

uint32_t
DataTransfer::MozItemCount() const
{
  return mItems->MozItemCount();
}

nsresult
DataTransfer::SetDataWithPrincipal(const nsAString& aFormat,
                                   nsIVariant* aData,
                                   uint32_t aIndex,
                                   nsIPrincipal* aPrincipal)
{
  nsAutoString format;
  GetRealFormat(aFormat, format);

  ErrorResult rv;
  RefPtr<DataTransferItem> item =
    mItems->SetDataWithPrincipal(format, aData, aIndex, aPrincipal,
                                 /* aInsertOnly = */ false,
                                 /* aHidden= */ false,
                                 rv);
  return rv.StealNSResult();
}

void
DataTransfer::SetDataWithPrincipalFromOtherProcess(const nsAString& aFormat,
                                                   nsIVariant* aData,
                                                   uint32_t aIndex,
                                                   nsIPrincipal* aPrincipal,
                                                   bool aHidden)
{
  if (aFormat.EqualsLiteral(kCustomTypesMime)) {
    FillInExternalCustomTypes(aData, aIndex, aPrincipal);
  } else {
    nsAutoString format;
    GetRealFormat(aFormat, format);

    ErrorResult rv;
    RefPtr<DataTransferItem> item =
      mItems->SetDataWithPrincipal(format, aData, aIndex, aPrincipal,
                                   /* aInsertOnly = */ false, aHidden, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }
  }
}

void
DataTransfer::GetRealFormat(const nsAString& aInFormat,
                            nsAString& aOutFormat) const
{
  // treat text/unicode as equivalent to text/plain
  nsAutoString lowercaseFormat;
  nsContentUtils::ASCIIToLower(aInFormat, lowercaseFormat);
  if (lowercaseFormat.EqualsLiteral("text") ||
      lowercaseFormat.EqualsLiteral("text/unicode")) {
    aOutFormat.AssignLiteral("text/plain");
    return;
  }

  if (lowercaseFormat.EqualsLiteral("url")) {
    aOutFormat.AssignLiteral("text/uri-list");
    return;
  }

  aOutFormat.Assign(lowercaseFormat);
}

nsresult
DataTransfer::CacheExternalData(const char* aFormat, uint32_t aIndex,
                                nsIPrincipal* aPrincipal, bool aHidden)
{
  ErrorResult rv;
  RefPtr<DataTransferItem> item;

  if (strcmp(aFormat, kUnicodeMime) == 0) {
    item = mItems->SetDataWithPrincipal(NS_LITERAL_STRING("text/plain"), nullptr,
                                        aIndex, aPrincipal, false, aHidden, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
    return NS_OK;
  }

  if (strcmp(aFormat, kURLDataMime) == 0) {
    item = mItems->SetDataWithPrincipal(NS_LITERAL_STRING("text/uri-list"), nullptr,
                                        aIndex, aPrincipal, false, aHidden, rv);
    if (NS_WARN_IF(rv.Failed())) {
      return rv.StealNSResult();
    }
    return NS_OK;
  }

  nsAutoString format;
  GetRealFormat(NS_ConvertUTF8toUTF16(aFormat), format);
  item = mItems->SetDataWithPrincipal(format, nullptr, aIndex,
                                      aPrincipal, false, aHidden, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }
  return NS_OK;
}

// there isn't a way to get a list of the formats that might be available on
// all platforms, so just check for the types that can actually be imported
// XXXndeakin there are some other formats but those are platform specific.
const char* kFormats[] = { kFileMime, kHTMLMime, kURLMime, kURLDataMime,
                           kUnicodeMime, kPNGImageMime, kJPEGImageMime,
                           kGIFImageMime };

void
DataTransfer::CacheExternalDragFormats()
{
  // Called during the constructor to cache the formats available from an
  // external drag. The data associated with each format will be set to null.
  // This data will instead only be retrieved in FillInExternalDragData when
  // asked for, as it may be time consuming for the source application to
  // generate it.

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession) {
    return;
  }

  // make sure that the system principal is used for external drags
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> sysPrincipal;
  ssm->GetSystemPrincipal(getter_AddRefs(sysPrincipal));

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported
  // XXXndeakin there are some other formats but those are platform specific.
  const char* formats[] = { kFileMime, kHTMLMime, kRTFMime,
                            kURLMime, kURLDataMime, kUnicodeMime };

  uint32_t count;
  dragSession->GetNumDropItems(&count);
  for (uint32_t c = 0; c < count; c++) {
    bool hasFileData = false;
    dragSession->IsDataFlavorSupported(kFileMime, &hasFileData);

    // First, check for the special format that holds custom types.
    bool supported;
    dragSession->IsDataFlavorSupported(kCustomTypesMime, &supported);
    if (supported) {
      FillInExternalCustomTypes(c, sysPrincipal);
    }

    for (uint32_t f = 0; f < ArrayLength(formats); f++) {
      // IsDataFlavorSupported doesn't take an index as an argument and just
      // checks if any of the items support a particular flavor, even though
      // the GetData method does take an index. Here, we just assume that
      // every item being dragged has the same set of flavors.
      bool supported;
      dragSession->IsDataFlavorSupported(kFormats[f], &supported);
      // if the format is supported, add an item to the array with null as
      // the data. When retrieved, GetRealData will read the data.
      if (supported) {
        CacheExternalData(kFormats[f], c, sysPrincipal, /* hidden = */ f && hasFileData);
      }
    }
  }
}

void
DataTransfer::CacheExternalClipboardFormats()
{
  NS_ASSERTION(mEventMessage == ePaste,
               "caching clipboard data for invalid event");

  // Called during the constructor for paste events to cache the formats
  // available on the clipboard. As with CacheExternalDragFormats, the
  // data will only be retrieved when needed.

  nsCOMPtr<nsIClipboard> clipboard =
    do_GetService("@mozilla.org/widget/clipboard;1");
  if (!clipboard || mClipboardType < 0) {
    return;
  }

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> sysPrincipal;
  ssm->GetSystemPrincipal(getter_AddRefs(sysPrincipal));

  // Check if the clipboard has any files
  bool hasFileData = false;
  const char *fileMime[] = { kFileMime };
  clipboard->HasDataMatchingFlavors(fileMime, 1, mClipboardType, &hasFileData);

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported.
  // Note that the loop below assumes that kCustomTypesMime will be first.
  const char* formats[] = { kCustomTypesMime, kFileMime, kHTMLMime, kRTFMime,
                            kURLMime, kURLDataMime, kUnicodeMime, kPNGImageMime,
                            kJPEGImageMime, kGIFImageMime };

  for (uint32_t f = 0; f < mozilla::ArrayLength(formats); ++f) {
    // check each format one at a time
    bool supported;
    clipboard->HasDataMatchingFlavors(&(formats[f]), 1, mClipboardType,
                                      &supported);
    // if the format is supported, add an item to the array with null as
    // the data. When retrieved, GetRealData will read the data.
    if (supported) {
      if (f == 0) {
        FillInExternalCustomTypes(0, sysPrincipal);
      } else {
        // If we aren't the file data, and we have file data, we want to be hidden
        CacheExternalData(formats[f], 0, sysPrincipal, /* hidden = */ f != 1 && hasFileData);
      }
    }
  }
}

void
DataTransfer::FillAllExternalData()
{
  if (mIsExternal) {
    for (uint32_t i = 0; i < MozItemCount(); ++i) {
      const nsTArray<RefPtr<DataTransferItem>>& items = *mItems->MozItemsAt(i);
      for (uint32_t j = 0; j < items.Length(); ++j) {
        MOZ_ASSERT(items[j]->Index() == i);

        items[j]->FillInExternalData();
      }
    }
  }
}

void
DataTransfer::FillInExternalCustomTypes(uint32_t aIndex,
                                        nsIPrincipal* aPrincipal)
{
  RefPtr<DataTransferItem> item = new DataTransferItem(this,
                                                       NS_LITERAL_STRING(kCustomTypesMime),
                                                       DataTransferItem::KIND_STRING);
  item->SetIndex(aIndex);

  nsCOMPtr<nsIVariant> variant = item->DataNoSecurityCheck();
  if (!variant) {
    return;
  }

  FillInExternalCustomTypes(variant, aIndex, aPrincipal);
}

void
DataTransfer::FillInExternalCustomTypes(nsIVariant* aData, uint32_t aIndex,
                                        nsIPrincipal* aPrincipal)
{
  char* chrs;
  uint32_t len = 0;
  nsresult rv = aData->GetAsStringWithSize(&len, &chrs);
  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoCString str;
  str.Adopt(chrs, len);

  nsCOMPtr<nsIInputStream> stringStream;
  NS_NewCStringInputStream(getter_AddRefs(stringStream), str);

  nsCOMPtr<nsIBinaryInputStream> stream =
    do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (!stream) {
    return;
  }

  stream->SetInputStream(stringStream);

  uint32_t type;
  do {
    stream->Read32(&type);
    if (type == eCustomClipboardTypeId_String) {
      uint32_t formatLength;
      stream->Read32(&formatLength);
      char* formatBytes;
      stream->ReadBytes(formatLength, &formatBytes);
      nsAutoString format;
      format.Adopt(reinterpret_cast<char16_t*>(formatBytes),
                   formatLength / sizeof(char16_t));

      uint32_t dataLength;
      stream->Read32(&dataLength);
      char* dataBytes;
      stream->ReadBytes(dataLength, &dataBytes);
      nsAutoString data;
      data.Adopt(reinterpret_cast<char16_t*>(dataBytes),
                 dataLength / sizeof(char16_t));

      RefPtr<nsVariantCC> variant = new nsVariantCC();
      variant->SetAsAString(data);

      SetDataWithPrincipal(format, variant, aIndex, aPrincipal);
    }
  } while (type != eCustomClipboardTypeId_None);
}

} // namespace dom
} // namespace mozilla
