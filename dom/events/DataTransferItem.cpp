/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataTransferItem.h"
#include "DataTransferItemList.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/DataTransferItemBinding.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/FileSystem.h"
#include "mozilla/dom/FileSystemDirectoryEntry.h"
#include "mozilla/dom/FileSystemFileEntry.h"
#include "nsIClipboard.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsContentUtils.h"
#include "nsVariant.h"

namespace {

struct FileMimeNameData
{
  const char* mMimeName;
  const char* mFileName;
};

FileMimeNameData kFileMimeNameMap[] = {
  { kFileMime, "GenericFileName" },
  { kPNGImageMime, "GenericImageNamePNG" },
};

} // anonymous namespace

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DataTransferItem, mData,
                                      mPrincipal, mDataTransfer, mCachedFile)
NS_IMPL_CYCLE_COLLECTING_ADDREF(DataTransferItem)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DataTransferItem)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DataTransferItem)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
DataTransferItem::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DataTransferItemBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<DataTransferItem>
DataTransferItem::Clone(DataTransfer* aDataTransfer) const
{
  MOZ_ASSERT(aDataTransfer);

  RefPtr<DataTransferItem> it = new DataTransferItem(aDataTransfer, mType);

  // Copy over all of the fields
  it->mKind = mKind;
  it->mIndex = mIndex;
  it->mData = mData;
  it->mPrincipal = mPrincipal;
  it->mChromeOnly = mChromeOnly;

  return it.forget();
}

void
DataTransferItem::SetData(nsIVariant* aData)
{
  // Invalidate our file cache, we will regenerate it with the new data
  mCachedFile = nullptr;

  if (!aData) {
    // We are holding a temporary null which will later be filled.
    // These are provided by the system, and have guaranteed properties about
    // their kind based on their type.
    MOZ_ASSERT(!mType.IsEmpty());

    mKind = KIND_STRING;
    for (uint32_t i = 0; i < ArrayLength(kFileMimeNameMap); ++i) {
      if (mType.EqualsASCII(kFileMimeNameMap[i].mMimeName)) {
        mKind = KIND_FILE;
        break;
      }
    }

    mData = nullptr;
    return;
  }

  mData = aData;
  mKind = KindFromData(mData);
}

/* static */ DataTransferItem::eKind
DataTransferItem::KindFromData(nsIVariant* aData)
{
  nsCOMPtr<nsISupports> supports;
  nsresult rv = aData->GetAsISupports(getter_AddRefs(supports));
  if (NS_SUCCEEDED(rv) && supports) {
    // Check if we have one of the supported file data formats
    if (RefPtr<Blob>(do_QueryObject(supports)) ||
        nsCOMPtr<BlobImpl>(do_QueryInterface(supports)) ||
        nsCOMPtr<nsIFile>(do_QueryInterface(supports))) {
      return KIND_FILE;
    }
  }

  nsAutoString string;
  // If we can't get the data type as a string, that means that the object
  // should be considered to be of the "other" type. This is impossible
  // through the APIs defined by the spec, but we provide extra Moz* APIs,
  // which allow setting of non-string data. We determine whether we can
  // consider it a string, by calling GetAsAString, and checking for success.
  rv = aData->GetAsAString(string);
  if (NS_SUCCEEDED(rv)) {
    return KIND_STRING;
  }

  return KIND_OTHER;
}

