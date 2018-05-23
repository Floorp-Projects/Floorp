/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/CheckedInt.h"

#include "DataTransfer.h"

#include "nsISupportsPrimitives.h"
#include "nsIScriptSecurityManager.h"
#include "mozilla/dom/DOMStringList.h"
#include "nsArray.h"
#include "nsError.h"
#include "nsIDragService.h"
#include "nsIClipboard.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(DataTransfer)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DataTransfer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DataTransfer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DataTransfer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::DataTransfer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
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

// The dom.events.dataTransfer.protected.enabled preference controls whether or
// not the `protected` dataTransfer state is enabled. If the `protected`
// dataTransfer stae is disabled, then the DataTransfer will be read-only
// whenever it should be protected, and will not be disconnected after a drag
// event is completed.
static bool
PrefProtected()
{
  static bool sInitialized = false;
  static bool sValue = false;
  if (!sInitialized) {
    sInitialized = true;
    Preferences::AddBoolVarCache(&sValue, "dom.events.dataTransfer.protected.enabled");
  }
  return sValue;
}

static DataTransfer::Mode
ModeForEvent(EventMessage aEventMessage)
{
  switch (aEventMessage) {
  case eCut:
  case eCopy:
  case eDragStart:
    // For these events, we want to be able to add data to the data transfer,
    // Otherwise, the data is already present.
    return DataTransfer::Mode::ReadWrite;
  case eDrop:
  case ePaste:
  case ePasteNoFormatting:
    // For these events we want to be able to read the data which is stored in
    // the DataTransfer, rather than just the type information.
    return DataTransfer::Mode::ReadOnly;
  default:
    return PrefProtected()
      ? DataTransfer::Mode::Protected
      : DataTransfer::Mode::ReadOnly;
  }
}

