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
#include "nsCRT.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsVariant.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/DataTransferBinding.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/OSFileSystem.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DataTransfer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DataTransfer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFiles)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDragTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDragImage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DataTransfer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFiles)
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
  // For these events, we want to be able to add data to the data transfer, so
  // clear the readonly state. Otherwise, the data is already present. For
  // external usage, cache the data from the native clipboard or drag.
  if (aEventMessage == eCut ||
      aEventMessage == eCopy ||
      aEventMessage == eDragStart ||
      aEventMessage == eLegacyDragGesture) {
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
                           nsTArray<nsTArray<TransferItem> >& aItems,
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
  , mItems(aItems)
  , mDragImage(aDragImage)
  , mDragImageX(aDragImageX)
  , mDragImageY(aDragImageY)
{
  MOZ_ASSERT(mParent);
  // The items are copied from aItems into mItems. There is no need to copy
  // the actual data in the items as the data transfer will be read only. The
  // draggesture and dragstart events are the only times when items are
  // modifiable, but those events should have been using the first constructor
  // above.
  NS_ASSERTION(aEventMessage != eLegacyDragGesture &&
               aEventMessage != eDragStart,
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
  nsCOMPtr<nsIAtom> eventTypeAtom = do_GetAtom(onEventType);
  if (!eventTypeAtom) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  EventMessage eventMessage = nsContentUtils::GetEventMessage(eventTypeAtom);
  nsRefPtr<DataTransfer> transfer = new DataTransfer(aGlobal.GetAsSupports(),
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
                nsIDragService::DRAGDROP_ACTION_MOVE))
        mDropEffect = e;
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

FileList*
DataTransfer::GetFiles(ErrorResult& aRv)
{
  if (mEventMessage != eDrop &&
      mEventMessage != eLegacyDragDrop &&
      mEventMessage != ePaste) {
    return nullptr;
  }

  if (!mFiles) {
    mFiles = new FileList(static_cast<nsIDOMDataTransfer*>(this));

    uint32_t count = mItems.Length();

    for (uint32_t i = 0; i < count; i++) {
      nsCOMPtr<nsIVariant> variant;
      aRv = MozGetDataAt(NS_ConvertUTF8toUTF16(kFileMime), i, getter_AddRefs(variant));
      if (aRv.Failed()) {
        return nullptr;
      }

      if (!variant)
        continue;

      nsCOMPtr<nsISupports> supports;
      nsresult rv = variant->GetAsISupports(getter_AddRefs(supports));

      if (NS_FAILED(rv))
        continue;

      nsCOMPtr<nsIFile> file = do_QueryInterface(supports);

      nsRefPtr<File> domFile;
      if (file) {
#ifdef DEBUG
        if (XRE_GetProcessType() == GeckoProcessType_Default) {
          bool isDir;
          file->IsDirectory(&isDir);
          MOZ_ASSERT(!isDir, "How did we get here?");
        }
#endif
        domFile = File::CreateFromFile(GetParentObject(), file);
      } else {
        nsCOMPtr<BlobImpl> blobImpl = do_QueryInterface(supports);
        if (!blobImpl) {
          continue;
        }

        MOZ_ASSERT(blobImpl->IsFile());

        domFile = File::Create(GetParentObject(), blobImpl);
        MOZ_ASSERT(domFile);
      }

      if (!mFiles->Append(domFile)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return nullptr;
      }
    }
  }

  return mFiles;
}

NS_IMETHODIMP
DataTransfer::GetFiles(nsIDOMFileList** aFileList)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aFileList = GetFiles(rv));
  return rv.StealNSResult();
}

already_AddRefed<DOMStringList>
DataTransfer::Types()
{
  nsRefPtr<DOMStringList> types = new DOMStringList();
  if (mItems.Length()) {
    bool addFile = false;
    const nsTArray<TransferItem>& item = mItems[0];
    for (uint32_t i = 0; i < item.Length(); i++) {
      const nsString& format = item[i].mFormat;
      types->Add(format);
      if (!addFile) {
        addFile = format.EqualsASCII(kFileMime) ||
                  format.EqualsASCII("application/x-moz-file-promise");
      }
    }

    if (addFile) {
      types->Add(NS_LITERAL_STRING("Files"));
    }
  }

  return types.forget();
}

