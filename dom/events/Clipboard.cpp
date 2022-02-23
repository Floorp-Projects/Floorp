/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AbstractThread.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Clipboard.h"
#include "mozilla/dom/ClipboardItem.h"
#include "mozilla/dom/ClipboardBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/DataTransfer.h"
#include "mozilla/dom/DataTransferItemList.h"
#include "mozilla/dom/DataTransferItem.h"
#include "mozilla/dom/Document.h"
#include "mozilla/StaticPrefs_dom.h"
#include "imgIContainer.h"
#include "imgITools.h"
#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsIClipboard.h"
#include "nsIInputStream.h"
#include "nsIParserUtils.h"
#include "nsITransferable.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsStringStream.h"
#include "nsVariant.h"

static mozilla::LazyLogModule gClipboardLog("Clipboard");

namespace mozilla::dom {

Clipboard::Clipboard(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow) {}

Clipboard::~Clipboard() = default;

already_AddRefed<Promise> Clipboard::ReadHelper(
    nsIPrincipal& aSubjectPrincipal, ClipboardReadType aClipboardReadType,
    ErrorResult& aRv) {
  // Create a new promise
  RefPtr<Promise> p = dom::Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // We want to disable security check for automated tests that have the pref
  //  dom.events.testing.asyncClipboard set to true
  if (!IsTestingPrefEnabled() &&
      !nsContentUtils::PrincipalHasPermission(aSubjectPrincipal,
                                              nsGkAtoms::clipboardRead)) {
    MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
            ("Clipboard, ReadHelper, "
             "Don't have permissions for reading\n"));
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  // Want isExternal = true in order to use the data transfer object to perform
  // a read
  RefPtr<DataTransfer> dataTransfer = new DataTransfer(
      this, ePaste, /* is external */ true, nsIClipboard::kGlobalClipboard);

  RefPtr<nsPIDOMWindowInner> owner = GetOwner();

  // Create a new runnable
  RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "Clipboard::Read", [p, dataTransfer, aClipboardReadType, owner,
                          principal = RefPtr{&aSubjectPrincipal}]() {
        IgnoredErrorResult ier;
        switch (aClipboardReadType) {
          case eRead: {
            MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
                    ("Clipboard, ReadHelper, read case\n"));
            dataTransfer->FillAllExternalData();

            // Convert the DataTransferItems to ClipboardItems.
            // FIXME(bug 1691825): This is only suitable for testing!
            // A real implementation would only read from the clipboard
            // in ClipboardItem::getType instead of doing it here.
            nsTArray<ClipboardItem::ItemEntry> entries;
            DataTransferItemList* items = dataTransfer->Items();
            for (size_t i = 0; i < items->Length(); i++) {
              bool found = false;
              DataTransferItem* item = items->IndexedGetter(i, found);

              // Only allow strings and files.
              if (!found || item->Kind() == DataTransferItem::KIND_OTHER) {
                continue;
              }

              nsAutoString type;
              item->GetType(type);

              if (item->Kind() == DataTransferItem::KIND_STRING) {
                // We just ignore items that we can't access.
                IgnoredErrorResult ignored;
                nsCOMPtr<nsIVariant> data = item->Data(principal, ignored);
                if (NS_WARN_IF(!data || ignored.Failed())) {
                  continue;
                }

                nsAutoString string;
                if (NS_WARN_IF(NS_FAILED(data->GetAsAString(string)))) {
                  continue;
                }

                ClipboardItem::ItemEntry* entry = entries.AppendElement();
                entry->mType = type;
                entry->mData.SetAsString() = string;
              } else {
                IgnoredErrorResult ignored;
                RefPtr<File> file = item->GetAsFile(*principal, ignored);
                if (NS_WARN_IF(!file || ignored.Failed())) {
                  continue;
                }

                ClipboardItem::ItemEntry* entry = entries.AppendElement();
                entry->mType = type;
                entry->mData.SetAsBlob() = file;
              }
            }

            nsTArray<RefPtr<ClipboardItem>> sequence;
            sequence.AppendElement(MakeRefPtr<ClipboardItem>(
                owner, PresentationStyle::Unspecified, std::move(entries)));
            p->MaybeResolve(sequence);
            break;
          }
          case eReadText:
            MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
                    ("Clipboard, ReadHelper, read text case\n"));
            nsAutoString str;
            dataTransfer->GetData(NS_LITERAL_STRING_FROM_CSTRING(kTextMime),
                                  str, *principal, ier);
            // Either resolve with a string extracted from data transfer item
            // or resolve with an empty string if nothing was found
            p->MaybeResolve(str);
            break;
        }
      });
  // Dispatch the runnable
  GetParentObject()->Dispatch(TaskCategory::Other, r.forget());
  return p.forget();
}

