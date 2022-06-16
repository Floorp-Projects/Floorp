/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Clipboard.h"

#include <algorithm>

#include "mozilla/AbstractThread.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/ClipboardItem.h"
#include "mozilla/dom/ClipboardBinding.h"
#include "mozilla/dom/ContentChild.h"
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
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "nsVariant.h"

static mozilla::LazyLogModule gClipboardLog("Clipboard");

namespace mozilla::dom {

Clipboard::Clipboard(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow) {}

Clipboard::~Clipboard() = default;

// static
bool Clipboard::IsTestingPrefEnabledOrHasReadPermission(
    nsIPrincipal& aSubjectPrincipal) {
  return IsTestingPrefEnabled() ||
         nsContentUtils::PrincipalHasPermission(aSubjectPrincipal,
                                                nsGkAtoms::clipboardRead);
}

// @return true iff the event was dispatched successfully.
static bool MaybeCreateAndDispatchMozClipboardReadTextPasteEvent(
    nsPIDOMWindowInner& aOwner) {
  RefPtr<Document> document = aOwner.GetDoc();

  if (!document) {
    // Presumably, this shouldn't happen but to be safe, this case is handled.
    MOZ_LOG(Clipboard::GetClipboardLog(), LogLevel::Debug,
            ("%s: no document.", __FUNCTION__));
    return false;
  }

  // Conceptionally, `ClipboardReadTextPasteChild` is the target of the event.
  // It ensures to receive the event by declaring the event in
  // <BrowserGlue.jsm>.
  return !NS_WARN_IF(NS_FAILED(nsContentUtils::DispatchChromeEvent(
      document, ToSupports(document), u"MozClipboardReadTextPaste"_ns,
      CanBubble::eNo, Cancelable::eNo)));
}

already_AddRefed<nsIRunnable> Clipboard::ReadTextRequest::Answer() {
  return NS_NewRunnableFunction(
      "Clipboard::ReadText",
      [p = std::move(mPromise),
       dataTransfer = MakeRefPtr<DataTransfer>(
           nullptr, ePaste, !XRE_IsContentProcess() /* aIsExternal */,
           nsIClipboard::kGlobalClipboard),
       principal = std::move(mSubjectPrincipal)]() {
        if (XRE_IsContentProcess()) {
          ContentChild* contentChild = ContentChild::GetSingleton();

          AutoTArray<nsCString, 1> types{nsLiteralCString{kUnicodeMime}};

          contentChild
              ->SendGetClipboardAsync(types, nsIClipboard::kGlobalClipboard)
              ->Then(
                  GetMainThreadSerialEventTarget(), __func__,
                  /* success */
                  [p, dataTransfer,
                   principal](const IPCDataTransfer& ipcDataTransfer) {
                    DataTransfer::IPCDataTransferTextItemsToDataTransfer(
                        ipcDataTransfer, false /* aHidden */, *dataTransfer);
                    Clipboard::ProcessDataTransfer(*dataTransfer, *p, eReadText,
                                                   nullptr /* aOwner */,
                                                   *principal,
                                                   /* aNeedToFill */ false);
                  },
                  /* failure */
                  [p](mozilla::ipc::ResponseRejectReason aReason) {
                    p->MaybeRejectWithUndefined();
                  });
        } else {
          Clipboard::ProcessDataTransfer(*dataTransfer, *p, eReadText,
                                         nullptr /* aOwner */, *principal,
                                         /* aNeedToFill */ true);
        }
      });
}

already_AddRefed<nsIRunnable>
Clipboard::HandleReadTextRequestWhichRequiresPasteButton(
    Promise& aPromise, nsIPrincipal& aSubjectPrincipal) {
  RefPtr<nsIRunnable> runnable;

  nsPIDOMWindowInner* owner = GetOwner();
  WindowContext* windowContext = owner ? owner->GetWindowContext() : nullptr;
  if (!windowContext) {
    MOZ_ASSERT_UNREACHABLE("There should be a WindowContext.");
    aPromise.MaybeRejectWithUndefined();
    return runnable.forget();
  }

  // If no transient user activation, reject the promise and return.
  if (!windowContext->HasValidTransientUserGestureActivation()) {
    aPromise.MaybeRejectWithNotAllowedError(
        "`navigator.clipboard.readText()` was blocked due to lack of "
        "user activation.");
    return runnable.forget();
  }

  // TODO: when a user activation stems from a contextmenu event
  // (https://developer.mozilla.org/en-US/docs/Web/API/Element/contextmenu_event),
  // forbid pasting (bug 1767941).

  switch (mTransientUserPasteState.RefreshAndGet(*windowContext)) {
    case TransientUserPasteState::Value::Initial: {
      MOZ_ASSERT(mReadTextRequests.IsEmpty());

      if (MaybeCreateAndDispatchMozClipboardReadTextPasteEvent(*owner)) {
        mTransientUserPasteState.OnStartWaitingForUserReactionToPasteMenuPopup(
            windowContext->GetUserGestureStart());
        mReadTextRequests.AppendElement(
            MakeUnique<ReadTextRequest>(aPromise, aSubjectPrincipal));
      } else {
        // This shouldn't happen but let's handle this case.
        aPromise.MaybeRejectWithUndefined();
      }
      break;
    }
    case TransientUserPasteState::Value::
        WaitingForUserReactionToPasteMenuPopup: {
      MOZ_ASSERT(!mReadTextRequests.IsEmpty());

      mReadTextRequests.AppendElement(
          MakeUnique<ReadTextRequest>(aPromise, aSubjectPrincipal));
      break;
    }
    case TransientUserPasteState::Value::TransientlyForbiddenByUser: {
      aPromise.MaybeRejectWithNotAllowedError(
          "`navigator.clipboard.readText()` was blocked due to the user "
          "dismissing the 'Paste' button.");
      break;
    }
    case TransientUserPasteState::Value::TransientlyAllowedByUser: {
      runnable = ReadTextRequest{aPromise, aSubjectPrincipal}.Answer();
      break;
    }
  }

  return runnable.forget();
}

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
  const bool isTestingPrefEnabledOrHasReadPermission =
      IsTestingPrefEnabledOrHasReadPermission(aSubjectPrincipal);

