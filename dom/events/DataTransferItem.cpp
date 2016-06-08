/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataTransferItem.h"
#include "DataTransferItemList.h"

#include "mozilla/ContentEvents.h"
#include "mozilla/EventForwards.h"
#include "mozilla/dom/DataTransferItemBinding.h"
#include "nsIClipboard.h"
#include "nsISupportsPrimitives.h"
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
  { kJPEGImageMime, "GenericImageNameJPEG" },
  { kGIFImageMime, "GenericImageNameGIF" },
  { kPNGImageMime, "GenericImageNamePNG" },
};

} // anonymous namespace

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DataTransferItem, mData,
                                      mPrincipal, mParent)
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
DataTransferItem::Clone(DataTransferItemList* aParent) const
{
  MOZ_ASSERT(aParent);

  RefPtr<DataTransferItem> it = new DataTransferItem(aParent, mType);

  // Copy over all of the fields
  it->mKind = mKind;
  it->mIndex = mIndex;
  it->mData = mData;
  it->mPrincipal = mPrincipal;

  return it.forget();
}

void
DataTransferItem::SetType(const nsAString& aType)
{
  mType = aType;
}

void
DataTransferItem::SetData(nsIVariant* aData)
{
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

  mKind = KIND_OTHER;
  mData = aData;

  nsCOMPtr<nsISupports> supports;
  nsresult rv = aData->GetAsISupports(getter_AddRefs(supports));
  if (NS_SUCCEEDED(rv) && supports) {
    RefPtr<File> file = FileFromISupports(supports);
    if (file) {
      mKind = KIND_FILE;
      return;
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
    mKind = KIND_STRING;
  }
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

  if (mParent->GetEventMessage() == ePaste) {
    MOZ_ASSERT(mIndex == 0, "index in clipboard must be 0");

    nsCOMPtr<nsIClipboard> clipboard =
      do_GetService("@mozilla.org/widget/clipboard;1");
    if (!clipboard || mParent->ClipboardType() < 0) {
      return;
    }

    nsresult rv = clipboard->GetData(trans, mParent->ClipboardType());
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

  if (Kind() == KIND_FILE) {
    // Because this is an external piece of data, mType is one of kFileMime,
    // kPNGImageMime, kJPEGImageMime, or kGIFImageMime. We want to convert
    // whatever type happens to actually be stored into a dom::File.

    RefPtr<File> file = FileFromISupports(data);
    MOZ_ASSERT(file, "Invalid format for Kind() == KIND_FILE");

    data = do_QueryObject(file);
  }

  RefPtr<nsVariantCC> variant = new nsVariantCC();

  nsCOMPtr<nsISupportsString> supportsstr = do_QueryInterface(data);
  if (supportsstr) {
    MOZ_ASSERT(Kind() == KIND_STRING);
    nsAutoString str;
    supportsstr->GetData(str);
    variant->SetAsAString(str);
  } else {
    nsCOMPtr<nsISupportsCString> supportscstr = do_QueryInterface(data);
    if (supportscstr) {
      MOZ_ASSERT(Kind() == KIND_STRING);
      nsAutoCString str;
      supportscstr->GetData(str);
      variant->SetAsACString(str);
    } else {
      MOZ_ASSERT(Kind() == KIND_FILE);
      variant->SetAsISupports(data);
    }
  }

  SetData(variant);
}

already_AddRefed<File>
DataTransferItem::GetAsFile(ErrorResult& aRv)
{
  if (mKind != KIND_FILE) {
    return nullptr;
  }

  nsIVariant* data = Data();
  if (NS_WARN_IF(!data)) {
    return nullptr;
  }

  nsCOMPtr<nsISupports> supports;
  aRv = data->GetAsISupports(getter_AddRefs(supports));
  MOZ_ASSERT(!aRv.Failed() && supports,
             "Files should be stored with type dom::File!");
  if (aRv.Failed() || !supports) {
    return nullptr;
  }

  RefPtr<File> file = FileFromISupports(supports);
  MOZ_ASSERT(file, "Files should be stored with type dom::File!");
  if (!file) {
    return nullptr;
  }

  // The File object should have been stored as a File in the nsIVariant. If it
  // was stored as a Blob, with a file BlobImpl, we could still get to this
  // point, except that file is a new File object, with a different identity
  // then the original blob ibject. This should never happen so we assert
  // against it.
  MOZ_ASSERT(SameCOMIdentity(supports, NS_ISUPPORTS_CAST(nsIDOMBlob*, file)),
             "Got back a new File object from FileFromISupports in GetAsFile!");

  return file.forget();
}

already_AddRefed<File>
DataTransferItem::FileFromISupports(nsISupports* aSupports)
{
  MOZ_ASSERT(aSupports);

  RefPtr<File> file;

  nsCOMPtr<nsIDOMBlob> domBlob = do_QueryInterface(aSupports);
  if (domBlob) {
    // Get out the blob - this is OK, because nsIDOMBlob is a builtinclass
    // and the only implementer is Blob.
    Blob* blob = static_cast<Blob*>(domBlob.get());
    file = blob->ToFile();
  } else if (nsCOMPtr<nsIFile> ifile = do_QueryInterface(aSupports)) {
    printf("Creating a File from a nsIFile!\n");
    file = File::CreateFromFile(GetParentObject(), ifile);
  } else if (nsCOMPtr<nsIInputStream> stream = do_QueryInterface(aSupports)) {
    // This consumes the stream object
    ErrorResult rv;
    file = CreateFileFromInputStream(GetParentObject(), stream, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }
  } else if (nsCOMPtr<BlobImpl> blobImpl = do_QueryInterface(aSupports)) {
    MOZ_ASSERT(blobImpl->IsFile());
    file = File::Create(GetParentObject(), blobImpl);
  }

  return file.forget();
}

already_AddRefed<File>
DataTransferItem::CreateFileFromInputStream(nsISupports* aParent,
                                            nsIInputStream* aStream,
                                            ErrorResult& aRv)
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

  nsXPIDLString fileName;
  aRv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           key, fileName);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  uint64_t available;
  aRv = aStream->Available(&available);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  void* data = nullptr;
  aRv = NS_ReadInputStreamToBuffer(aStream, &data, available);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return File::CreateMemoryFile(aParent, data, available, fileName,
                                mType, PR_Now());
}

void
DataTransferItem::GetAsString(FunctionStringCallback* aCallback,
                              ErrorResult& aRv)
{
  if (!aCallback || mKind != KIND_STRING) {
    return;
  }

  nsIVariant* data = Data();
  if (NS_WARN_IF(!data)) {
    return;
  }

  nsAutoString stringData;
  data->GetAsAString(stringData);

  // Dispatch the callback to the main thread
  class GASRunnable final : public Runnable
  {
  public:
    GASRunnable(FunctionStringCallback* aCallback,
                const nsAString& aStringData)
      : mCallback(aCallback), mStringData(aStringData)
    {}

    NS_IMETHOD Run() override
    {
      ErrorResult rv;
      mCallback->Call(mStringData, rv);
      NS_WARN_IF(rv.Failed());
      return rv.StealNSResult();
    }
  private:
    RefPtr<FunctionStringCallback> mCallback;
    nsString mStringData;
  };

  RefPtr<GASRunnable> runnable = new GASRunnable(aCallback, stringData);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("NS_DispatchToMainThread Failed in "
               "DataTransferItem::GetAsString!");
  }
}

} // namespace dom
} // namespace mozilla