already_AddRefed<Promise> Clipboard::Read(nsIPrincipal& aSubjectPrincipal,
                                          ErrorResult& aRv) {
  return ReadHelper(aSubjectPrincipal, eRead, aRv);
}

already_AddRefed<Promise> Clipboard::ReadText(nsIPrincipal& aSubjectPrincipal,
                                              ErrorResult& aRv) {
  return ReadHelper(aSubjectPrincipal, eReadText, aRv);
}

namespace {

struct NativeEntry {
  nsString mType;
  nsCOMPtr<nsIVariant> mData;

  NativeEntry(const nsAString& aType, nsIVariant* aData)
      : mType(aType), mData(aData) {}
};
using NativeEntryPromise = MozPromise<NativeEntry, CopyableErrorResult, false>;

class BlobTextHandler final : public PromiseNativeHandler {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit BlobTextHandler(const nsAString& aType) : mType(aType) {}

  RefPtr<NativeEntryPromise> Promise() { return mHolder.Ensure(__func__); }

  void Reject() {
    CopyableErrorResult rv;
    rv.ThrowUnknownError("Unable to read blob for '"_ns +
                         NS_ConvertUTF16toUTF8(mType) + "' as text."_ns);
    mHolder.Reject(rv, __func__);
  }

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    AssertIsOnMainThread();

    nsString text;
    if (!ConvertJSValueToUSVString(aCx, aValue, "ClipboardItem text", text)) {
      Reject();
      return;
    }

    RefPtr<nsVariantCC> variant = new nsVariantCC();
    variant->SetAsAString(text);

    NativeEntry native(mType, variant);
    mHolder.Resolve(std::move(native), __func__);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    Reject();
  }

 private:
  ~BlobTextHandler() = default;

  nsString mType;
  MozPromiseHolder<NativeEntryPromise> mHolder;
};

NS_IMPL_ISUPPORTS0(BlobTextHandler)

RefPtr<NativeEntryPromise> GetStringNativeEntry(
    const ClipboardItem::ItemEntry& entry) {
  if (entry.mData.IsString()) {
    RefPtr<nsVariantCC> variant = new nsVariantCC();
    variant->SetAsAString(entry.mData.GetAsString());
    NativeEntry native(entry.mType, variant);
    return NativeEntryPromise::CreateAndResolve(native, __func__);
  }

  RefPtr<BlobTextHandler> handler = new BlobTextHandler(entry.mType);
  IgnoredErrorResult ignored;
  RefPtr<Promise> promise = entry.mData.GetAsBlob()->Text(ignored);
  if (ignored.Failed()) {
    CopyableErrorResult rv;
    rv.ThrowUnknownError("Unable to read blob for '"_ns +
                         NS_ConvertUTF16toUTF8(entry.mType) + "' as text."_ns);
    return NativeEntryPromise::CreateAndReject(rv, __func__);
  }
  promise->AppendNativeHandler(handler);
  return handler->Promise();
}

class ImageDecodeCallback final : public imgIContainerCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit ImageDecodeCallback(const nsAString& aType) : mType(aType) {}

  RefPtr<NativeEntryPromise> Promise() { return mHolder.Ensure(__func__); }

  NS_IMETHOD OnImageReady(imgIContainer* aImage, nsresult aStatus) override {
    // Request the image's width to force decoding the image header.
    int32_t ignored;
    if (NS_FAILED(aStatus) || NS_FAILED(aImage->GetWidth(&ignored))) {
      CopyableErrorResult rv;
      rv.ThrowDataError("Unable to decode blob for '"_ns +
                        NS_ConvertUTF16toUTF8(mType) + "' as image."_ns);
      mHolder.Reject(rv, __func__);
      return NS_OK;
    }

    RefPtr<nsVariantCC> variant = new nsVariantCC();
    variant->SetAsISupports(aImage);

    // Note: We always put the image as "native" on the clipboard.
    NativeEntry native(NS_LITERAL_STRING_FROM_CSTRING(kNativeImageMime),
                       variant);
    mHolder.Resolve(std::move(native), __func__);
    return NS_OK;
  };

 private:
  ~ImageDecodeCallback() = default;

  nsString mType;
  MozPromiseHolder<NativeEntryPromise> mHolder;
};

NS_IMPL_ISUPPORTS(ImageDecodeCallback, imgIContainerCallback)

