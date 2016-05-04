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
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/FileList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/OSFileSystem.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            TransferItem& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  ImplCycleCollectionTraverse(aCallback, aField.mData, aName, aFlags);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(DataTransfer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DataTransfer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFileList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mItems)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDragTarget)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDragImage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(DataTransfer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFileList)
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

FileList*
DataTransfer::GetFiles(ErrorResult& aRv)
{
  return GetFileListInternal(aRv, nsContentUtils::SubjectPrincipal());
}

FileList*
DataTransfer::GetFileListInternal(ErrorResult& aRv,
                                  nsIPrincipal* aSubjectPrincipal)
{
  if (mEventMessage != eDrop &&
      mEventMessage != eLegacyDragDrop &&
      mEventMessage != ePaste) {
    return nullptr;
  }

  if (!mFileList) {
    mFileList = new FileList(static_cast<nsIDOMDataTransfer*>(this));

    uint32_t count = mItems.Length();

    for (uint32_t i = 0; i < count; i++) {
      nsCOMPtr<nsIVariant> variant;
      aRv = GetDataAtInternal(NS_ConvertUTF8toUTF16(kFileMime), i,
                              aSubjectPrincipal, getter_AddRefs(variant));
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      if (!variant) {
        continue;
      }

      nsCOMPtr<nsISupports> supports;
      nsresult rv = variant->GetAsISupports(getter_AddRefs(supports));

      if (NS_WARN_IF(NS_FAILED(rv))) {
        continue;
      }

      nsCOMPtr<nsIFile> file = do_QueryInterface(supports);

      RefPtr<File> domFile;
      if (file) {
        MOZ_ASSERT(XRE_GetProcessType() == GeckoProcessType_Default,
                   "nsIFile objects are not expected on the content process");

        bool isDir;
        aRv = file->IsDirectory(&isDir);
        if (NS_WARN_IF(aRv.Failed())) {
          return nullptr;
        }

        if (isDir) {
          continue;
        }

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

      mFileList->Append(domFile);
    }
  }

  return mFileList;
}

NS_IMETHODIMP
DataTransfer::GetFiles(nsIDOMFileList** aFileList)
{
  ErrorResult rv;
  NS_IF_ADDREF(*aFileList =
    GetFileListInternal(rv, nsContentUtils::GetSystemPrincipal()));
  return rv.StealNSResult();
}

already_AddRefed<DOMStringList>
DataTransfer::Types() const
{
  ErrorResult rv;
  return MozTypesAt(0, rv);
}

NS_IMETHODIMP
DataTransfer::GetTypes(nsISupports** aTypes)
{
  RefPtr<DOMStringList> types = Types();
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
  nsresult rv =
    GetDataAtInternal(aFormat, 0, nsContentUtils::SubjectPrincipal(),
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
  RefPtr<nsVariantCC> variant = new nsVariantCC();
  variant->SetAsAString(aData);

  aRv = SetDataAtInternal(aFormat, variant, 0,
                          nsContentUtils::SubjectPrincipal());
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
  if (aIndex < mItems.Length()) {
    bool addFile = false;
    // note that you can retrieve the types regardless of their principal
    const nsTArray<TransferItem>& item = mItems[aIndex];
    for (uint32_t i = 0; i < item.Length(); i++) {
      const nsString& format = item[i].mFormat;
      types->Add(format);
      if (!addFile) {
        addFile = format.EqualsASCII(kFileMime);
      }
    }

    if (addFile) {
      // If this is a content caller, and a file is in the data transfer, remove
      // the non-file types. This prevents alternate text forms of the file
      // from being returned.
      if (!nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
        types->Clear();
        types->Add(NS_LITERAL_STRING(kFileMime));
      }

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

  // If this is a content caller, and a file is in the data transfer, only
  // return the file type.
  if (!format.EqualsLiteral(kFileMime) &&
      !nsContentUtils::IsSystemPrincipal(aSubjectPrincipal)) {
    uint32_t count = item.Length();
    for (uint32_t i = 0; i < count; i++) {
      if (item[i].mFormat.EqualsLiteral(kFileMime)) {
        return NS_OK;
      }
    }
  }

  // Check if the caller is allowed to access the drag data. Callers with
  // chrome privileges can always read the data. During the
  // drop event, allow retrieving the data except in the case where the
  // source of the drag is in a child frame of the caller. In that case,
  // we only allow access to data of the same principal. During other events,
  // only allow access to the data with the same principal.
  bool checkFormatItemPrincipal = mIsCrossDomainSubFrameDrop ||
      (mEventMessage != eDrop && mEventMessage != eLegacyDragDrop &&
       mEventMessage != ePaste);

  uint32_t count = item.Length();
  for (uint32_t i = 0; i < count; i++) {
    TransferItem& formatitem = item[i];
    if (formatitem.mFormat.Equals(format)) {
      if (formatitem.mPrincipal && checkFormatItemPrincipal &&
          !aSubjectPrincipal->Subsumes(formatitem.mPrincipal)) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }

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
          NS_ENSURE_TRUE(aSubjectPrincipal->Subsumes(dataPrincipal),
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
  aRv = GetDataAtInternal(aFormat, aIndex, nsContentUtils::SubjectPrincipal(),
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
  if (aIndex > mItems.Length()) {
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

  // Don't allow non-chrome to add non-string or file data. We'll block file
  // promises as well which are used internally for drags to the desktop.
  if (!nsContentUtils::IsSystemPrincipal(aSubjectPrincipal)) {
    if (aFormat.EqualsLiteral(kFilePromiseMime) ||
        aFormat.EqualsLiteral(kFileMime)) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    uint16_t type;
    aData->GetDataType(&type);
    if (type == nsIDataType::VTYPE_INTERFACE ||
        type == nsIDataType::VTYPE_INTERFACE_IS) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  return SetDataWithPrincipal(aFormat, aData, aIndex, aSubjectPrincipal);
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
    aRv = SetDataAtInternal(aFormat, data, aIndex,
                            nsContentUtils::SubjectPrincipal());
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
          (NS_FAILED(principal->Subsumes(formatitem.mPrincipal, &subsumes)) ||
           !subsumes)) {
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }

      item.RemoveElementAt(i);

      // if a format was specified, break out. Otherwise, loop around until
      // all formats have been removed
      if (!clearall) {
        break;
      }
    }
  }

  // if the last format for an item is removed, remove the entire item
  if (!item.Length()) {
     mItems.RemoveElementAt(aIndex);
  }
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

  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (!mFileList) {
    GetFiles(aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  Sequence<RefPtr<File>> filesSeq;
  mFileList->ToSequence(filesSeq, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  p->MaybeResolve(filesSeq);

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
      const TransferItem& formatitem = item[f];
      if (!formatitem.mData) { // skip empty items
        continue;
      }

      // If the data is of one of the well-known formats, use it directly.
      bool isCustomFormat = true;
      for (uint32_t f = 0; f < ArrayLength(knownFormats); f++) {
        if (formatitem.mFormat.EqualsASCII(knownFormats[f])) {
          isCustomFormat = false;
          break;
        }
      }

      uint32_t lengthInBytes;
      nsCOMPtr<nsISupports> convertedData;

      if (handlingCustomFormats) {
        if (!ConvertFromVariant(formatitem.mData, getter_AddRefs(convertedData),
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

            int32_t formatLength =
              formatitem.mFormat.Length() * sizeof(nsString::char_type);

            stream->Write32(eCustomClipboardTypeId_String);
            stream->Write32(formatLength);
            stream->WriteBytes((const char *)formatitem.mFormat.get(),
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
        if (!ConvertFromVariant(formatitem.mData, getter_AddRefs(convertedData),
                                &lengthInBytes)) {
          continue;
        }

        // The underlying drag code uses text/unicode, so use that instead of
        // text/plain
        const char* format;
        NS_ConvertUTF16toUTF8 utf8format(formatitem.mFormat);
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
            (NS_FAILED(aPrincipal->Subsumes(itemformat.mPrincipal,
                                            &subsumes)) || !subsumes)) {
          return NS_ERROR_DOM_SECURITY_ERR;
        }

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
DataTransfer::SetDataWithPrincipalFromOtherProcess(const nsAString& aFormat,
                                                   nsIVariant* aData,
                                                   uint32_t aIndex,
                                                   nsIPrincipal* aPrincipal)
{
  if (aFormat.EqualsLiteral(kCustomTypesMime)) {
    FillInExternalCustomTypes(aData, aIndex, aPrincipal);
  } else {
    SetDataWithPrincipal(aFormat, aData, aIndex, aPrincipal);
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

void
DataTransfer::CacheExternalData(const char* aFormat, uint32_t aIndex,
                                nsIPrincipal* aPrincipal)
{
  if (strcmp(aFormat, kUnicodeMime) == 0) {
    SetDataWithPrincipal(NS_LITERAL_STRING("text/plain"), nullptr, aIndex,
                         aPrincipal);
    return;
  }

  if (strcmp(aFormat, kURLDataMime) == 0) {
    SetDataWithPrincipal(NS_LITERAL_STRING("text/uri-list"), nullptr, aIndex,
                         aPrincipal);
    return;
  }

  SetDataWithPrincipal(NS_ConvertUTF8toUTF16(aFormat), nullptr, aIndex,
                       aPrincipal);
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
  const char* formats[] = { kFileMime, kHTMLMime, kRTFMime,
                            kURLMime, kURLDataMime, kUnicodeMime };

  uint32_t count;
  dragSession->GetNumDropItems(&count);
  for (uint32_t c = 0; c < count; c++) {
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

  nsCOMPtr<nsIClipboard> clipboard =
    do_GetService("@mozilla.org/widget/clipboard;1");
  if (!clipboard || mClipboardType < 0) {
    return;
  }

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> sysPrincipal;
  ssm->GetSystemPrincipal(getter_AddRefs(sysPrincipal));

  // there isn't a way to get a list of the formats that might be available on
  // all platforms, so just check for the types that can actually be imported.
  // Note that the loop below assumes that kCustomTypesMime will be first.
  const char* formats[] = { kCustomTypesMime, kFileMime, kHTMLMime, kRTFMime,
                            kURLMime, kURLDataMime, kUnicodeMime };

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
        CacheExternalData(formats[f], 0, sysPrincipal);
      }
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
  if (strcmp(format, "text/plain") == 0) {
    format = kUnicodeMime;
  } else if (strcmp(format, "text/uri-list") == 0) {
    format = kURLDataMime;
  }

  nsCOMPtr<nsITransferable> trans =
    do_CreateInstance("@mozilla.org/widget/transferable;1");
  if (!trans) {
    return;
  }

  trans->Init(nullptr);
  trans->AddDataFlavor(format);

  if (mEventMessage == ePaste) {
    MOZ_ASSERT(aIndex == 0, "index in clipboard must be 0");

    nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1");
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

  RefPtr<nsVariantCC> variant = new nsVariantCC();

  nsCOMPtr<nsISupportsString> supportsstr = do_QueryInterface(data);
  if (supportsstr) {
    nsAutoString str;
    supportsstr->GetData(str);
    variant->SetAsAString(str);
  }
  else {
    nsCOMPtr<nsISupportsCString> supportscstr = do_QueryInterface(data);
    if (supportscstr) {
      nsAutoCString str;
      supportscstr->GetData(str);
      variant->SetAsACString(str);
    } else {
      variant->SetAsISupports(data);
    }
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

void
DataTransfer::FillInExternalCustomTypes(uint32_t aIndex,
                                        nsIPrincipal* aPrincipal)
{
  TransferItem item;
  item.mFormat.AssignLiteral(kCustomTypesMime);

  FillInExternalData(item, aIndex);
  if (!item.mData) {
    return;
  }

  FillInExternalCustomTypes(item.mData, aIndex, aPrincipal);
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
  stream->SetInputStream(stringStream);
  if (!stream) {
    return;
  }

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
