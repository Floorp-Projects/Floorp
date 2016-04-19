/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderDoctorDiagnostics.h"

#include "mozilla/dom/DecoderDoctorNotificationBinding.h"
#include "mozilla/Logging.h"
#include "nsGkAtoms.h"
#include "nsIDocument.h"
#include "nsIObserverService.h"
#include "nsITimer.h"
#include "nsIWeakReference.h"

static mozilla::LazyLogModule sDecoderDoctorLog("DecoderDoctor");
#define DD_LOG(level, arg, ...) MOZ_LOG(sDecoderDoctorLog, level, (arg, ##__VA_ARGS__))
#define DD_DEBUG(arg, ...) DD_LOG(mozilla::LogLevel::Debug, arg, ##__VA_ARGS__)
#define DD_INFO(arg, ...) DD_LOG(mozilla::LogLevel::Info, arg, ##__VA_ARGS__)
#define DD_WARN(arg, ...) DD_LOG(mozilla::LogLevel::Warning, arg, ##__VA_ARGS__)

namespace mozilla
{

// Class that collects a sequence of diagnostics from the same document over a
// small period of time, in order to provide a synthesized analysis.
//
// Referenced by the document through a nsINode property, mTimer, and
// inter-task captures.
// When notified that the document is dead, or when the timer expires but
// nothing new happened, StopWatching() will remove the document property and
// timer (if present), so no more work will happen and the watcher will be
// destroyed once all references are gone.
class DecoderDoctorDocumentWatcher : public nsITimerCallback
{
public:
  static RefPtr<DecoderDoctorDocumentWatcher>
  RetrieveOrCreate(nsIDocument* aDocument);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK

  void AddDiagnostics(const nsAString& aFormat,
                      const char* aCallSite,
                      DecoderDoctorDiagnostics&& aDiagnostics);

private:
  explicit DecoderDoctorDocumentWatcher(nsIDocument* aDocument);
  virtual ~DecoderDoctorDocumentWatcher();

  // This will prevent further work from happening, watcher will deregister
  // itself from document (if requested) and cancel any timer, and soon die.
  void StopWatching(bool aRemoveProperty);

  // Remove property from document; will call DestroyPropertyCallback.
  void RemovePropertyFromDocument();
  // Callback for property destructor, will be automatically called when the
  // document (in aObject) is being destroyed.
  static void DestroyPropertyCallback(void* aObject,
                                      nsIAtom* aPropertyName,
                                      void* aPropertyValue,
                                      void* aData);

  static const uint32_t sAnalysisPeriod_ms = 1000;
  void EnsureTimerIsStarted();

  void ReportAnalysis(dom::DecoderDoctorNotificationType aNotificationType,
                      const char* aReportStringId,
                      const nsAString& aFormats);

  void SynthesizeAnalysis();

  // Raw pointer to an nsIDocument.
  // Must be non-null during construction.
  // Nulled when we want to stop watching, because either:
  // 1. The document has been destroyed (notified through
  //    DestroyPropertyCallback).
  // 2. We have not received new diagnostic information within a short time
  //    period, so we just stop watching.
  // Once nulled, no more actual work will happen, and the watcher will be
  // destroyed soon.
  nsIDocument* mDocument;

  struct Diagnostics
  {
    Diagnostics(DecoderDoctorDiagnostics&& aDiagnostics,
                const nsAString& aFormat,
                const char* aCallSite)
      : mDecoderDoctorDiagnostics(Move(aDiagnostics))
      , mFormat(aFormat)
      , mCallSite(aCallSite)
    {}
    Diagnostics(const Diagnostics&) = delete;
    Diagnostics(Diagnostics&& aOther)
      : mDecoderDoctorDiagnostics(Move(aOther.mDecoderDoctorDiagnostics))
      , mFormat(Move(aOther.mFormat))
      , mCallSite(Move(aOther.mCallSite))
    {}