RefPtr<NativeEntryPromise> GetImageNativeEntry(
    const ClipboardItem::ItemEntry& entry) {
  if (entry.mData.IsString()) {
    CopyableErrorResult rv;
    rv.ThrowTypeError("DOMString not supported for '"_ns +
                      NS_ConvertUTF16toUTF8(entry.mType) +
                      "' as image data."_ns);
    return NativeEntryPromise::CreateAndReject(rv, __func__);
  }

  IgnoredErrorResult ignored;
  nsCOMPtr<nsIInputStream> stream;
  entry.mData.GetAsBlob()->CreateInputStream(getter_AddRefs(stream), ignored);
  if (ignored.Failed()) {
    CopyableErrorResult rv;
    rv.ThrowUnknownError("Unable to read blob for '"_ns +
                         NS_ConvertUTF16toUTF8(entry.mType) + "' as image."_ns);
    return NativeEntryPromise::CreateAndReject(rv, __func__);
  }

  RefPtr<ImageDecodeCallback> callback = new ImageDecodeCallback(entry.mType);
  nsCOMPtr<imgITools> imgtool = do_CreateInstance("@mozilla.org/image/tools;1");
  imgtool->DecodeImageAsync(stream, NS_ConvertUTF16toUTF8(entry.mType),
                            callback, GetMainThreadSerialEventTarget());
  return callback->Promise();
}

Result<NativeEntry, ErrorResult> SanitizeNativeEntry(
    const NativeEntry& aEntry) {
  MOZ_ASSERT(aEntry.mType.EqualsLiteral(kHTMLMime));

  nsAutoString string;
  aEntry.mData->GetAsAString(string);

  nsCOMPtr<nsIParserUtils> parserUtils =
      do_GetService(NS_PARSERUTILS_CONTRACTID);
  if (!parserUtils) {
    ErrorResult rv;
    rv.ThrowUnknownError("Error while processing '"_ns +
                         NS_ConvertUTF16toUTF8(aEntry.mType) + "'."_ns);
    return Err(std::move(rv));
  }

  uint32_t flags = nsIParserUtils::SanitizerAllowStyle |
                   nsIParserUtils::SanitizerAllowComments;
  nsAutoString sanitized;
  if (NS_FAILED(parserUtils->Sanitize(string, flags, sanitized))) {
    ErrorResult rv;
    rv.ThrowUnknownError("Error while processing '"_ns +
                         NS_ConvertUTF16toUTF8(aEntry.mType) + "'."_ns);
    return Err(std::move(rv));
  }

  RefPtr<nsVariantCC> variant = new nsVariantCC();
  variant->SetAsAString(sanitized);
  return NativeEntry(aEntry.mType, variant);
}

// Restrict to types allowed by Chrome
// SVG is still disabled by default in Chrome.
static bool IsValidType(const nsAString& aType) {
  return aType.EqualsLiteral(kPNGImageMime) || aType.EqualsLiteral(kTextMime) ||
         aType.EqualsLiteral(kHTMLMime);
}

using NativeItemPromise = NativeEntryPromise::AllPromiseType;

RefPtr<NativeItemPromise> GetClipboardNativeItem(const ClipboardItem& aItem) {
  nsTArray<RefPtr<NativeEntryPromise>> promises;
  for (const auto& entry : aItem.Entries()) {
    if (!IsValidType(entry.mType)) {
      CopyableErrorResult rv;
      rv.ThrowNotAllowedError("Type '"_ns + NS_ConvertUTF16toUTF8(entry.mType) +
                              "' not supported for write"_ns);
      return NativeItemPromise::CreateAndReject(rv, __func__);
    }

    if (entry.mType.EqualsLiteral(kPNGImageMime)) {
      promises.AppendElement(GetImageNativeEntry(entry));
    } else {
      RefPtr<NativeEntryPromise> promise = GetStringNativeEntry(entry);
      if (entry.mType.EqualsLiteral(kHTMLMime)) {
        promise = promise->Then(
            GetMainThreadSerialEventTarget(), __func__,
            [](const NativeEntryPromise::ResolveOrRejectValue& aValue)
                -> RefPtr<NativeEntryPromise> {
              if (aValue.IsReject()) {
                return NativeEntryPromise::CreateAndReject(aValue.RejectValue(),
                                                           __func__);
              }

              auto sanitized = SanitizeNativeEntry(aValue.ResolveValue());
              if (sanitized.isErr()) {
                return NativeEntryPromise::CreateAndReject(
                    CopyableErrorResult(sanitized.unwrapErr()), __func__);
              }
              return NativeEntryPromise::CreateAndResolve(sanitized.unwrap(),
                                                          __func__);
            });
      }
      promises.AppendElement(promise);
    }
  }
  return NativeEntryPromise::All(GetCurrentSerialEventTarget(), promises);
}

}  // namespace