NS_IMETHODIMP
DataTransfer::GetTypes(nsISupports** aTypes)
{
  nsRefPtr<DOMStringList> types = Types();
  types.forget(aTypes);

  return NS_OK;
}

void
DataTransfer::GetData(const nsAString& aFormat, nsAString& aData,
                      ErrorResult& aRv)
{
  // return an empty string if data for the format was not found
  aData.Truncate();

  nsCOMPtr<nsIVariant> data;
  nsresult rv = MozGetDataAt(aFormat, 0, getter_AddRefs(data));
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
          if (idx == -1)
            break;
        }
        else {
          if (idx == -1)
            aData.Assign(Substring(stringdata, lastidx));
          else
            aData.Assign(Substring(stringdata, lastidx, idx - lastidx));
          aData = nsContentUtils::TrimWhitespace<nsCRT::IsAsciiSpace>(aData, true);
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

NS_IMETHODIMP
DataTransfer::GetData(const nsAString& aFormat, nsAString& aData)
{
  ErrorResult rv;
  GetData(aFormat, aData, rv);
  return rv.StealNSResult();
}

void
DataTransfer::SetData(const nsAString& aFormat, const nsAString& aData,
                      ErrorResult& aRv)
{
  nsRefPtr<nsVariant> variant = new nsVariant();
  variant->SetAsAString(aData);

  aRv = MozSetDataAt(aFormat, variant, 0);
}

NS_IMETHODIMP
DataTransfer::SetData(const nsAString& aFormat, const nsAString& aData)
{
  ErrorResult rv;
  SetData(aFormat, aData, rv);
  return rv.StealNSResult();
}

void
DataTransfer::ClearData(const Optional<nsAString>& aFormat, ErrorResult& aRv)
{
  if (mReadOnly) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (mItems.Length() == 0) {
    return;
  }

  if (aFormat.WasPassed()) {
    MozClearDataAtHelper(aFormat.Value(), 0, aRv);
  } else {
    MozClearDataAtHelper(EmptyString(), 0, aRv);
  }
}

NS_IMETHODIMP
DataTransfer::ClearData(const nsAString& aFormat)
{
  Optional<nsAString> format;
  format = &aFormat;
  ErrorResult rv;
  ClearData(format, rv);
  return rv.StealNSResult();
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
DataTransfer::MozTypesAt(uint32_t aIndex, ErrorResult& aRv)
{
  // Only the first item is valid for clipboard events
  if (aIndex > 0 &&
      (mEventMessage == eCut || mEventMessage == eCopy ||
       mEventMessage == ePaste)) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return nullptr;
  }

  nsRefPtr<DOMStringList> types = new DOMStringList();
  if (aIndex < mItems.Length()) {
    // note that you can retrieve the types regardless of their principal
    nsTArray<TransferItem>& item = mItems[aIndex];
    for (uint32_t i = 0; i < item.Length(); i++)
      types->Add(item[i].mFormat);
  }

  return types.forget();
}

NS_IMETHODIMP
DataTransfer::MozTypesAt(uint32_t aIndex, nsISupports** aTypes)
{
  ErrorResult rv;
  nsRefPtr<DOMStringList> types = MozTypesAt(aIndex, rv);
  types.forget(aTypes);
  return rv.StealNSResult();
}

NS_IMETHODIMP
DataTransfer::MozGetDataAt(const nsAString& aFormat, uint32_t aIndex,
                           nsIVariant** aData)
{
  *aData = nullptr;

  if (aFormat.IsEmpty())
    return NS_OK;

  if (aIndex >= mItems.Length()) {
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

  nsTArray<TransferItem>& item = mItems[aIndex];

  // Check if the caller is allowed to access the drag data. Callers with
  // chrome privileges can always read the data. During the
  // drop event, allow retrieving the data except in the case where the
  // source of the drag is in a child frame of the caller. In that case,
  // we only allow access to data of the same principal. During other events,
  // only allow access to the data with the same principal.
  nsIPrincipal* principal = nullptr;
  if (mIsCrossDomainSubFrameDrop ||
      (mEventMessage != eDrop && mEventMessage != eLegacyDragDrop &&
       mEventMessage != ePaste &&
       !nsContentUtils::IsCallerChrome())) {
    principal = nsContentUtils::SubjectPrincipal();
  }

  uint32_t count = item.Length();
  for (uint32_t i = 0; i < count; i++) {
    TransferItem& formatitem = item[i];
    if (formatitem.mFormat.Equals(format)) {
      bool subsumes;
      if (formatitem.mPrincipal && principal &&
          (NS_FAILED(principal->Subsumes(formatitem.mPrincipal, &subsumes)) || !subsumes))
        return NS_ERROR_DOM_SECURITY_ERR;

      if (!formatitem.mData) {
        FillInExternalData(formatitem, aIndex);
      } else {
        nsCOMPtr<nsISupports> data;
        formatitem.mData->GetAsISupports(getter_AddRefs(data));
        // Make sure the code that is calling us is same-origin with the data.
        nsCOMPtr<EventTarget> pt = do_QueryInterface(data);
        if (pt) {
          nsresult rv = NS_OK;
          nsIScriptContext* c = pt->GetContextForEventHandlers(&rv);
          NS_ENSURE_TRUE(c && NS_SUCCEEDED(rv), NS_ERROR_DOM_SECURITY_ERR);
          nsIGlobalObject* go = c->GetGlobalObject();
          NS_ENSURE_TRUE(go, NS_ERROR_DOM_SECURITY_ERR);
          nsCOMPtr<nsIScriptObjectPrincipal> sp = do_QueryInterface(go);
          MOZ_ASSERT(sp, "This cannot fail on the main thread.");
          nsIPrincipal* dataPrincipal = sp->GetPrincipal();
          NS_ENSURE_TRUE(dataPrincipal, NS_ERROR_DOM_SECURITY_ERR);
          if (!principal) {
            principal = nsContentUtils::SubjectPrincipal();
          }
          bool equals = false;
          NS_ENSURE_TRUE(NS_SUCCEEDED(principal->Equals(dataPrincipal, &equals)) && equals,
                         NS_ERROR_DOM_SECURITY_ERR);
        }
      }
      *aData = formatitem.mData;
      NS_IF_ADDREF(*aData);
      return NS_OK;
    }
  }

  return NS_OK;
}

void
DataTransfer::MozGetDataAt(JSContext* aCx, const nsAString& aFormat,
                           uint32_t aIndex,
                           JS::MutableHandle<JS::Value> aRetval,
                           mozilla::ErrorResult& aRv)
{
  nsCOMPtr<nsIVariant> data;
  aRv = MozGetDataAt(aFormat, aIndex, getter_AddRefs(data));
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

NS_IMETHODIMP
DataTransfer::MozSetDataAt(const nsAString& aFormat, nsIVariant* aData,
                           uint32_t aIndex)
{
  if (aFormat.IsEmpty()) {
    return NS_OK;
  }

  if (mReadOnly) {
    return NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR;
  }

  // Specifying an index less than the current length will replace an existing
  // item. Specifying an index equal to the current length will add a new item.
  if (aIndex > mItems.Length()) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // Only the first item is valid for clipboard events
  if (aIndex > 0 &&
      (mEventMessage == eCut || mEventMessage == eCopy ||
       mEventMessage == ePaste)) {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }

  // don't allow non-chrome to add file data
  // XXX perhaps this should also limit any non-string type as well
  if ((aFormat.EqualsLiteral("application/x-moz-file-promise") ||
       aFormat.EqualsLiteral("application/x-moz-file")) &&
       !nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return SetDataWithPrincipal(aFormat, aData, aIndex,
                              nsContentUtils::SubjectPrincipal());
}

void
DataTransfer::MozSetDataAt(JSContext* aCx, const nsAString& aFormat,
                           JS::Handle<JS::Value> aData,
                           uint32_t aIndex, ErrorResult& aRv)
{
  nsCOMPtr<nsIVariant> data;
  aRv = nsContentUtils::XPConnect()->JSValToVariant(aCx, aData,
                                                    getter_AddRefs(data));
  if (!aRv.Failed()) {
    aRv = MozSetDataAt(aFormat, data, aIndex);
  }
}

void
DataTransfer::MozClearDataAt(const nsAString& aFormat, uint32_t aIndex,
                             ErrorResult& aRv)
{
  if (mReadOnly) {
    aRv.Throw(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR);
    return;
  }

  if (aIndex >= mItems.Length()) {
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

  MozClearDataAtHelper(aFormat, aIndex, aRv);
}

void
DataTransfer::MozClearDataAtHelper(const nsAString& aFormat, uint32_t aIndex,
                                   ErrorResult& aRv)
{
  MOZ_ASSERT(!mReadOnly);
  MOZ_ASSERT(aIndex < mItems.Length());
  MOZ_ASSERT(aIndex == 0 ||
             (mEventMessage != eCut && mEventMessage != eCopy &&
              mEventMessage != ePaste));

  nsAutoString format;
  GetRealFormat(aFormat, format);

  nsIPrincipal* principal = nsContentUtils::SubjectPrincipal();

  // if the format is empty, clear all formats
  bool clearall = format.IsEmpty();

  nsTArray<TransferItem>& item = mItems[aIndex];
  // count backwards so that the count and index don't have to be adjusted
  // after removing an element
  for (int32_t i = item.Length() - 1; i >= 0; i--) {
    TransferItem& formatitem = item[i];
    if (clearall || formatitem.mFormat.Equals(format)) {
      // don't allow removing data that has a stronger principal
      bool subsumes;
      if (formatitem.mPrincipal && principal &&
          (NS_FAILED(principal->Subsumes(formatitem.mPrincipal, &subsumes)) || !subsumes)) {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }

      item.RemoveElementAt(i);

      // if a format was specified, break out. Otherwise, loop around until
      // all formats have been removed
      if (!clearall)
        break;
    }
  }

  // if the last format for an item is removed, remove the entire item
  if (!item.Length())
     mItems.RemoveElementAt(aIndex);
}

NS_IMETHODIMP
DataTransfer::MozClearDataAt(const nsAString& aFormat, uint32_t aIndex)
{
  ErrorResult rv;
  MozClearDataAt(aFormat, aIndex, rv);
  return rv.StealNSResult();
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

static already_AddRefed<OSFileSystem>
MakeOrReuseFileSystem(const nsAString& aNewLocalRootPath,
                      OSFileSystem* aFS,
                      nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);

  nsRefPtr<OSFileSystem> fs;
  if (aFS) {
    const nsAString& prevLocalRootPath = aFS->GetLocalRootPath();
    if (aNewLocalRootPath == prevLocalRootPath) {
      fs = aFS;
    }
  }
  if (!fs) {
    fs = new OSFileSystem(aNewLocalRootPath);
    fs->Init(aWindow);
  }
  return fs.forget();
}

already_AddRefed<Promise>
DataTransfer::GetFilesAndDirectories(ErrorResult& aRv)
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

  nsRefPtr<Promise> p = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mFiles) {
    GetFiles(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  Sequence<OwningFileOrDirectory> filesAndDirsSeq;

  if (mFiles && mFiles->Length()) {
    if (!filesAndDirsSeq.SetLength(mFiles->Length(), mozilla::fallible_t())) {
      p->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
      return p.forget();
    }

    nsPIDOMWindow* window = parentNode->OwnerDoc()->GetInnerWindow();

    nsRefPtr<OSFileSystem> fs;
    for (uint32_t i = 0; i < mFiles->Length(); ++i) {
      if (mFiles->Item(i)->Impl()->IsDirectory()) {
#if defined(ANDROID) || defined(MOZ_B2G)
        MOZ_ASSERT(false,
                   "Directory picking should have been redirected to normal "
                   "file picking for platforms that don't have a directory "
                   "picker");
#endif
        nsAutoString path;
        mFiles->Item(i)->GetMozFullPathInternal(path, aRv);
        if (aRv.Failed()) {
          return nullptr;
        }
        int32_t leafSeparatorIndex = path.RFind(FILE_PATH_SEPARATOR);
        nsDependentSubstring dirname = Substring(path, 0, leafSeparatorIndex);
        nsDependentSubstring basename = Substring(path, leafSeparatorIndex);
        fs = MakeOrReuseFileSystem(dirname, fs, window);
        filesAndDirsSeq[i].SetAsDirectory() = new Directory(fs, basename);
      } else {
        filesAndDirsSeq[i].SetAsFile() = mFiles->Item(i);
      }
    }
  }

  p->MaybeResolve(filesAndDirsSeq);

  return p.forget();
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
  DataTransfer* newDataTransfer =
    new DataTransfer(aParent, aEventMessage, mEffectAllowed, mCursorState,
                     mIsExternal, aUserCancelled, aIsCrossDomainSubFrameDrop,
                     mClipboardType, mItems, mDragImage, mDragImageX,
                     mDragImageY);

  *aNewDataTransfer = newDataTransfer;
  NS_ADDREF(*aNewDataTransfer);
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
    
  nsIDocument* doc = dragNode->GetCurrentDoc();
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

  uint32_t count = mItems.Length();
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
  if (aIndex >= mItems.Length()) {
    return nullptr;
  }

  nsTArray<TransferItem>& item = mItems[aIndex];
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

  bool added = false;
  for (uint32_t f = 0; f < count; f++) {
    const TransferItem& formatitem = item[f];
    if (!formatitem.mData) { // skip empty items
      continue;
    }

    uint32_t length;
    nsCOMPtr<nsISupports> convertedData;
    if (!ConvertFromVariant(formatitem.mData, getter_AddRefs(convertedData), &length)) {
      continue;
    }

    // the underlying drag code uses text/unicode, so use that instead of text/plain
    const char* format;
    NS_ConvertUTF16toUTF8 utf8format(formatitem.mFormat);
    if (utf8format.EqualsLiteral("text/plain")) {
      format = kUnicodeMime;
    } else {
      format = utf8format.get();
    }

    // if a converter is set for a format, set the converter for the
    // transferable and don't add the item
    nsCOMPtr<nsIFormatConverter> converter = do_QueryInterface(convertedData);
    if (converter) {
      transferable->AddDataFlavor(format);
      transferable->SetConverter(converter);
      continue;
    }

    nsresult rv = transferable->SetTransferData(format, convertedData, length);
    if (NS_FAILED(rv)) {
      return nullptr;
    }

    added = true;
  }

  // only return the transferable if data was successfully added to it
  if (added) {
    return transferable.forget();
  }

  return nullptr;
}

bool
DataTransfer::ConvertFromVariant(nsIVariant* aVariant,
                                 nsISupports** aSupports,
                                 uint32_t* aLength)
{
  *aSupports = nullptr;
  *aLength = 0;

  uint16_t type;
  aVariant->GetDataType(&type);
  if (type == nsIDataType::VTYPE_INTERFACE ||
      type == nsIDataType::VTYPE_INTERFACE_IS) {
    nsCOMPtr<nsISupports> data;
    if (NS_FAILED(aVariant->GetAsISupports(getter_AddRefs(data))))
       return false;
 
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
      if (!ptrSupports)
        return false;

      ptrSupports->SetData(data);
      ptrSupports.forget(aSupports);

      *aLength = sizeof(nsISupportsInterfacePointer *);
    }

    return true;
  }

  char16_t* chrs;
  uint32_t len = 0;
  nsresult rv = aVariant->GetAsWStringWithSize(&len, &chrs);
  if (NS_FAILED(rv))
    return false;

  nsAutoString str;
  str.Adopt(chrs, len);

  nsCOMPtr<nsISupportsString>
    strSupports(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  if (!strSupports)
    return false;

  strSupports->SetData(str);

  strSupports.forget(aSupports);

  // each character is two bytes
  *aLength = str.Length() << 1;

  return true;
}

void
DataTransfer::ClearAll()
{
  mItems.Clear();
}

nsresult
DataTransfer::SetDataWithPrincipal(const nsAString& aFormat,
                                   nsIVariant* aData,
                                   uint32_t aIndex,
                                   nsIPrincipal* aPrincipal)
{
  nsAutoString format;
  GetRealFormat(aFormat, format);

  // check if the item for the format already exists. In that case,
  // just replace it.
  TransferItem* formatitem;
  if (aIndex < mItems.Length()) {
    nsTArray<TransferItem>& item = mItems[aIndex];
    uint32_t count = item.Length();
    for (uint32_t i = 0; i < count; i++) {
      TransferItem& itemformat = item[i];
      if (itemformat.mFormat.Equals(format)) {
        // don't allow replacing data that has a stronger principal
        bool subsumes;
        if (itemformat.mPrincipal && aPrincipal &&
            (NS_FAILED(aPrincipal->Subsumes(itemformat.mPrincipal, &subsumes)) || !subsumes))
          return NS_ERROR_DOM_SECURITY_ERR;

        itemformat.mPrincipal = aPrincipal;
        itemformat.mData = aData;
        return NS_OK;
      }
    }

    // add a new format
    formatitem = item.AppendElement();
  }
  else {
    NS_ASSERTION(aIndex == mItems.Length(), "Index out of range");

    // add a new index
    nsTArray<TransferItem>* item = mItems.AppendElement();
    NS_ENSURE_TRUE(item, NS_ERROR_OUT_OF_MEMORY);

    formatitem = item->AppendElement();
  }

  NS_ENSURE_TRUE(formatitem, NS_ERROR_OUT_OF_MEMORY);

  formatitem->mFormat = format;
  formatitem->mPrincipal = aPrincipal;
  formatitem->mData = aData;

  return NS_OK;
}

void
DataTransfer::GetRealFormat(const nsAString& aInFormat, nsAString& aOutFormat)
{
  // treat text/unicode as equivalent to text/plain
  nsAutoString lowercaseFormat;
  nsContentUtils::ASCIIToLower(aInFormat, lowercaseFormat);
  if (lowercaseFormat.EqualsLiteral("text") || lowercaseFormat.EqualsLiteral("text/unicode"))
    aOutFormat.AssignLiteral("text/plain");
  else if (lowercaseFormat.EqualsLiteral("url"))
    aOutFormat.AssignLiteral("text/uri-list");
  else
    aOutFormat.Assign(lowercaseFormat);
}

void
DataTransfer::CacheExternalData(const char* aFormat, uint32_t aIndex, nsIPrincipal* aPrincipal)
{
  if (strcmp(aFormat, kUnicodeMime) == 0) {
    SetDataWithPrincipal(NS_LITERAL_STRING("text/plain"), nullptr, aIndex, aPrincipal);
  } else {
    if (strcmp(aFormat, kURLDataMime) == 0) {
      SetDataWithPrincipal(NS_LITERAL_STRING("text/uri-list"), nullptr, aIndex, aPrincipal);
    }
    SetDataWithPrincipal(NS_ConvertUTF8toUTF16(aFormat), nullptr, aIndex, aPrincipal);
  }
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
  if (!dragSession)
    return;

  // make sure that the system principal is used for external drags
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> sysPrincipal;
  ssm->GetSystemPrincipal(getter_AddRefs(sysPrincipal));

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported
  // XXXndeakin there are some other formats but those are platform specific.
  const char* formats[] = { kFileMime, kHTMLMime, kURLMime, kURLDataMime, kUnicodeMime };

  uint32_t count;
  dragSession->GetNumDropItems(&count);
  for (uint32_t c = 0; c < count; c++) {
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
        CacheExternalData(formats[f], c, sysPrincipal);
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

  nsCOMPtr<nsIClipboard> clipboard = do_GetService("@mozilla.org/widget/clipboard;1");
  if (!clipboard || mClipboardType < 0) {
    return;
  }

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> sysPrincipal;
  ssm->GetSystemPrincipal(getter_AddRefs(sysPrincipal));

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported
  const char* formats[] = { kFileMime, kHTMLMime, kURLMime, kURLDataMime, kUnicodeMime };

  for (uint32_t f = 0; f < mozilla::ArrayLength(formats); ++f) {
    // check each format one at a time
    bool supported;
    clipboard->HasDataMatchingFlavors(&(formats[f]), 1, mClipboardType, &supported);
    // if the format is supported, add an item to the array with null as
    // the data. When retrieved, GetRealData will read the data.
    if (supported) {
      CacheExternalData(formats[f], 0, sysPrincipal);
    }
  }
}

void
DataTransfer::FillInExternalData(TransferItem& aItem, uint32_t aIndex)
{
  NS_PRECONDITION(mIsExternal, "Not an external data transfer");

  if (aItem.mData) {
    return;
  }

  // only drag and paste events should be calling FillInExternalData
  NS_ASSERTION(mEventMessage != eCut && mEventMessage != eCopy,
               "clipboard event with empty data");

    NS_ConvertUTF16toUTF8 utf8format(aItem.mFormat);
    const char* format = utf8format.get();
    if (strcmp(format, "text/plain") == 0)
      format = kUnicodeMime;
    else if (strcmp(format, "text/uri-list") == 0)
      format = kURLDataMime;

    nsCOMPtr<nsITransferable> trans =
      do_CreateInstance("@mozilla.org/widget/transferable;1");
    if (!trans)
      return;

  trans->Init(nullptr);
  trans->AddDataFlavor(format);

  if (mEventMessage == ePaste) {
    MOZ_ASSERT(aIndex == 0, "index in clipboard must be 0");

    nsCOMPtr<nsIClipboard> clipboard = do_GetService("@mozilla.org/widget/clipboard;1");
    if (!clipboard || mClipboardType < 0) {
      return;
    }

    clipboard->GetData(trans, mClipboardType);
  } else {
    nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
    if (!dragSession) {
      return;
    }

#ifdef DEBUG
    // Since this is an external drag, the source document will always be null.
    nsCOMPtr<nsIDOMDocument> domDoc;
    dragSession->GetSourceDocument(getter_AddRefs(domDoc));
    MOZ_ASSERT(!domDoc);
#endif

    dragSession->GetData(trans, aIndex);
  }

    uint32_t length = 0;
    nsCOMPtr<nsISupports> data;
    trans->GetTransferData(format, getter_AddRefs(data), &length);
    if (!data)
      return;

    nsRefPtr<nsVariant> variant = new nsVariant();

    nsCOMPtr<nsISupportsString> supportsstr = do_QueryInterface(data);
    if (supportsstr) {
      nsAutoString str;
      supportsstr->GetData(str);
      variant->SetAsAString(str);
    }
    else {
      variant->SetAsISupports(data);
    }

    aItem.mData = variant;
  }

void
DataTransfer::FillAllExternalData()
{
  if (mIsExternal) {
    for (uint32_t i = 0; i < mItems.Length(); ++i) {
      nsTArray<TransferItem>& itemArray = mItems[i];
      for (uint32_t j = 0; j < itemArray.Length(); ++j) {
        if (!itemArray[j].mData) {
          FillInExternalData(itemArray[j], i);
        }
      }
    }
  }
}

} // namespace dom
} // namespace mozilla