    const DecoderDoctorDiagnostics mDecoderDoctorDiagnostics;
    const nsString mFormat;
    const nsCString mCallSite;
  };
  typedef nsTArray<Diagnostics> DiagnosticsSequence;
  DiagnosticsSequence mDiagnosticsSequence;

  nsCOMPtr<nsITimer> mTimer; // Keep timer alive until we run.
  DiagnosticsSequence::size_type mDiagnosticsHandled = 0;
};


NS_IMPL_ISUPPORTS(DecoderDoctorDocumentWatcher, nsITimerCallback)

// static
RefPtr<DecoderDoctorDocumentWatcher>
DecoderDoctorDocumentWatcher::RetrieveOrCreate(nsIDocument* aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDocument);
  RefPtr<DecoderDoctorDocumentWatcher> watcher =
    static_cast<DecoderDoctorDocumentWatcher*>(
      aDocument->GetProperty(nsGkAtoms::decoderDoctor));
  if (!watcher) {
    watcher = new DecoderDoctorDocumentWatcher(aDocument);
    if (NS_WARN_IF(NS_FAILED(
          aDocument->SetProperty(nsGkAtoms::decoderDoctor,
                                 watcher.get(),
                                 DestroyPropertyCallback,
                                 /*transfer*/ false)))) {
      DD_WARN("DecoderDoctorDocumentWatcher::RetrieveOrCreate(doc=%p) - Could not set property in document, will destroy new watcher[%p]",
              aDocument, watcher.get());
      return nullptr;
    }
    // Document owns watcher through this property.
    // Released in DestroyPropertyCallback().
    NS_ADDREF(watcher.get());
  }
  return watcher;
}

DecoderDoctorDocumentWatcher::DecoderDoctorDocumentWatcher(nsIDocument* aDocument)
  : mDocument(aDocument)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocument);
  DD_DEBUG("DecoderDoctorDocumentWatcher[%p]::DecoderDoctorDocumentWatcher(doc=%p)",
           this, mDocument);
}

DecoderDoctorDocumentWatcher::~DecoderDoctorDocumentWatcher()
{
  MOZ_ASSERT(NS_IsMainThread());
  DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p <- expect 0]::~DecoderDoctorDocumentWatcher()",
           this, mDocument);
  // mDocument should have been reset through StopWatching()!
  MOZ_ASSERT(!mDocument);
}

void
DecoderDoctorDocumentWatcher::RemovePropertyFromDocument()
{
  MOZ_ASSERT(NS_IsMainThread());
  DecoderDoctorDocumentWatcher* watcher =
    static_cast<DecoderDoctorDocumentWatcher*>(
      mDocument->GetProperty(nsGkAtoms::decoderDoctor));
  if (!watcher) {
    return;
  }
  DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::RemovePropertyFromDocument()\n",
           watcher, watcher->mDocument);
  // This will remove the property and call our DestroyPropertyCallback.
  mDocument->DeleteProperty(nsGkAtoms::decoderDoctor);
}

// Callback for property destructors. |aObject| is the object
// the property is being removed for, |aPropertyName| is the property
// being removed, |aPropertyValue| is the value of the property, and |aData|
// is the opaque destructor data that was passed to SetProperty().
// static
void
DecoderDoctorDocumentWatcher::DestroyPropertyCallback(void* aObject,
                                                      nsIAtom* aPropertyName,
                                                      void* aPropertyValue,
                                                      void*)
{
  MOZ_ASSERT(NS_IsMainThread());
#ifdef DEBUG
  nsIDocument* document = static_cast<nsIDocument*>(aObject);
#endif
  MOZ_ASSERT(aPropertyName == nsGkAtoms::decoderDoctor);
  DecoderDoctorDocumentWatcher* watcher =
    static_cast<DecoderDoctorDocumentWatcher*>(aPropertyValue);
  MOZ_ASSERT(watcher);
  MOZ_ASSERT(watcher->mDocument == document);
  DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::DestroyPropertyCallback()\n",
           watcher, watcher->mDocument);
  // 'false': StopWatching should not try and remove the property.
  watcher->StopWatching(false);
  NS_RELEASE(watcher);
}