already_AddRefed<Promise> Clipboard::Write(
    const Sequence<OwningNonNull<ClipboardItem>>& aData,
    nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
  // Create a promise
  RefPtr<Promise> p = dom::Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<nsPIDOMWindowInner> owner = GetOwner();
  Document* doc = owner ? owner->GetDoc() : nullptr;
  if (!doc) {
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  // We want to disable security check for automated tests that have the pref
  //  dom.events.testing.asyncClipboard set to true
  if (!IsTestingPrefEnabled() &&
      !nsContentUtils::IsCutCopyAllowed(doc, aSubjectPrincipal)) {
    MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
            ("Clipboard, Write, Not allowed to write to clipboard\n"));
    p->MaybeRejectWithNotAllowedError(
        "Clipboard write was blocked due to lack of user activation.");
    return p.forget();
  }

  // Get the clipboard service
  nsCOMPtr<nsIClipboard> clipboard(
      do_GetService("@mozilla.org/widget/clipboard;1"));
  if (!clipboard) {
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  nsCOMPtr<nsILoadContext> context = doc->GetLoadContext();
  if (!context) {
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  if (aData.Length() > 1) {
    p->MaybeRejectWithNotAllowedError(
        "Clipboard write is only supported with one ClipboardItem at the "
        "moment");
    return p.forget();
  }

  if (aData.Length() == 0) {
    // Nothing needs to be written to the clipboard.
    p->MaybeResolveWithUndefined();
    return p.forget();
  }

  GetClipboardNativeItem(aData[0])->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [owner, p, clipboard, context, principal = RefPtr{&aSubjectPrincipal}](
          const nsTArray<NativeEntry>& aEntries) {
        RefPtr<DataTransfer> dataTransfer =
            new DataTransfer(owner, eCopy,
                             /* is external */ true,
                             /* clipboard type */ -1);

        for (const auto& entry : aEntries) {
          nsresult rv = dataTransfer->SetDataWithPrincipal(
              entry.mType, entry.mData, 0, principal);

          if (NS_FAILED(rv)) {
            p->MaybeRejectWithUndefined();
            return;
          }
        }

        // Get the transferable
        RefPtr<nsITransferable> transferable =
            dataTransfer->GetTransferable(0, context);
        if (!transferable) {
          p->MaybeRejectWithUndefined();
          return;
        }

        // Finally write data to clipboard
        nsresult rv =
            clipboard->SetData(transferable,
                               /* owner of the transferable */ nullptr,
                               nsIClipboard::kGlobalClipboard);
        if (NS_FAILED(rv)) {
          p->MaybeRejectWithUndefined();
          return;
        }

        p->MaybeResolveWithUndefined();
      },
      [p](const CopyableErrorResult& aErrorResult) {
        p->MaybeReject(CopyableErrorResult(aErrorResult));
      });

  return p.forget();
}

already_AddRefed<Promise> Clipboard::WriteText(const nsAString& aData,
                                               nsIPrincipal& aSubjectPrincipal,
                                               ErrorResult& aRv) {
  // Create a single-element Sequence to reuse Clipboard::Write.
  nsTArray<ClipboardItem::ItemEntry> items;
  ClipboardItem::ItemEntry* entry = items.AppendElement();
  entry->mType = NS_LITERAL_STRING_FROM_CSTRING(kTextMime);
  entry->mData.SetAsString() = aData;

  nsTArray<OwningNonNull<ClipboardItem>> sequence;
  RefPtr<ClipboardItem> item = new ClipboardItem(
      GetOwner(), PresentationStyle::Unspecified, std::move(items));
  sequence.AppendElement(*item);

  return Write(std::move(sequence), aSubjectPrincipal, aRv);
}

JSObject* Clipboard::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return Clipboard_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
LogModule* Clipboard::GetClipboardLog() { return gClipboardLog; }

/* static */
bool Clipboard::ReadTextEnabled(JSContext* aCx, JSObject* aGlobal) {
  nsIPrincipal* prin = nsContentUtils::SubjectPrincipal(aCx);
  return IsTestingPrefEnabled() || prin->GetIsAddonOrExpandedAddonPrincipal() ||
         prin->IsSystemPrincipal();
}

/* static */
bool Clipboard::IsTestingPrefEnabled() {
  bool clipboardTestingEnabled =
      StaticPrefs::dom_events_testing_asyncClipboard_DoNotUseDirectly();
  MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
          ("Clipboard, Is testing enabled? %d\n", clipboardTestingEnabled));
  return clipboardTestingEnabled;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(Clipboard)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(Clipboard,
                                                  DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(Clipboard, DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Clipboard)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(Clipboard, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(Clipboard, DOMEventTargetHelper)

}  // namespace mozilla::dom