DataTransfer::DataTransfer(nsISupports* aParent, EventMessage aEventMessage,
                           bool aIsExternal, int32_t aClipboardType)
  : mParent(aParent)
  , mDropEffect(nsIDragService::DRAGDROP_ACTION_NONE)
  , mEffectAllowed(nsIDragService::DRAGDROP_ACTION_UNINITIALIZED)
  , mEventMessage(aEventMessage)
  , mCursorState(false)
  , mMode(ModeForEvent(aEventMessage))
  , mIsExternal(aIsExternal)
  , mUserCancelled(false)
  , mIsCrossDomainSubFrameDrop(false)
  , mClipboardType(aClipboardType)
  , mDragImageX(0)
  , mDragImageY(0)
{
  mItems = new DataTransferItemList(this, aIsExternal);

  // For external usage, cache the data from the native clipboard or drag.
  if (mIsExternal && mMode != Mode::ReadWrite) {
    if (aEventMessage == ePasteNoFormatting) {
      mEventMessage = ePaste;
      CacheExternalClipboardFormats(true);
    } else if (aEventMessage == ePaste) {
      CacheExternalClipboardFormats(false);
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
  , mMode(ModeForEvent(aEventMessage))
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
DataTransfer::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{

  RefPtr<DataTransfer> transfer = new DataTransfer(aGlobal.GetAsSupports(),
                                                   eCopy, /* is external */ false, /* clipboard type */ -1);
  transfer->mEffectAllowed = nsIDragService::DRAGDROP_ACTION_NONE;
  return transfer.forget();
}

JSObject*
DataTransfer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DataTransferBinding::Wrap(aCx, this, aGivenProto);
}

void
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
}

void
DataTransfer::SetEffectAllowed(const nsAString& aEffectAllowed)
{
  if (aEffectAllowed.EqualsLiteral("uninitialized")) {
    mEffectAllowed = nsIDragService::DRAGDROP_ACTION_UNINITIALIZED;
    return;
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
}

void
DataTransfer::GetMozTriggeringPrincipalURISpec(nsAString& aPrincipalURISpec)
{
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession) {
    aPrincipalURISpec.Truncate(0);
    return;
  }

  nsCString principalURISpec;
  dragSession->GetTriggeringPrincipalURISpec(principalURISpec);
  CopyUTF8toUTF16(principalURISpec, aPrincipalURISpec);
}

already_AddRefed<FileList>
DataTransfer::GetFiles(nsIPrincipal& aSubjectPrincipal)
{
  return mItems->Files(&aSubjectPrincipal);
}

void
DataTransfer::GetTypes(nsTArray<nsString>& aTypes, CallerType aCallerType) const
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

    if (item->ChromeOnly() && aCallerType != CallerType::System) {
      continue;
    }

    // NOTE: The reason why we get the internal type here is because we want
    // kFileMime to appear in the types list for backwards compatibility
    // reasons.
    nsAutoString type;
    item->GetInternalType(type);
    if (item->Kind() != DataTransferItem::KIND_FILE || type.EqualsASCII(kFileMime)) {
      // If the entry has kind KIND_STRING or KIND_OTHER we want to add it to the list.
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
                      nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  // return an empty string if data for the format was not found
  aData.Truncate();

  nsCOMPtr<nsIVariant> data;
  nsresult rv =
    GetDataAtInternal(aFormat, 0, &aSubjectPrincipal,
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
                      nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  RefPtr<nsVariantCC> variant = new nsVariantCC();
  variant->SetAsAString(aData);

  aRv = SetDataAtInternal(aFormat, variant, 0, &aSubjectPrincipal);
}

void
DataTransfer::ClearData(const Optional<nsAString>& aFormat,
                        nsIPrincipal& aSubjectPrincipal,
                        ErrorResult& aRv)
{
  if (IsReadOnly()) {
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

void
DataTransfer::SetMozCursor(const nsAString& aCursorState)
{
  // Lock the cursor to an arrow during the drag.
  mCursorState = aCursorState.EqualsLiteral("default");
}

already_AddRefed<nsINode>
DataTransfer::GetMozSourceNode()
{
  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (!dragSession) {
    return nullptr;
  }

  nsCOMPtr<nsINode> sourceNode;
  dragSession->GetSourceNode(getter_AddRefs(sourceNode));
  if (sourceNode && !nsContentUtils::LegacyIsCallerNativeCode()
      && !nsContentUtils::CanCallerAccess(sourceNode)) {
    return nullptr;
  }

  return sourceNode.forget();
}

already_AddRefed<DOMStringList>
DataTransfer::MozTypesAt(uint32_t aIndex, CallerType aCallerType,
                         ErrorResult& aRv) const
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
      if (items[i]->ChromeOnly() && aCallerType != CallerType::System) {
        continue;
      }

      // NOTE: The reason why we get the internal type here is because we want
      // kFileMime to appear in the types list for backwards compatibility
      // reasons.
      nsAutoString type;
      items[i]->GetInternalType(type);
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
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aRv)
{
  nsCOMPtr<nsIVariant> data;
  aRv = GetDataAtInternal(aFormat, aIndex, &aSubjectPrincipal,
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

already_AddRefed<DataTransfer>
DataTransfer::MozCloneForEvent(const nsAString& aEvent, ErrorResult& aRv)
{
  RefPtr<nsAtom> atomEvt = NS_Atomize(aEvent);
  if (!atomEvt) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  EventMessage eventMessage = nsContentUtils::GetEventMessage(atomEvt);

  RefPtr<DataTransfer> dt;
  nsresult rv = Clone(mParent, eventMessage, false, false, getter_AddRefs(dt));
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return nullptr;
  }
  return dt.forget();
}

nsresult
DataTransfer::SetDataAtInternal(const nsAString& aFormat, nsIVariant* aData,
                                uint32_t aIndex,
                                nsIPrincipal* aSubjectPrincipal)
{
  if (aFormat.IsEmpty()) {
    return NS_OK;
  }

  if (IsReadOnly()) {
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
    return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
  }

  if (!PrincipalMaySetData(aFormat, aData, aSubjectPrincipal)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return SetDataWithPrincipal(aFormat, aData, aIndex, aSubjectPrincipal);
}

void
DataTransfer::MozSetDataAt(JSContext* aCx, const nsAString& aFormat,
                           JS::Handle<JS::Value> aData, uint32_t aIndex,
                           nsIPrincipal& aSubjectPrincipal,
                           ErrorResult& aRv)
{
  nsCOMPtr<nsIVariant> data;
  aRv = nsContentUtils::XPConnect()->JSValToVariant(aCx, aData,
                                                    getter_AddRefs(data));
  if (!aRv.Failed()) {
    aRv = SetDataAtInternal(aFormat, data, aIndex, &aSubjectPrincipal);
  }
}

void
DataTransfer::MozClearDataAt(const nsAString& aFormat, uint32_t aIndex,
                             nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv)
{
  if (IsReadOnly()) {
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
                                   nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(!IsReadOnly());
  MOZ_ASSERT(aIndex < MozItemCount());
  MOZ_ASSERT(aIndex == 0 ||
             (mEventMessage != eCut && mEventMessage != eCopy &&
              mEventMessage != ePaste));

  nsAutoString format;
  GetRealFormat(aFormat, format);

  mItems->MozRemoveByTypeAt(format, aIndex, aSubjectPrincipal, aRv);
}

void
DataTransfer::SetDragImage(Element& aImage, int32_t aX, int32_t aY)
{
  if (!IsReadOnly()) {
    mDragImage = &aImage;
    mDragImageX = aX;
    mDragImageY = aY;
  }
}

void
DataTransfer::UpdateDragImage(Element& aImage, int32_t aX, int32_t aY)
{
  if (mEventMessage < eDragDropEventFirst || mEventMessage > eDragDropEventLast) {
    return;
  }

  nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
  if (dragSession) {
    dragSession->UpdateDragImage(&aImage, aX, aY);
  }
}

already_AddRefed<Promise>
DataTransfer::GetFilesAndDirectories(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aRv)
{
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

  RefPtr<FileList> files = mItems->Files(&aSubjectPrincipal);
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
                       nsIPrincipal& aSubjectPrincipal,
                       ErrorResult& aRv)
{
  // Currently we don't support directories.
  return GetFilesAndDirectories(aSubjectPrincipal, aRv);
}

void
DataTransfer::AddElement(Element& aElement, ErrorResult& aRv)
{
  if (IsReadOnly()) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  mDragTarget = &aElement;
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

already_AddRefed<nsIArray>
DataTransfer::GetTransferables(nsINode* aDragTarget)
{
  MOZ_ASSERT(aDragTarget);

  nsIDocument* doc = aDragTarget->GetComposedDoc();
  if (!doc) {
    return nullptr;
  }

  return GetTransferables(doc->GetLoadContext());
}

already_AddRefed<nsIArray>
DataTransfer::GetTransferables(nsILoadContext* aLoadContext)
{
  nsCOMPtr<nsIMutableArray> transArray = nsArray::Create();
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
  nsCOMPtr<nsIObjectOutputStream> stream;

  bool added = false;
  bool handlingCustomFormats = true;

  // When writing the custom data, we need to ensure that there is sufficient
  // space for a (uint32_t) data ending type, and the null byte character at
  // the end of the nsCString. We claim that space upfront and store it in
  // baseLength. This value will be set to zero if a write error occurs
  // indicating that the data and length are no longer valid.
  const uint32_t baseLength = sizeof(uint32_t) + 1;
  uint32_t totalCustomLength = baseLength;

  const char* knownFormats[] = {
    kTextMime, kHTMLMime, kNativeHTMLMime, kRTFMime,
    kURLMime, kURLDataMime, kURLDescriptionMime, kURLPrivateMime,
    kPNGImageMime, kJPEGImageMime, kGIFImageMime, kNativeImageMime,
    kFileMime, kFilePromiseMime, kFilePromiseURLMime,
    kFilePromiseDestFilename, kFilePromiseDirectoryMime,
    kMozTextInternal, kHTMLContext, kHTMLInfo, kImageRequestMime };

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
      formatitem->GetInternalType(type);

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
        // custom type. If totalCustomLength is 0, then a write error occurred
        // on a previous item, so ignore any others.
        if (isCustomFormat && totalCustomLength > 0) {
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

              stream = NS_NewObjectOutputStream(outputStream);
            }

            CheckedInt<uint32_t> formatLength =
              CheckedInt<uint32_t>(type.Length()) * sizeof(nsString::char_type);

            // The total size of the stream is the format length, the data
            // length, two integers to hold the lengths and one integer for
            // the string flag. Guard against large data by ignoring any that
            // don't fit.
            CheckedInt<uint32_t> newSize = formatLength + totalCustomLength +
                                           lengthInBytes + (sizeof(uint32_t) * 3);
            if (newSize.isValid()) {
              // If a write error occurs, set totalCustomLength to 0 so that
              // further processing gets ignored.
              nsresult rv = stream->Write32(eCustomClipboardTypeId_String);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                totalCustomLength = 0;
                continue;
              }
              rv = stream->Write32(formatLength.value());
              if (NS_WARN_IF(NS_FAILED(rv))) {
                totalCustomLength = 0;
                continue;
              }
              rv = stream->WriteBytes((const char *)type.get(), formatLength.value());
              if (NS_WARN_IF(NS_FAILED(rv))) {
                totalCustomLength = 0;
                continue;
              }
              rv = stream->Write32(lengthInBytes);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                totalCustomLength = 0;
                continue;
              }
              rv = stream->WriteBytes((const char *)data.get(), lengthInBytes);
              if (NS_WARN_IF(NS_FAILED(rv))) {
                totalCustomLength = 0;
                continue;
              }

              totalCustomLength = newSize.value();
            }
          }
        }
      } else if (isCustomFormat && stream) {
        // This is the second pass of the loop (handlingCustomFormats is false).
        // When encountering the first custom format, append all of the stream
        // at this position. If totalCustomLength is 0 indicating a write error
        // occurred, or no data has been added to it, don't output anything,
        if (totalCustomLength > baseLength) {
          // Write out an end of data terminator.
          nsresult rv = stream->Write32(eCustomClipboardTypeId_None);
          if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIInputStream> inputStream;
            storageStream->NewInputStream(0, getter_AddRefs(inputStream));

            RefPtr<nsStringBuffer> stringBuffer =
              nsStringBuffer::Alloc(totalCustomLength);

            // Subtract off the null terminator when reading.
            totalCustomLength--;

            // Read the data from the stream and add a null-terminator as
            // ToString needs it.
            uint32_t amountRead;
            rv = inputStream->Read(static_cast<char*>(stringBuffer->Data()),
                              totalCustomLength, &amountRead);
            if (NS_SUCCEEDED(rv)) {
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
            }
          }
        }

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
DataTransfer::Disconnect()
{
  SetMode(Mode::Protected);
  if (PrefProtected()) {
    ClearAll();
  }
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
                                   nsIPrincipal* aPrincipal,
                                   bool aHidden)
{
  nsAutoString format;
  GetRealFormat(aFormat, format);

  ErrorResult rv;
  RefPtr<DataTransferItem> item =
    mItems->SetDataWithPrincipal(format, aData, aIndex, aPrincipal,
                                 /* aInsertOnly = */ false,
                                 aHidden,
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
  // NOTE: kFileMime must have index 0
  const char* formats[] = { kFileMime, kHTMLMime, kURLMime, kURLDataMime,
                            kUnicodeMime, kPNGImageMime };

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
      dragSession->IsDataFlavorSupported(formats[f], &supported);
      // if the format is supported, add an item to the array with null as
      // the data. When retrieved, GetRealData will read the data.
      if (supported) {
        CacheExternalData(formats[f], c, sysPrincipal, /* hidden = */ f && hasFileData);
      }
    }
  }
}

void
DataTransfer::CacheExternalClipboardFormats(bool aPlainTextOnly)
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

  if (aPlainTextOnly) {
    bool supported;
    const char* unicodeMime[] = { kUnicodeMime };
    clipboard->HasDataMatchingFlavors(unicodeMime, 1, mClipboardType,
                                      &supported);
    if (supported) {
      CacheExternalData(kUnicodeMime, 0, sysPrincipal, false);
    }
    return;
  }

  // Check if the clipboard has any files
  bool hasFileData = false;
  const char *fileMime[] = { kFileMime };
  clipboard->HasDataMatchingFlavors(fileMime, 1, mClipboardType, &hasFileData);

  // We will be ignoring any application/x-moz-file files found in the paste
  // datatransfer within e10s, as they will fail to be sent over IPC. Because of
  // that, we will unset hasFileData, whether or not it would have been set.
  // (bug 1308007)
  if (XRE_IsContentProcess()) {
    hasFileData = false;
  }

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported.
  // NOTE: kCustomTypesMime must have index 0, kFileMime index 1
  const char* formats[] = { kCustomTypesMime, kFileMime, kHTMLMime, kRTFMime,
                            kURLMime, kURLDataMime, kUnicodeMime, kPNGImageMime };

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
        // In non-e10s we support pasting files from explorer.exe.
        // Unfortunately, we fail to send that data over IPC in e10s, so we
        // don't want to add the item to the DataTransfer and end up producing a
        // null `application/x-moz-file`. (bug 1308007)
        if (XRE_IsContentProcess() && f == 1) {
          continue;
        }

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

  CheckedInt<int32_t> checkedLen(len);
  if (!checkedLen.isValid()) {
    return;
  }

  nsCOMPtr<nsIInputStream> stringStream;
  NS_NewByteInputStream(getter_AddRefs(stringStream), chrs, checkedLen.value(),
                        NS_ASSIGNMENT_ADOPT);

  nsCOMPtr<nsIObjectInputStream> stream =
    NS_NewObjectInputStream(stringStream);

  uint32_t type;
  do {
    rv = stream->Read32(&type);
    NS_ENSURE_SUCCESS_VOID(rv);
    if (type == eCustomClipboardTypeId_String) {
      uint32_t formatLength;
      rv = stream->Read32(&formatLength);
      NS_ENSURE_SUCCESS_VOID(rv);
      char* formatBytes;
      rv = stream->ReadBytes(formatLength, &formatBytes);
      NS_ENSURE_SUCCESS_VOID(rv);
      nsAutoString format;
      format.Adopt(reinterpret_cast<char16_t*>(formatBytes),
                   formatLength / sizeof(char16_t));

      uint32_t dataLength;
      rv = stream->Read32(&dataLength);
      NS_ENSURE_SUCCESS_VOID(rv);
      char* dataBytes;
      rv = stream->ReadBytes(dataLength, &dataBytes);
      NS_ENSURE_SUCCESS_VOID(rv);
      nsAutoString data;
      data.Adopt(reinterpret_cast<char16_t*>(dataBytes),
                 dataLength / sizeof(char16_t));

      RefPtr<nsVariantCC> variant = new nsVariantCC();
      rv = variant->SetAsAString(data);
      NS_ENSURE_SUCCESS_VOID(rv);

      SetDataWithPrincipal(format, variant, aIndex, aPrincipal);
    }
  } while (type != eCustomClipboardTypeId_None);
}

void
DataTransfer::SetMode(DataTransfer::Mode aMode)
{
  if (!PrefProtected() && aMode == Mode::Protected) {
    mMode = Mode::ReadOnly;
  } else {
    mMode = aMode;
  }
}

} // namespace dom
} // namespace mozilla