  switch (aClipboardReadType) {
    case eReadText: {
      RefPtr<nsIRunnable> runnable =
          isTestingPrefEnabledOrHasReadPermission
              ? ReadTextRequest{*p, aSubjectPrincipal}.Answer()
              : HandleReadTextRequestWhichRequiresPasteButton(
                    *p, aSubjectPrincipal);

      if (runnable) {
        GetParentObject()->Dispatch(TaskCategory::Other, runnable.forget());
      }

      break;
    }
    case eRead: {
      if (!isTestingPrefEnabledOrHasReadPermission) {
        MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
                ("Clipboard, ReadHelper, "
                 "Don't have permissions for reading\n"));
        p->MaybeRejectWithUndefined();
        return p.forget();
      }

      // Want isExternal = true in order to use the data transfer object to
      // perform a read
      RefPtr<DataTransfer> dataTransfer = new DataTransfer(
          this, ePaste, true /* aIsExternal */, nsIClipboard::kGlobalClipboard);

      RefPtr<nsPIDOMWindowInner> owner = GetOwner();

      RefPtr<nsIRunnable> r = NS_NewRunnableFunction(
          "Clipboard::Read", [p, dataTransfer, owner, aClipboardReadType,
                              principal = RefPtr{&aSubjectPrincipal}]() {
            // TODO: read async also for `eRead`, see bug 1755863.
            ProcessDataTransfer(*dataTransfer, *p, aClipboardReadType, owner,
                                *principal,
                                /* aNeedToFill */ true);
          });

      GetParentObject()->Dispatch(TaskCategory::Other, r.forget());
      break;
    }
  }

  return p.forget();
}