void
DecoderDoctorDocumentWatcher::StopWatching(bool aRemoveProperty)
{
  MOZ_ASSERT(NS_IsMainThread());
  // StopWatching() shouldn't be called twice.
  MOZ_ASSERT(mDocument);

  if (aRemoveProperty) {
    RemovePropertyFromDocument();
  }

  // Forget document now, this will prevent more work from being started.
  mDocument = nullptr;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

void
DecoderDoctorDocumentWatcher::EnsureTimerIsStarted()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTimer) {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (NS_WARN_IF(!mTimer)) {
      return;
    }
    if (NS_WARN_IF(NS_FAILED(
          mTimer->InitWithCallback(
            this, sAnalysisPeriod_ms, nsITimer::TYPE_ONE_SHOT)))) {
      mTimer = nullptr;
    }
  }
}

static void
DispatchNotification(nsISupports* aSubject,
                     dom::DecoderDoctorNotificationType aNotificationType,
                     const nsAString& aFormats)
{
  if (!aSubject) {
    return;
  }
  dom::DecoderDoctorNotification data;
  data.mType = aNotificationType;
  if (!aFormats.IsEmpty()) {
    data.mFormats.Construct(aFormats);
  }
  nsAutoString json;
  data.ToJSON(json);
  if (json.IsEmpty()) {
    DD_WARN("DecoderDoctorDiagnostics/DispatchEvent() - Could not create json for dispatch");
    // No point in dispatching this notification without data, the front-end
    // wouldn't know what to display.
    return;
  }
  DD_DEBUG("DecoderDoctorDiagnostics/DispatchEvent() %s", NS_ConvertUTF16toUTF8(json).get());
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(aSubject, "decoder-doctor-notification", json.get());
  }
}

void
DecoderDoctorDocumentWatcher::ReportAnalysis(
  dom::DecoderDoctorNotificationType aNotificationType,
  const char* aReportStringId,
  const nsAString& aFormats)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDocument) {
    return;
  }

  const char16_t* params[] = { aFormats.Data() };
  DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::ReportAnalysis() ReportToConsole - aMsg='%s' params[0]='%s'",
           this, mDocument, aReportStringId,
           NS_ConvertUTF16toUTF8(params[0]).get());
  nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                  NS_LITERAL_CSTRING("Media"),
                                  mDocument,
                                  nsContentUtils::eDOM_PROPERTIES,
                                  aReportStringId,
                                  params,
                                  ArrayLength(params));

  // For now, disable all front-end notifications by default.
  // TODO: Future bugs will use finer-grained filtering instead.
  if (Preferences::GetBool("media.decoderdoctor.enable-notification-bar", false)) {
    DispatchNotification(
      mDocument->GetInnerWindow(), aNotificationType, aFormats);
  }
}

void
DecoderDoctorDocumentWatcher::SynthesizeAnalysis()
{
  MOZ_ASSERT(NS_IsMainThread());

  bool canPlay = false;
#if defined(MOZ_FFMPEG)
  bool PlatformDecoderNeeded = false;
#endif
  nsAutoString formats;
  for (auto& diag : mDiagnosticsSequence) {
    if (diag.mDecoderDoctorDiagnostics.CanPlay()) {
      canPlay = true;
    } else {
#if defined(MOZ_FFMPEG)
      if (diag.mDecoderDoctorDiagnostics.DidFFmpegFailToLoad()) {
        PlatformDecoderNeeded = true;
      }
#endif
      if (!formats.IsEmpty()) {
        formats += NS_LITERAL_STRING(", ");
      }
      formats += diag.mFormat;
    }
  }
  if (!canPlay) {
#if defined(MOZ_FFMPEG)
    if (PlatformDecoderNeeded) {
      DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::Notify() - formats: %s -> Cannot play media because platform decoder was not found",
               this, mDocument, NS_ConvertUTF16toUTF8(formats).get());
      ReportAnalysis(dom::DecoderDoctorNotificationType::Platform_decoder_not_found,
                     "MediaPlatformDecoderNotFound", formats);
    } else
#endif
    {
      DD_WARN("DecoderDoctorDocumentWatcher[%p, doc=%p]::Notify() - Cannot play media, formats: %s",
              this, mDocument, NS_ConvertUTF16toUTF8(formats).get());
      ReportAnalysis(dom::DecoderDoctorNotificationType::Cannot_play,
                     "MediaCannotPlayNoDecoders", formats);
    }
  } else if (!formats.IsEmpty()) {
    DD_INFO("DecoderDoctorDocumentWatcher[%p, doc=%p]::Notify() - Can play media, but no decoders for some requested formats: %s",
            this, mDocument, NS_ConvertUTF16toUTF8(formats).get());
    if (Preferences::GetBool("media.decoderdoctor.verbose", false)) {
      ReportAnalysis(
        dom::DecoderDoctorNotificationType::Can_play_but_some_missing_decoders,
        "MediaNoDecoders", formats);
    }
  } else {
    DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::Notify() - Can play media, decoders available for all requested formats",
             this, mDocument);
  }
}