void
DataTransferItem::FillInExternalData()
{
  if (mData) {
    return;
  }

  NS_ConvertUTF16toUTF8 utf8format(mType);
  const char* format = utf8format.get();
  if (strcmp(format, "text/plain") == 0) {
    format = kUnicodeMime;
  } else if (strcmp(format, "text/uri-list") == 0) {
    format = kURLDataMime;
  }

  nsCOMPtr<nsITransferable> trans =
    do_CreateInstance("@mozilla.org/widget/transferable;1");
  if (NS_WARN_IF(!trans)) {
    return;
  }

  trans->Init(nullptr);
  trans->AddDataFlavor(format);

  if (mDataTransfer->GetEventMessage() == ePaste) {
    MOZ_ASSERT(mIndex == 0, "index in clipboard must be 0");

    nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1");
    if (!clipboard || mDataTransfer->ClipboardType() < 0) {
      return;
    }

    nsresult rv = clipboard->GetData(trans, mDataTransfer->ClipboardType());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  } else {
    nsCOMPtr<nsIDragSession> dragSession = nsContentUtils::GetDragSession();
    if (!dragSession) {
      return;
    }

    nsresult rv = dragSession->GetData(trans, mIndex);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  uint32_t length = 0;
  nsCOMPtr<nsISupports> data;
  nsresult rv = trans->GetTransferData(format, getter_AddRefs(data), &length);
  if (NS_WARN_IF(NS_FAILED(rv) || !data)) {
    return;
  }

  // Fill the variant
  RefPtr<nsVariantCC> variant = new nsVariantCC();

  eKind oldKind = Kind();
  if (oldKind == KIND_FILE) {
    // Because this is an external piece of data, mType is one of kFileMime,
    // kPNGImageMime, kJPEGImageMime, or kGIFImageMime. Some of these types
    // are passed in as a nsIInputStream which must be converted to a
    // dom::File before storing.
    if (nsCOMPtr<nsIInputStream> istream = do_QueryInterface(data)) {
      RefPtr<File> file = CreateFileFromInputStream(istream);
      if (NS_WARN_IF(!file)) {
        return;
      }
      data = do_QueryObject(file);
    }

    variant->SetAsISupports(data);
  } else {
    // We have an external piece of string data. Extract it and store it in the variant
    MOZ_ASSERT(oldKind == KIND_STRING);

    nsCOMPtr<nsISupportsString> supportsstr = do_QueryInterface(data);
    if (supportsstr) {
      nsAutoString str;
      supportsstr->GetData(str);
      variant->SetAsAString(str);
    } else {
      nsCOMPtr<nsISupportsCString> supportscstr = do_QueryInterface(data);
      if (supportscstr) {
        nsAutoCString str;
        supportscstr->GetData(str);
        variant->SetAsACString(str);
      }
    }
  }

  SetData(variant);

  if (oldKind != Kind()) {
    NS_WARNING("Clipboard data provided by the OS does not match predicted kind");
    mDataTransfer->TypesListMayHaveChanged();
  }
}

void
DataTransferItem::GetType(nsAString& aType)
{
  // If we don't have a File, we can just put whatever our recorded internal
  // type is.
  if (Kind() != KIND_FILE) {
    aType = mType;
    return;
  }

  // If we do have a File, then we need to look at our File object to discover
  // what its mime type is. We can use the System Principal here, as this
  // information should be avaliable even if the data is currently inaccessible
  // (for example during a dragover).
  //
  // XXX: This seems inefficient, as it seems like we should be able to get this
  // data without getting the entire File object, which may require talking to
  // the OS.
  ErrorResult rv;
  RefPtr<File> file = GetAsFile(*nsContentUtils::GetSystemPrincipal(), rv);
  MOZ_ASSERT(!rv.Failed(), "Failed to get file data with system principal");

  // If we don't actually have a file, fall back to returning the internal type.
  if (NS_WARN_IF(!file)) {
    aType = mType;
    return;
  }

  file->GetType(aType);
}

already_AddRefed<File>
DataTransferItem::GetAsFile(nsIPrincipal& aSubjectPrincipal,
                            ErrorResult& aRv)
{
  // This is done even if we have an mCachedFile, as it performs the necessary
  // permissions checks to ensure that we are allowed to access this type.
  nsCOMPtr<nsIVariant> data = Data(&aSubjectPrincipal, aRv);
  if (NS_WARN_IF(!data || aRv.Failed())) {
    return nullptr;
  }

  // We have to check our kind after getting the data, because if we have
  // external data and the OS lied to us (which unfortunately does happen
  // sometimes), then we might not have the same type of data as we did coming
  // into this function.
  if (NS_WARN_IF(mKind != KIND_FILE)) {
    return nullptr;
  }

  // Generate the dom::File from the stored data, caching it so that the
  // same object is returned in the future.
  if (!mCachedFile) {
    nsCOMPtr<nsISupports> supports;
    aRv = data->GetAsISupports(getter_AddRefs(supports));
    MOZ_ASSERT(!aRv.Failed() && supports,
               "File objects should be stored as nsISupports variants");
    if (aRv.Failed() || !supports) {
      return nullptr;
    }

    if (RefPtr<Blob> blob = do_QueryObject(supports)) {
      mCachedFile = blob->ToFile();
    } else if (nsCOMPtr<BlobImpl> blobImpl = do_QueryInterface(supports)) {
      MOZ_ASSERT(blobImpl->IsFile());
      mCachedFile = File::Create(mDataTransfer, blobImpl);
    } else if (nsCOMPtr<nsIFile> ifile = do_QueryInterface(supports)) {
      mCachedFile = File::CreateFromFile(mDataTransfer, ifile);
    } else {
      MOZ_ASSERT(false, "One of the above code paths should be taken");
      return nullptr;
    }
  }

  RefPtr<File> file = mCachedFile;
  return file.forget();
}

already_AddRefed<FileSystemEntry>
DataTransferItem::GetAsEntry(nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv)
{
  RefPtr<File> file = GetAsFile(aSubjectPrincipal, aRv);
  if (NS_WARN_IF(aRv.Failed()) || !file) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global;
  // This is annoying, but DataTransfer may have various things as parent.
  nsCOMPtr<EventTarget> target =
    do_QueryInterface(mDataTransfer->GetParentObject());
  if (target) {
    global = target->GetOwnerGlobal();
  } else {
    RefPtr<Event> event = do_QueryObject(mDataTransfer->GetParentObject());
    if (event) {
      global = event->GetParentObject();
    }
  }

  if (!global) {
    return nullptr;
  }

  RefPtr<FileSystem> fs = FileSystem::Create(global);
  RefPtr<FileSystemEntry> entry;
  BlobImpl* impl = file->Impl();
  MOZ_ASSERT(impl);

  if (impl->IsDirectory()) {
    nsAutoString fullpath;
    impl->GetMozFullPathInternal(fullpath, aRv);
    if (aRv.Failed()) {
      aRv.SuppressException();
      return nullptr;
    }

    nsCOMPtr<nsIFile> directoryFile;
    // fullPath is already in unicode, we don't have to use
    // NS_NewNativeLocalFile.
    nsresult rv = NS_NewLocalFile(fullpath, true,
                                  getter_AddRefs(directoryFile));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    RefPtr<Directory> directory = Directory::Create(global, directoryFile);
    entry = new FileSystemDirectoryEntry(global, directory, nullptr, fs);
  } else {
    entry = new FileSystemFileEntry(global, file, nullptr, fs);
  }

  Sequence<RefPtr<FileSystemEntry>> entries;
  if (!entries.AppendElement(entry, fallible)) {
    return nullptr;
  }

  fs->CreateRoot(entries);
  return entry.forget();
}

already_AddRefed<File>
DataTransferItem::CreateFileFromInputStream(nsIInputStream* aStream)
{
  const char* key = nullptr;
  for (uint32_t i = 0; i < ArrayLength(kFileMimeNameMap); ++i) {
    if (mType.EqualsASCII(kFileMimeNameMap[i].mMimeName)) {
      key = kFileMimeNameMap[i].mFileName;
      break;
    }
  }
  if (!key) {
    MOZ_ASSERT_UNREACHABLE("Unsupported mime type");
    key = "GenericFileName";
  }

  nsAutoString fileName;
  nsresult rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   key, fileName);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  uint64_t available;
  void* data = nullptr;
  rv = NS_ReadInputStreamToBuffer(aStream, &data, -1, &available);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return File::CreateMemoryFile(mDataTransfer, data, available, fileName,
                                mType, PR_Now());
}