/* static */
void Clipboard::ProcessDataTransfer(DataTransfer& aDataTransfer,
                                    Promise& aPromise,
                                    ClipboardReadType aClipboardReadType,
                                    nsPIDOMWindowInner* aOwner,
                                    nsIPrincipal& aSubjectPrincipal,
                                    bool aNeedToFill) {
  IgnoredErrorResult ier;
  switch (aClipboardReadType) {
    case eRead: {
      MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
              ("Clipboard, ReadHelper, read case\n"));
      if (aNeedToFill) {
        aDataTransfer.FillAllExternalData();
      }
      // Convert the DataTransferItems to ClipboardItems.
      // FIXME(bug 1691825): This is only suitable for testing!
      // A real implementation would only read from the clipboard
      // in ClipboardItem::getType instead of doing it here.
      nsTArray<ClipboardItem::ItemEntry> entries;
      DataTransferItemList* items = aDataTransfer.Items();
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
          nsCOMPtr<nsIVariant> data = item->Data(&aSubjectPrincipal, ignored);
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
          RefPtr<File> file = item->GetAsFile(aSubjectPrincipal, ignored);
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
          aOwner, PresentationStyle::Unspecified, std::move(entries)));
      aPromise.MaybeResolve(sequence);
      break;
    }
    case eReadText: {
      MOZ_LOG(GetClipboardLog(), LogLevel::Debug,
              ("Clipboard, ReadHelper, read text case\n"));
      nsAutoString str;
      // If we haven't filled the data transfer with the items from the
      // clipboard yet, then DataTransfer::GetData will get data from the
      // clipboard and retrieve the item in the desired format if it exists.
      // Otherwise, if our data transfer is filled with items from the clipboard
      // and we call DataTransfer::GetData, it will simply try to retrieve the
      // item in the desired format among the items it already has.
      aDataTransfer.GetData(NS_LITERAL_STRING_FROM_CSTRING(kTextMime), str,
                            aSubjectPrincipal, ier);
      // Either resolve with a string extracted from data transfer item
      // or resolve with an empty string if nothing was found
      aPromise.MaybeResolve(str);
      break;
    }
  }
}

auto Clipboard::TransientUserPasteState::RefreshAndGet(
    WindowContext& aWindowContext) -> Value {
  MOZ_ASSERT(aWindowContext.HasValidTransientUserGestureActivation());

  switch (mValue) {
    case Value::Initial: {
      MOZ_ASSERT(mUserGestureStart.IsNull());
      break;
    }
    case Value::WaitingForUserReactionToPasteMenuPopup: {
      MOZ_ASSERT(!mUserGestureStart.IsNull());
      MOZ_ASSERT(
          mUserGestureStart == aWindowContext.GetUserGestureStart(),
          "A new transient user gesture activation should be impossible while "
          "there's no response to the 'Paste' button.");
      // `OnUserReactedToPasteMenuPopup` will handle the reaction.
      break;
    }
    case Value::TransientlyForbiddenByUser: {
      [[fallthrough]];
    }
    case Value::TransientlyAllowedByUser: {
      MOZ_ASSERT(!mUserGestureStart.IsNull());

      if (mUserGestureStart != aWindowContext.GetUserGestureStart()) {
        *this = {};
      }
      break;
    }
  }

  return mValue;
}

void Clipboard::TransientUserPasteState::
    OnStartWaitingForUserReactionToPasteMenuPopup(
        const TimeStamp& aUserGestureStart) {
  MOZ_ASSERT(mValue == Value::Initial);
  MOZ_ASSERT(!aUserGestureStart.IsNull());

  mValue = Value::WaitingForUserReactionToPasteMenuPopup;
  mUserGestureStart = aUserGestureStart;
}

void Clipboard::TransientUserPasteState::OnUserReactedToPasteMenuPopup(
    const bool aAllowed) {
  mValue = aAllowed ? Value::TransientlyAllowedByUser
                    : Value::TransientlyForbiddenByUser;
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

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
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

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue,
                        ErrorResult& aRv) override {
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

void Clipboard::ReadTextRequest::MaybeRejectWithNotAllowedError(
    const nsACString& aMessage) {
  mPromise->MaybeRejectWithNotAllowedError(aMessage);
}

void Clipboard::OnUserReactedToPasteMenuPopup(const bool aAllowed) {
  MOZ_LOG(GetClipboardLog(), LogLevel::Debug, ("%s", __FUNCTION__));

  mTransientUserPasteState.OnUserReactedToPasteMenuPopup(aAllowed);

  MOZ_ASSERT(!mReadTextRequests.IsEmpty());

  for (UniquePtr<ReadTextRequest>& request : mReadTextRequests) {
    if (aAllowed) {
      GetParentObject()->Dispatch(TaskCategory::Other, request->Answer());
    } else {
      request->MaybeRejectWithNotAllowedError(
          "The user dismissed the 'Paste' button."_ns);
    }
  }

  mReadTextRequests.Clear();
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