void
DecoderDoctorDocumentWatcher::AddDiagnostics(const nsAString& aFormat,
                                            const char* aCallSite,
                                            DecoderDoctorDiagnostics&& aDiagnostics)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mDocument) {
    return;
  }

  DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::AddDiagnostics(format='%s', call site '%s', can play=%d, platform lib failed to load=%d)",
           this, mDocument, NS_ConvertUTF16toUTF8(aFormat).get(),
           aCallSite, aDiagnostics.CanPlay(), aDiagnostics.DidFFmpegFailToLoad());
  mDiagnosticsSequence.AppendElement(
    Diagnostics(Move(aDiagnostics), aFormat, aCallSite));
  EnsureTimerIsStarted();
}

NS_IMETHODIMP
DecoderDoctorDocumentWatcher::Notify(nsITimer* timer)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(timer == mTimer);

  // Forget timer. (Assuming timer keeps itself and us alive during this call.)
  mTimer = nullptr;

  if (!mDocument) {
    return NS_OK;
  }

  if (mDiagnosticsSequence.Length() > mDiagnosticsHandled) {
    // We have new diagnostic data.
    mDiagnosticsHandled = mDiagnosticsSequence.Length();

    SynthesizeAnalysis();

    // Restart timer, to redo analysis or stop watching this document,
    // depending on whether anything new happens.
    EnsureTimerIsStarted();
  } else {
    DD_DEBUG("DecoderDoctorDocumentWatcher[%p, doc=%p]::Notify() - No new diagnostics to analyze -> Stop watching",
             this, mDocument);
    // Stop watching this document, we don't expect more diagnostics for now.
    // If more diagnostics come in, we'll treat them as another burst, separately.
    // 'true' to remove the property from the document.
    StopWatching(true);
  }

  return NS_OK;
}


void
DecoderDoctorDiagnostics::StoreDiagnostics(nsIDocument* aDocument,
                                           const nsAString& aFormat,
                                           const char* aCallSite)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!aDocument)) {
    DD_WARN("DecoderDoctorDiagnostics[%p]::StoreDiagnostics(nsIDocument* aDocument=nullptr, format='%s', call site '%s')",
            this, NS_ConvertUTF16toUTF8(aFormat).get(), aCallSite);
    return;
  }

  RefPtr<DecoderDoctorDocumentWatcher> watcher =
    DecoderDoctorDocumentWatcher::RetrieveOrCreate(aDocument);

  if (NS_WARN_IF(!watcher)) {
    DD_WARN("DecoderDoctorDiagnostics[%p]::StoreDiagnostics(nsIDocument* aDocument=nullptr, format='%s', call site '%s') - Could not create document watcher",
            this, NS_ConvertUTF16toUTF8(aFormat).get(), aCallSite);
    return;
  }

  // StoreDiagnostics should only be called once, after all data is available,
  // so it is safe to Move() from this object.
  watcher->AddDiagnostics(aFormat, aCallSite, Move(*this));
}

} // namespace mozilla