void
DataTransferItem::GetAsString(FunctionStringCallback* aCallback,
                              nsIPrincipal& aSubjectPrincipal,
                              ErrorResult& aRv)
{
  if (!aCallback) {
    return;
  }

  // Theoretically this should be done inside of the runnable, as it might be an
  // expensive operation on some systems, however we wouldn't get access to the
  // NS_ERROR_DOM_SECURITY_ERROR messages which may be raised by this method.
  nsCOMPtr<nsIVariant> data = Data(&aSubjectPrincipal, aRv);
  if (NS_WARN_IF(!data || aRv.Failed())) {
    return;
  }

  // We have to check our kind after getting the data, because if we have
  // external data and the OS lied to us (which unfortunately does happen
  // sometimes), then we might not have the same type of data as we did coming
  // into this function.
  if (NS_WARN_IF(mKind != KIND_STRING)) {
    return;
  }

  nsAutoString stringData;
  nsresult rv = data->GetAsAString(stringData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  // Dispatch the callback to the main thread
  class GASRunnable final : public Runnable
  {
  public:
    GASRunnable(FunctionStringCallback* aCallback, const nsAString& aStringData)
      : mozilla::Runnable("GASRunnable")
      , mCallback(aCallback)
      , mStringData(aStringData)
    {}

    NS_IMETHOD Run() override
    {
      ErrorResult rv;
      mCallback->Call(mStringData, rv);
      NS_WARNING_ASSERTION(!rv.Failed(), "callback failed");
      return rv.StealNSResult();
    }
  private:
    RefPtr<FunctionStringCallback> mCallback;
    nsString mStringData;
  };

  RefPtr<GASRunnable> runnable = new GASRunnable(aCallback, stringData);

  // DataTransfer.mParent might be EventTarget, nsIGlobalObject, ClipboardEvent
  // nsPIDOMWindowOuter, null
  nsISupports* parent = mDataTransfer->GetParentObject();
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(parent);
  if (parent && !global) {
    if (nsCOMPtr<dom::EventTarget> target = do_QueryInterface(parent)) {
      global = target->GetOwnerGlobal();
    } else if (RefPtr<Event> event = do_QueryObject(parent)) {
      global = event->GetParentObject();
    }
  }
  if (global) {
    rv = global->Dispatch(TaskCategory::Other, runnable.forget());
  } else {
    rv = NS_DispatchToMainThread(runnable);
  }
  if (NS_FAILED(rv)) {
    NS_WARNING("Dispatch to main thread Failed in "
               "DataTransferItem::GetAsString!");
  }
}

already_AddRefed<nsIVariant>
DataTransferItem::DataNoSecurityCheck()
{
  if (!mData) {
    FillInExternalData();
  }
  nsCOMPtr<nsIVariant> data = mData;
  return data.forget();
}

already_AddRefed<nsIVariant>
DataTransferItem::Data(nsIPrincipal* aPrincipal, ErrorResult& aRv)
{
  MOZ_ASSERT(aPrincipal);

  // If the inbound principal is system, we can skip the below checks, as
  // they will trivially succeed.
  if (nsContentUtils::IsSystemPrincipal(aPrincipal)) {
    return DataNoSecurityCheck();
  }

  // We should not allow raw data to be accessed from a Protected DataTransfer.
  // We don't prevent this access if the accessing document is Chrome.
  if (mDataTransfer->IsProtected()) {
    return nullptr;
  }

  nsCOMPtr<nsIVariant> variant = DataNoSecurityCheck();

  MOZ_ASSERT(!ChromeOnly(), "Non-chrome code shouldn't see a ChromeOnly DataTransferItem");
  if (ChromeOnly()) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  bool checkItemPrincipal = mDataTransfer->IsCrossDomainSubFrameDrop() ||
    (mDataTransfer->GetEventMessage() != eDrop &&
     mDataTransfer->GetEventMessage() != ePaste);

  // Check if the caller is allowed to access the drag data. Callers with
  // chrome privileges can always read the data. During the
  // drop event, allow retrieving the data except in the case where the
  // source of the drag is in a child frame of the caller. In that case,
  // we only allow access to data of the same principal. During other events,
  // only allow access to the data with the same principal.
  //
  // We don't want to fail with an exception in this siutation, rather we want
  // to just pretend as though the stored data is "nullptr". This is consistent
  // with Chrome's behavior and is less surprising for web applications which
  // don't expect execptions to be raised when performing certain operations.
  if (Principal() && checkItemPrincipal &&
      !aPrincipal->Subsumes(Principal())) {
    return nullptr;
  }

  if (!variant) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> data;
  nsresult rv = variant->GetAsISupports(getter_AddRefs(data));
  if (NS_SUCCEEDED(rv) && data) {
    nsCOMPtr<EventTarget> pt = do_QueryInterface(data);
    if (pt) {
      nsIGlobalObject* go = pt->GetOwnerGlobal();
      if (NS_WARN_IF(!go)) {
        return nullptr;
      }

      nsCOMPtr<nsIScriptObjectPrincipal> sp = do_QueryInterface(go);
      MOZ_ASSERT(sp, "This cannot fail on the main thread.");

      nsIPrincipal* dataPrincipal = sp->GetPrincipal();
      if (NS_WARN_IF(!dataPrincipal || !aPrincipal->Equals(dataPrincipal))) {
        return nullptr;
      }
    }
  }

  return variant.forget();
}

} // namespace dom
} // namespace mozilla
