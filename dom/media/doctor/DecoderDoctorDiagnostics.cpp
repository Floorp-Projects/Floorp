/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderDoctorDiagnostics.h"

#include "mozilla/dom/DecoderDoctorNotificationBinding.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "mozilla/TimeStamp.h"
#include "nsContentUtils.h"
#include "nsGkAtoms.h"
#include "mozilla/dom/Document.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsITimer.h"
#include "nsPluginHost.h"
#include "nsPrintfCString.h"
#include "VideoUtils.h"

#if defined(MOZ_FFMPEG)
#  include "FFmpegRuntimeLinker.h"
#endif

static mozilla::LazyLogModule sDecoderDoctorLog("DecoderDoctor");
#define DD_LOG(level, arg, ...) \
  MOZ_LOG(sDecoderDoctorLog, level, (arg, ##__VA_ARGS__))
#define DD_DEBUG(arg, ...) DD_LOG(mozilla::LogLevel::Debug, arg, ##__VA_ARGS__)
#define DD_INFO(arg, ...) DD_LOG(mozilla::LogLevel::Info, arg, ##__VA_ARGS__)
#define DD_WARN(arg, ...) DD_LOG(mozilla::LogLevel::Warning, arg, ##__VA_ARGS__)

namespace mozilla {

// Class that collects a sequence of diagnostics from the same document over a
// small period of time, in order to provide a synthesized analysis.
//
// Referenced by the document through a nsINode property, mTimer, and
// inter-task captures.
// When notified that the document is dead, or when the timer expires but
// nothing new happened, StopWatching() will remove the document property and
// timer (if present), so no more work will happen and the watcher will be
// destroyed once all references are gone.
class DecoderDoctorDocumentWatcher : public nsITimerCallback, public nsINamed {
 public:
  static already_AddRefed<DecoderDoctorDocumentWatcher> RetrieveOrCreate(
      dom::Document* aDocument);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  void AddDiagnostics(DecoderDoctorDiagnostics&& aDiagnostics,
                      const char* aCallSite);

 private:
  explicit DecoderDoctorDocumentWatcher(dom::Document* aDocument);
  virtual ~DecoderDoctorDocumentWatcher();

  // This will prevent further work from happening, watcher will deregister
  // itself from document (if requested) and cancel any timer, and soon die.
  void StopWatching(bool aRemoveProperty);

  // Remove property from document; will call DestroyPropertyCallback.
  void RemovePropertyFromDocument();
  // Callback for property destructor, will be automatically called when the
  // document (in aObject) is being destroyed.
  static void DestroyPropertyCallback(void* aObject, nsAtom* aPropertyName,
                                      void* aPropertyValue, void* aData);

  static const uint32_t sAnalysisPeriod_ms = 1000;
  void EnsureTimerIsStarted();

  void SynthesizeAnalysis();

  // Raw pointer to a Document.
  // Must be non-null during construction.
  // Nulled when we want to stop watching, because either:
  // 1. The document has been destroyed (notified through
  //    DestroyPropertyCallback).
  // 2. We have not received new diagnostic information within a short time
  //    period, so we just stop watching.
  // Once nulled, no more actual work will happen, and the watcher will be
  // destroyed soon.
  dom::Document* mDocument;

  struct Diagnostics {
    Diagnostics(DecoderDoctorDiagnostics&& aDiagnostics, const char* aCallSite,
                mozilla::TimeStamp aTimeStamp)
        : mDecoderDoctorDiagnostics(std::move(aDiagnostics)),
          mCallSite(aCallSite),
          mTimeStamp(aTimeStamp) {}
    Diagnostics(const Diagnostics&) = delete;
    Diagnostics(Diagnostics&& aOther)
        : mDecoderDoctorDiagnostics(
              std::move(aOther.mDecoderDoctorDiagnostics)),
          mCallSite(std::move(aOther.mCallSite)),
          mTimeStamp(aOther.mTimeStamp) {}

    const DecoderDoctorDiagnostics mDecoderDoctorDiagnostics;
    const nsCString mCallSite;
    const mozilla::TimeStamp mTimeStamp;
  };
  typedef nsTArray<Diagnostics> DiagnosticsSequence;
  DiagnosticsSequence mDiagnosticsSequence;

  nsCOMPtr<nsITimer> mTimer;  // Keep timer alive until we run.
  DiagnosticsSequence::size_type mDiagnosticsHandled = 0;
};

NS_IMPL_ISUPPORTS(DecoderDoctorDocumentWatcher, nsITimerCallback, nsINamed)

// static
already_AddRefed<DecoderDoctorDocumentWatcher>
DecoderDoctorDocumentWatcher::RetrieveOrCreate(dom::Document* aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDocument);
  RefPtr<DecoderDoctorDocumentWatcher> watcher =
      static_cast<DecoderDoctorDocumentWatcher*>(
          aDocument->GetProperty(nsGkAtoms::decoderDoctor));
  if (!watcher) {
    watcher = new DecoderDoctorDocumentWatcher(aDocument);
    if (NS_WARN_IF(NS_FAILED(aDocument->SetProperty(
            nsGkAtoms::decoderDoctor, watcher.get(), DestroyPropertyCallback,
            /*transfer*/ false)))) {
      DD_WARN(
          "DecoderDoctorDocumentWatcher::RetrieveOrCreate(doc=%p) - Could not "
          "set property in document, will destroy new watcher[%p]",
          aDocument, watcher.get());
      return nullptr;
    }
    // Document owns watcher through this property.
    // Released in DestroyPropertyCallback().
    NS_ADDREF(watcher.get());
  }
  return watcher.forget();
}

DecoderDoctorDocumentWatcher::DecoderDoctorDocumentWatcher(
    dom::Document* aDocument)
    : mDocument(aDocument) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDocument);
  DD_DEBUG(
      "DecoderDoctorDocumentWatcher[%p]::DecoderDoctorDocumentWatcher(doc=%p)",
      this, mDocument);
}

DecoderDoctorDocumentWatcher::~DecoderDoctorDocumentWatcher() {
  MOZ_ASSERT(NS_IsMainThread());
  DD_DEBUG(
      "DecoderDoctorDocumentWatcher[%p, doc=%p <- expect "
      "0]::~DecoderDoctorDocumentWatcher()",
      this, mDocument);
  // mDocument should have been reset through StopWatching()!
  MOZ_ASSERT(!mDocument);
}

void DecoderDoctorDocumentWatcher::RemovePropertyFromDocument() {
  MOZ_ASSERT(NS_IsMainThread());
  DecoderDoctorDocumentWatcher* watcher =
      static_cast<DecoderDoctorDocumentWatcher*>(
          mDocument->GetProperty(nsGkAtoms::decoderDoctor));
  if (!watcher) {
    return;
  }
  DD_DEBUG(
      "DecoderDoctorDocumentWatcher[%p, "
      "doc=%p]::RemovePropertyFromDocument()\n",
      watcher, watcher->mDocument);
  // This will remove the property and call our DestroyPropertyCallback.
  mDocument->DeleteProperty(nsGkAtoms::decoderDoctor);
}

// Callback for property destructors. |aObject| is the object
// the property is being removed for, |aPropertyName| is the property
// being removed, |aPropertyValue| is the value of the property, and |aData|
// is the opaque destructor data that was passed to SetProperty().
// static
void DecoderDoctorDocumentWatcher::DestroyPropertyCallback(
    void* aObject, nsAtom* aPropertyName, void* aPropertyValue, void*) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPropertyName == nsGkAtoms::decoderDoctor);
  DecoderDoctorDocumentWatcher* watcher =
      static_cast<DecoderDoctorDocumentWatcher*>(aPropertyValue);
  MOZ_ASSERT(watcher);
#ifdef DEBUG
  auto* document = static_cast<dom::Document*>(aObject);
  MOZ_ASSERT(watcher->mDocument == document);
#endif
  DD_DEBUG(
      "DecoderDoctorDocumentWatcher[%p, doc=%p]::DestroyPropertyCallback()\n",
      watcher, watcher->mDocument);
  // 'false': StopWatching should not try and remove the property.
  watcher->StopWatching(false);
  NS_RELEASE(watcher);
}

void DecoderDoctorDocumentWatcher::StopWatching(bool aRemoveProperty) {
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

void DecoderDoctorDocumentWatcher::EnsureTimerIsStarted() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mTimer) {
    NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, sAnalysisPeriod_ms,
                            nsITimer::TYPE_ONE_SHOT);
  }
}

enum class ReportParam : uint8_t {
  // Marks the end of the parameter list.
  // Keep this zero! (For implicit zero-inits when used in definitions below.)
  None = 0,

  Formats,
  DecodeIssue,
  DocURL,
  ResourceURL
};

struct NotificationAndReportStringId {
  // Notification type, handled by browser-media.js.
  dom::DecoderDoctorNotificationType mNotificationType;
  // Console message id. Key in dom/locales/.../chrome/dom/dom.properties.
  const char* mReportStringId;
  static const int maxReportParams = 4;
  ReportParam mReportParams[maxReportParams];
};

// Note: ReportStringIds are limited to alphanumeric only.
static const NotificationAndReportStringId sMediaWidevineNoWMF = {
    dom::DecoderDoctorNotificationType::Platform_decoder_not_found,
    "MediaWidevineNoWMF",
    {ReportParam::None}};
static const NotificationAndReportStringId sMediaWMFNeeded = {
    dom::DecoderDoctorNotificationType::Platform_decoder_not_found,
    "MediaWMFNeeded",
    {ReportParam::Formats}};
static const NotificationAndReportStringId sMediaPlatformDecoderNotFound = {
    dom::DecoderDoctorNotificationType::Platform_decoder_not_found,
    "MediaPlatformDecoderNotFound",
    {ReportParam::Formats}};
static const NotificationAndReportStringId sMediaCannotPlayNoDecoders = {
    dom::DecoderDoctorNotificationType::Cannot_play,
    "MediaCannotPlayNoDecoders",
    {ReportParam::Formats}};
static const NotificationAndReportStringId sMediaNoDecoders = {
    dom::DecoderDoctorNotificationType::Can_play_but_some_missing_decoders,
    "MediaNoDecoders",
    {ReportParam::Formats}};
static const NotificationAndReportStringId sCannotInitializePulseAudio = {
    dom::DecoderDoctorNotificationType::Cannot_initialize_pulseaudio,
    "MediaCannotInitializePulseAudio",
    {ReportParam::None}};
static const NotificationAndReportStringId sUnsupportedLibavcodec = {
    dom::DecoderDoctorNotificationType::Unsupported_libavcodec,
    "MediaUnsupportedLibavcodec",
    {ReportParam::None}};
static const NotificationAndReportStringId sMediaDecodeError = {
    dom::DecoderDoctorNotificationType::Decode_error,
    "MediaDecodeError",
    {ReportParam::ResourceURL, ReportParam::DecodeIssue}};
static const NotificationAndReportStringId sMediaDecodeWarning = {
    dom::DecoderDoctorNotificationType::Decode_warning,
    "MediaDecodeWarning",
    {ReportParam::ResourceURL, ReportParam::DecodeIssue}};

static const NotificationAndReportStringId* const
    sAllNotificationsAndReportStringIds[] = {&sMediaWidevineNoWMF,
                                             &sMediaWMFNeeded,
                                             &sMediaPlatformDecoderNotFound,
                                             &sMediaCannotPlayNoDecoders,
                                             &sMediaNoDecoders,
                                             &sCannotInitializePulseAudio,
                                             &sUnsupportedLibavcodec,
                                             &sMediaDecodeError,
                                             &sMediaDecodeWarning};

// Create a webcompat-friendly description of a MediaResult.
static nsString MediaResultDescription(const MediaResult& aResult,
                                       bool aIsError) {
  nsCString name;
  GetErrorName(aResult.Code(), name);
  return NS_ConvertUTF8toUTF16(nsPrintfCString(
      "%s Code: %s (0x%08" PRIx32 ")%s%s", aIsError ? "Error" : "Warning",
      name.get(), static_cast<uint32_t>(aResult.Code()),
      aResult.Message().IsEmpty() ? "" : "\nDetails: ",
      aResult.Message().get()));
}

static void DispatchNotification(
    nsISupports* aSubject, const NotificationAndReportStringId& aNotification,
    bool aIsSolved, const nsAString& aFormats, const nsAString& aDecodeIssue,
    const nsACString& aDocURL, const nsAString& aResourceURL) {
  if (!aSubject) {
    return;
  }
  dom::DecoderDoctorNotification data;
  data.mType = aNotification.mNotificationType;
  data.mIsSolved = aIsSolved;
  data.mDecoderDoctorReportId.Assign(
      NS_ConvertUTF8toUTF16(aNotification.mReportStringId));
  if (!aFormats.IsEmpty()) {
    data.mFormats.Construct(aFormats);
  }
  if (!aDecodeIssue.IsEmpty()) {
    data.mDecodeIssue.Construct(aDecodeIssue);
  }
  if (!aDocURL.IsEmpty()) {
    data.mDocURL.Construct(NS_ConvertUTF8toUTF16(aDocURL));
  }
  if (!aResourceURL.IsEmpty()) {
    data.mResourceURL.Construct(aResourceURL);
  }
  nsAutoString json;
  data.ToJSON(json);
  if (json.IsEmpty()) {
    DD_WARN(
        "DecoderDoctorDiagnostics/DispatchEvent() - Could not create json for "
        "dispatch");
    // No point in dispatching this notification without data, the front-end
    // wouldn't know what to display.
    return;
  }
  DD_DEBUG("DecoderDoctorDiagnostics/DispatchEvent() %s",
           NS_ConvertUTF16toUTF8(json).get());
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(aSubject, "decoder-doctor-notification", json.get());
  }
}

static void ReportToConsole(dom::Document* aDocument,
                            const char* aConsoleStringId,
                            const nsTArray<nsString>& aParams) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDocument);

  DD_DEBUG(
      "DecoderDoctorDiagnostics.cpp:ReportToConsole(doc=%p) ReportToConsole"
      " - aMsg='%s' params={%s%s%s%s}",
      aDocument, aConsoleStringId,
      aParams.IsEmpty() ? "<no params>"
                        : NS_ConvertUTF16toUTF8(aParams[0]).get(),
      (aParams.Length() < 1 || aParams[0].IsEmpty()) ? "" : ", ",
      (aParams.Length() < 1 || aParams[0].IsEmpty())
          ? ""
          : NS_ConvertUTF16toUTF8(aParams[0]).get(),
      aParams.Length() < 2 ? "" : ", ...");
  nsContentUtils::ReportToConsole(
      nsIScriptError::warningFlag, NS_LITERAL_CSTRING("Media"), aDocument,
      nsContentUtils::eDOM_PROPERTIES, aConsoleStringId, aParams);
}

static bool AllowNotification(
    const NotificationAndReportStringId& aNotification) {
  // "media.decoder-doctor.notifications-allowed" controls which notifications
  // may be dispatched to the front-end. It either contains:
  // - '*' -> Allow everything.
  // - Comma-separater list of ids -> Allow if aReportStringId (from
  //                                  dom.properties) is one of them.
  // - Nothing (missing or empty) -> Disable everything.
  nsAutoCString filter;
  Preferences::GetCString("media.decoder-doctor.notifications-allowed", filter);
  return filter.EqualsLiteral("*") ||
         StringListContains(filter, aNotification.mReportStringId);
}

static bool AllowDecodeIssue(const MediaResult& aDecodeIssue,
                             bool aDecodeIssueIsError) {
  if (aDecodeIssue == NS_OK) {
    // 'NS_OK' means we are not actually reporting a decode issue, so we
    // allow the report.
    return true;
  }

  // "media.decoder-doctor.decode-{errors,warnings}-allowed" controls which
  // decode issues may be dispatched to the front-end. It either contains:
  // - '*' -> Allow everything.
  // - Comma-separater list of ids -> Allow if the issue name is one of them.
  // - Nothing (missing or empty) -> Disable everything.
  nsAutoCString filter;
  Preferences::GetCString(aDecodeIssueIsError
                              ? "media.decoder-doctor.decode-errors-allowed"
                              : "media.decoder-doctor.decode-warnings-allowed",
                          filter);
  if (filter.EqualsLiteral("*")) {
    return true;
  }

  nsCString decodeIssueName;
  GetErrorName(aDecodeIssue.Code(), static_cast<nsACString&>(decodeIssueName));
  return StringListContains(filter, decodeIssueName);
}

static void ReportAnalysis(
    dom::Document* aDocument,
    const NotificationAndReportStringId& aNotification, bool aIsSolved,
    const nsAString& aFormats = NS_LITERAL_STRING(""),
    const MediaResult& aDecodeIssue = NS_OK, bool aDecodeIssueIsError = true,
    const nsACString& aDocURL = NS_LITERAL_CSTRING(""),
    const nsAString& aResourceURL = NS_LITERAL_STRING("")) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aDocument) {
    return;
  }

  nsString decodeIssueDescription;
  if (aDecodeIssue != NS_OK) {
    decodeIssueDescription.Assign(
        MediaResultDescription(aDecodeIssue, aDecodeIssueIsError));
  }

  // Report non-solved issues to console.
  if (!aIsSolved) {
    // Build parameter array needed by console message.
    AutoTArray<nsString, NotificationAndReportStringId::maxReportParams> params;
    for (int i = 0; i < NotificationAndReportStringId::maxReportParams; ++i) {
      if (aNotification.mReportParams[i] == ReportParam::None) {
        break;
      }
      switch (aNotification.mReportParams[i]) {
        case ReportParam::Formats:
          params.AppendElement(aFormats);
          break;
        case ReportParam::DecodeIssue:
          params.AppendElement(decodeIssueDescription);
          break;
        case ReportParam::DocURL:
          params.AppendElement(NS_ConvertUTF8toUTF16(aDocURL));
          break;
        case ReportParam::ResourceURL:
          params.AppendElement(aResourceURL);
          break;
        default:
          MOZ_ASSERT_UNREACHABLE("Bad notification parameter choice");
          break;
      }
    }
    ReportToConsole(aDocument, aNotification.mReportStringId, params);
  }

  if (AllowNotification(aNotification) &&
      AllowDecodeIssue(aDecodeIssue, aDecodeIssueIsError)) {
    DispatchNotification(aDocument->GetInnerWindow(), aNotification, aIsSolved,
                         aFormats, decodeIssueDescription, aDocURL,
                         aResourceURL);
  }
}

static nsString CleanItemForFormatsList(const nsAString& aItem) {
  nsString item(aItem);
  // Remove commas from item, as commas are used to separate items. It's fine
  // to have a one-way mapping, it's only used for comparisons and in
  // console display (where formats shouldn't contain commas in the first place)
  item.ReplaceChar(',', ' ');
  item.CompressWhitespace();
  return item;
}

static void AppendToFormatsList(nsAString& aList, const nsAString& aItem) {
  if (!aList.IsEmpty()) {
    aList += NS_LITERAL_STRING(", ");
  }
  aList += CleanItemForFormatsList(aItem);
}

static bool FormatsListContains(const nsAString& aList,
                                const nsAString& aItem) {
  return StringListContains(aList, CleanItemForFormatsList(aItem));
}

void DecoderDoctorDocumentWatcher::SynthesizeAnalysis() {
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoString playableFormats;
  nsAutoString unplayableFormats;
  // Subsets of unplayableFormats that require a specific platform decoder:
#if defined(XP_WIN)
  nsAutoString formatsRequiringWMF;
#endif
#if defined(MOZ_FFMPEG)
  nsAutoString formatsRequiringFFMpeg;
#endif
  nsAutoString supportedKeySystems;
  nsAutoString unsupportedKeySystems;
  DecoderDoctorDiagnostics::KeySystemIssue lastKeySystemIssue =
      DecoderDoctorDiagnostics::eUnset;
  // Only deal with one decode error per document (the first one found).
  const MediaResult* firstDecodeError = nullptr;
  const nsString* firstDecodeErrorMediaSrc = nullptr;
  // Only deal with one decode warning per document (the first one found).
  const MediaResult* firstDecodeWarning = nullptr;
  const nsString* firstDecodeWarningMediaSrc = nullptr;

  for (const auto& diag : mDiagnosticsSequence) {
    switch (diag.mDecoderDoctorDiagnostics.Type()) {
      case DecoderDoctorDiagnostics::eFormatSupportCheck:
        if (diag.mDecoderDoctorDiagnostics.CanPlay()) {
          AppendToFormatsList(playableFormats,
                              diag.mDecoderDoctorDiagnostics.Format());
        } else {
          AppendToFormatsList(unplayableFormats,
                              diag.mDecoderDoctorDiagnostics.Format());
#if defined(XP_WIN)
          if (diag.mDecoderDoctorDiagnostics.DidWMFFailToLoad()) {
            AppendToFormatsList(formatsRequiringWMF,
                                diag.mDecoderDoctorDiagnostics.Format());
          }
#endif
#if defined(MOZ_FFMPEG)
          if (diag.mDecoderDoctorDiagnostics.DidFFmpegFailToLoad()) {
            AppendToFormatsList(formatsRequiringFFMpeg,
                                diag.mDecoderDoctorDiagnostics.Format());
          }
#endif
        }
        break;
      case DecoderDoctorDiagnostics::eMediaKeySystemAccessRequest:
        if (diag.mDecoderDoctorDiagnostics.IsKeySystemSupported()) {
          AppendToFormatsList(supportedKeySystems,
                              diag.mDecoderDoctorDiagnostics.KeySystem());
        } else {
          AppendToFormatsList(unsupportedKeySystems,
                              diag.mDecoderDoctorDiagnostics.KeySystem());
          DecoderDoctorDiagnostics::KeySystemIssue issue =
              diag.mDecoderDoctorDiagnostics.GetKeySystemIssue();
          if (issue != DecoderDoctorDiagnostics::eUnset) {
            lastKeySystemIssue = issue;
          }
        }
        break;
      case DecoderDoctorDiagnostics::eEvent:
        MOZ_ASSERT_UNREACHABLE("Events shouldn't be stored for processing.");
        break;
      case DecoderDoctorDiagnostics::eDecodeError:
        if (!firstDecodeError) {
          firstDecodeError = &diag.mDecoderDoctorDiagnostics.DecodeIssue();
          firstDecodeErrorMediaSrc =
              &diag.mDecoderDoctorDiagnostics.DecodeIssueMediaSrc();
        }
        break;
      case DecoderDoctorDiagnostics::eDecodeWarning:
        if (!firstDecodeWarning) {
          firstDecodeWarning = &diag.mDecoderDoctorDiagnostics.DecodeIssue();
          firstDecodeWarningMediaSrc =
              &diag.mDecoderDoctorDiagnostics.DecodeIssueMediaSrc();
        }
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unhandled DecoderDoctorDiagnostics type");
        break;
    }
  }

  // Check if issues have been solved, by finding if some now-playable
  // key systems or formats were previously recorded as having issues.
  if (!supportedKeySystems.IsEmpty() || !playableFormats.IsEmpty()) {
    DD_DEBUG(
        "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
        "supported key systems '%s', playable formats '%s'; See if they show "
        "issues have been solved...",
        this, mDocument, NS_ConvertUTF16toUTF8(supportedKeySystems).Data(),
        NS_ConvertUTF16toUTF8(playableFormats).get());
    const nsAString* workingFormatsArray[] = {&supportedKeySystems,
                                              &playableFormats};
    // For each type of notification, retrieve the pref that contains formats/
    // key systems with issues.
    for (const NotificationAndReportStringId* id :
         sAllNotificationsAndReportStringIds) {
      nsAutoCString formatsPref("media.decoder-doctor.");
      formatsPref += id->mReportStringId;
      formatsPref += ".formats";
      nsAutoString formatsWithIssues;
      Preferences::GetString(formatsPref.Data(), formatsWithIssues);
      if (formatsWithIssues.IsEmpty()) {
        continue;
      }
      // See if that list of formats-with-issues contains any formats that are
      // now playable/supported.
      bool solved = false;
      for (const nsAString* workingFormats : workingFormatsArray) {
        for (const auto& workingFormat : MakeStringListRange(*workingFormats)) {
          if (FormatsListContains(formatsWithIssues, workingFormat)) {
            // This now-working format used not to work -> Report solved issue.
            DD_INFO(
                "DecoderDoctorDocumentWatcher[%p, "
                "doc=%p]::SynthesizeAnalysis() - %s solved ('%s' now works, it "
                "was in pref(%s)='%s')",
                this, mDocument, id->mReportStringId,
                NS_ConvertUTF16toUTF8(workingFormat).get(), formatsPref.Data(),
                NS_ConvertUTF16toUTF8(formatsWithIssues).get());
            ReportAnalysis(mDocument, *id, true, workingFormat);
            // This particular Notification&ReportId has been solved, no need
            // to keep looking at other keysys/formats that might solve it too.
            solved = true;
            break;
          }
        }
        if (solved) {
          break;
        }
      }
      if (!solved) {
        DD_DEBUG(
            "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
            "%s not solved (pref(%s)='%s')",
            this, mDocument, id->mReportStringId, formatsPref.Data(),
            NS_ConvertUTF16toUTF8(formatsWithIssues).get());
      }
    }
  }

  // Look at Key System issues first, as they take precedence over format
  // checks.
  if (!unsupportedKeySystems.IsEmpty() && supportedKeySystems.IsEmpty()) {
    // No supported key systems!
    switch (lastKeySystemIssue) {
      case DecoderDoctorDiagnostics::eWidevineWithNoWMF:
        DD_INFO(
            "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
            "unsupported key systems: %s, Widevine without WMF",
            this, mDocument,
            NS_ConvertUTF16toUTF8(unsupportedKeySystems).get());
        ReportAnalysis(mDocument, sMediaWidevineNoWMF, false,
                       unsupportedKeySystems);
        return;
      default:
        break;
    }
  }

  // Next, check playability of requested formats.
  if (!unplayableFormats.IsEmpty()) {
    // Some requested formats cannot be played.
    if (playableFormats.IsEmpty()) {
      // No requested formats can be played. See if we can help the user, by
      // going through expected decoders from most to least desirable.
#if defined(XP_WIN)
      if (!formatsRequiringWMF.IsEmpty()) {
        DD_INFO(
            "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
            "unplayable formats: %s -> Cannot play media because WMF was not "
            "found",
            this, mDocument, NS_ConvertUTF16toUTF8(formatsRequiringWMF).get());
        ReportAnalysis(mDocument, sMediaWMFNeeded, false, formatsRequiringWMF);
        return;
      }
#endif
#if defined(MOZ_FFMPEG)
      if (!formatsRequiringFFMpeg.IsEmpty()) {
        switch (FFmpegRuntimeLinker::LinkStatusCode()) {
          case FFmpegRuntimeLinker::LinkStatus_INVALID_FFMPEG_CANDIDATE:
          case FFmpegRuntimeLinker::LinkStatus_UNUSABLE_LIBAV57:
          case FFmpegRuntimeLinker::LinkStatus_INVALID_LIBAV_CANDIDATE:
          case FFmpegRuntimeLinker::LinkStatus_OBSOLETE_FFMPEG:
          case FFmpegRuntimeLinker::LinkStatus_OBSOLETE_LIBAV:
          case FFmpegRuntimeLinker::LinkStatus_INVALID_CANDIDATE:
            DD_INFO(
                "DecoderDoctorDocumentWatcher[%p, "
                "doc=%p]::SynthesizeAnalysis() - unplayable formats: %s -> "
                "Cannot play media because of unsupported %s (Reason: %s)",
                this, mDocument,
                NS_ConvertUTF16toUTF8(formatsRequiringFFMpeg).get(),
                FFmpegRuntimeLinker::LinkStatusLibraryName(),
                FFmpegRuntimeLinker::LinkStatusString());
            ReportAnalysis(mDocument, sUnsupportedLibavcodec, false,
                           formatsRequiringFFMpeg);
            return;
          case FFmpegRuntimeLinker::LinkStatus_INIT:
            MOZ_FALLTHROUGH_ASSERT("Unexpected LinkStatus_INIT");
          case FFmpegRuntimeLinker::LinkStatus_SUCCEEDED:
            MOZ_FALLTHROUGH_ASSERT("Unexpected LinkStatus_SUCCEEDED");
          case FFmpegRuntimeLinker::LinkStatus_NOT_FOUND:
            DD_INFO(
                "DecoderDoctorDocumentWatcher[%p, "
                "doc=%p]::SynthesizeAnalysis() - unplayable formats: %s -> "
                "Cannot play media because platform decoder was not found "
                "(Reason: %s)",
                this, mDocument,
                NS_ConvertUTF16toUTF8(formatsRequiringFFMpeg).get(),
                FFmpegRuntimeLinker::LinkStatusString());
            ReportAnalysis(mDocument, sMediaPlatformDecoderNotFound, false,
                           formatsRequiringFFMpeg);
            return;
        }
      }
#endif
      DD_INFO(
          "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
          "Cannot play media, unplayable formats: %s",
          this, mDocument, NS_ConvertUTF16toUTF8(unplayableFormats).get());
      ReportAnalysis(mDocument, sMediaCannotPlayNoDecoders, false,
                     unplayableFormats);
      return;
    }

    DD_INFO(
        "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - Can "
        "play media, but no decoders for some requested formats: %s",
        this, mDocument, NS_ConvertUTF16toUTF8(unplayableFormats).get());
    if (Preferences::GetBool("media.decoder-doctor.verbose", false)) {
      ReportAnalysis(mDocument, sMediaNoDecoders, false, unplayableFormats);
    }
    return;
  }

  if (firstDecodeError) {
    DD_INFO(
        "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
        "Decode error: %s",
        this, mDocument, firstDecodeError->Description().get());
    ReportAnalysis(mDocument, sMediaDecodeError, false, NS_LITERAL_STRING(""),
                   *firstDecodeError,
                   true,  // aDecodeIssueIsError=true
                   mDocument->GetDocumentURI()->GetSpecOrDefault(),
                   *firstDecodeErrorMediaSrc);
    return;
  }

  if (firstDecodeWarning) {
    DD_INFO(
        "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - "
        "Decode warning: %s",
        this, mDocument, firstDecodeWarning->Description().get());
    ReportAnalysis(mDocument, sMediaDecodeWarning, false, NS_LITERAL_STRING(""),
                   *firstDecodeWarning,
                   false,  // aDecodeIssueIsError=false
                   mDocument->GetDocumentURI()->GetSpecOrDefault(),
                   *firstDecodeWarningMediaSrc);
    return;
  }

  DD_DEBUG(
      "DecoderDoctorDocumentWatcher[%p, doc=%p]::SynthesizeAnalysis() - Can "
      "play media, decoders available for all requested formats",
      this, mDocument);
}

void DecoderDoctorDocumentWatcher::AddDiagnostics(
    DecoderDoctorDiagnostics&& aDiagnostics, const char* aCallSite) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDiagnostics.Type() != DecoderDoctorDiagnostics::eEvent);

  if (!mDocument) {
    return;
  }

  const mozilla::TimeStamp now = mozilla::TimeStamp::Now();

  constexpr size_t MaxDiagnostics = 128;
  constexpr double MaxSeconds = 10.0;
  while (
      mDiagnosticsSequence.Length() > MaxDiagnostics ||
      (!mDiagnosticsSequence.IsEmpty() &&
       (now - mDiagnosticsSequence[0].mTimeStamp).ToSeconds() > MaxSeconds)) {
    // Too many, or too old.
    mDiagnosticsSequence.RemoveElementAt(0);
    if (mDiagnosticsHandled != 0) {
      // Make sure Notify picks up the new element added below.
      --mDiagnosticsHandled;
    }
  }

  DD_DEBUG(
      "DecoderDoctorDocumentWatcher[%p, "
      "doc=%p]::AddDiagnostics(DecoderDoctorDiagnostics{%s}, call site '%s')",
      this, mDocument, aDiagnostics.GetDescription().Data(), aCallSite);
  mDiagnosticsSequence.AppendElement(
      Diagnostics(std::move(aDiagnostics), aCallSite, now));
  EnsureTimerIsStarted();
}

NS_IMETHODIMP
DecoderDoctorDocumentWatcher::Notify(nsITimer* timer) {
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
    DD_DEBUG(
        "DecoderDoctorDocumentWatcher[%p, doc=%p]::Notify() - No new "
        "diagnostics to analyze -> Stop watching",
        this, mDocument);
    // Stop watching this document, we don't expect more diagnostics for now.
    // If more diagnostics come in, we'll treat them as another burst,
    // separately. 'true' to remove the property from the document.
    StopWatching(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
DecoderDoctorDocumentWatcher::GetName(nsACString& aName) {
  aName.AssignLiteral("DecoderDoctorDocumentWatcher_timer");
  return NS_OK;
}

void DecoderDoctorDiagnostics::StoreFormatDiagnostics(dom::Document* aDocument,
                                                      const nsAString& aFormat,
                                                      bool aCanPlay,
                                                      const char* aCallSite) {
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure we only store once.
  MOZ_ASSERT(mDiagnosticsType == eUnsaved);
  mDiagnosticsType = eFormatSupportCheck;

  if (NS_WARN_IF(aFormat.Length() > 2048)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreFormatDiagnostics(Document* "
        "aDocument=%p, format= TOO LONG! '%s', can-play=%d, call site '%s')",
        aDocument, this, NS_ConvertUTF16toUTF8(aFormat).get(), aCanPlay,
        aCallSite);
    return;
  }

  if (NS_WARN_IF(!aDocument)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreFormatDiagnostics(Document* "
        "aDocument=nullptr, format='%s', can-play=%d, call site '%s')",
        this, NS_ConvertUTF16toUTF8(aFormat).get(), aCanPlay, aCallSite);
    return;
  }
  if (NS_WARN_IF(aFormat.IsEmpty())) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreFormatDiagnostics(Document* "
        "aDocument=%p, format=<empty>, can-play=%d, call site '%s')",
        this, aDocument, aCanPlay, aCallSite);
    return;
  }

  RefPtr<DecoderDoctorDocumentWatcher> watcher =
      DecoderDoctorDocumentWatcher::RetrieveOrCreate(aDocument);

  if (NS_WARN_IF(!watcher)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreFormatDiagnostics(Document* "
        "aDocument=%p, format='%s', can-play=%d, call site '%s') - Could not "
        "create document watcher",
        this, aDocument, NS_ConvertUTF16toUTF8(aFormat).get(), aCanPlay,
        aCallSite);
    return;
  }

  mFormat = aFormat;
  mCanPlay = aCanPlay;

  // StoreDiagnostics should only be called once, after all data is available,
  // so it is safe to std::move() from this object.
  watcher->AddDiagnostics(std::move(*this), aCallSite);
  // Even though it's moved-from, the type should stay set
  // (Only used to ensure that we do store only once.)
  MOZ_ASSERT(mDiagnosticsType == eFormatSupportCheck);
}

void DecoderDoctorDiagnostics::StoreMediaKeySystemAccess(
    dom::Document* aDocument, const nsAString& aKeySystem, bool aIsSupported,
    const char* aCallSite) {
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure we only store once.
  MOZ_ASSERT(mDiagnosticsType == eUnsaved);
  mDiagnosticsType = eMediaKeySystemAccessRequest;

  if (NS_WARN_IF(aKeySystem.Length() > 2048)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreMediaKeySystemAccess(Document* "
        "aDocument=%p, keysystem= TOO LONG! '%s', supported=%d, call site "
        "'%s')",
        aDocument, this, NS_ConvertUTF16toUTF8(aKeySystem).get(), aIsSupported,
        aCallSite);
    return;
  }

  if (NS_WARN_IF(!aDocument)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreMediaKeySystemAccess(Document* "
        "aDocument=nullptr, keysystem='%s', supported=%d, call site '%s')",
        this, NS_ConvertUTF16toUTF8(aKeySystem).get(), aIsSupported, aCallSite);
    return;
  }
  if (NS_WARN_IF(aKeySystem.IsEmpty())) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreMediaKeySystemAccess(Document* "
        "aDocument=%p, keysystem=<empty>, supported=%d, call site '%s')",
        this, aDocument, aIsSupported, aCallSite);
    return;
  }

  RefPtr<DecoderDoctorDocumentWatcher> watcher =
      DecoderDoctorDocumentWatcher::RetrieveOrCreate(aDocument);

  if (NS_WARN_IF(!watcher)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreMediaKeySystemAccess(Document* "
        "aDocument=%p, keysystem='%s', supported=%d, call site '%s') - Could "
        "not create document watcher",
        this, aDocument, NS_ConvertUTF16toUTF8(aKeySystem).get(), aIsSupported,
        aCallSite);
    return;
  }

  mKeySystem = aKeySystem;
  mIsKeySystemSupported = aIsSupported;

  // StoreMediaKeySystemAccess should only be called once, after all data is
  // available, so it is safe to std::move() from this object.
  watcher->AddDiagnostics(std::move(*this), aCallSite);
  // Even though it's moved-from, the type should stay set
  // (Only used to ensure that we do store only once.)
  MOZ_ASSERT(mDiagnosticsType == eMediaKeySystemAccessRequest);
}

void DecoderDoctorDiagnostics::StoreEvent(dom::Document* aDocument,
                                          const DecoderDoctorEvent& aEvent,
                                          const char* aCallSite) {
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure we only store once.
  MOZ_ASSERT(mDiagnosticsType == eUnsaved);
  mDiagnosticsType = eEvent;
  mEvent = aEvent;

  if (NS_WARN_IF(!aDocument)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreEvent(Document* "
        "aDocument=nullptr, aEvent=%s, call site '%s')",
        this, GetDescription().get(), aCallSite);
    return;
  }

  // Don't keep events for later processing, just handle them now.
#ifdef MOZ_PULSEAUDIO
  switch (aEvent.mDomain) {
    case DecoderDoctorEvent::eAudioSinkStartup:
      if (aEvent.mResult == NS_ERROR_DOM_MEDIA_CUBEB_INITIALIZATION_ERR) {
        DD_INFO(
            "DecoderDoctorDocumentWatcher[%p, doc=%p]::AddDiagnostics() - "
            "unable to initialize PulseAudio",
            this, aDocument);
        ReportAnalysis(aDocument, sCannotInitializePulseAudio, false,
                       NS_LITERAL_STRING("*"));
      } else if (aEvent.mResult == NS_OK) {
        DD_INFO(
            "DecoderDoctorDocumentWatcher[%p, doc=%p]::AddDiagnostics() - now "
            "able to initialize PulseAudio",
            this, aDocument);
        ReportAnalysis(aDocument, sCannotInitializePulseAudio, true,
                       NS_LITERAL_STRING("*"));
      }
      break;
  }
#endif  // MOZ_PULSEAUDIO
}

void DecoderDoctorDiagnostics::StoreDecodeError(dom::Document* aDocument,
                                                const MediaResult& aError,
                                                const nsString& aMediaSrc,
                                                const char* aCallSite) {
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure we only store once.
  MOZ_ASSERT(mDiagnosticsType == eUnsaved);
  mDiagnosticsType = eDecodeError;

  if (NS_WARN_IF(aError.Message().Length() > 2048)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreDecodeError(Document* "
        "aDocument=%p, aError= TOO LONG! '%s', aMediaSrc=<provided>, call site "
        "'%s')",
        aDocument, this, aError.Description().get(), aCallSite);
    return;
  }

  if (NS_WARN_IF(aMediaSrc.Length() > 2048)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreDecodeError(Document* "
        "aDocument=%p, aError=%s, aMediaSrc= TOO LONG! <provided>, call site "
        "'%s')",
        aDocument, this, aError.Description().get(), aCallSite);
    return;
  }

  if (NS_WARN_IF(!aDocument)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreDecodeError("
        "Document* aDocument=nullptr, aError=%s,"
        " aMediaSrc=<provided>, call site '%s')",
        this, aError.Description().get(), aCallSite);
    return;
  }

  RefPtr<DecoderDoctorDocumentWatcher> watcher =
      DecoderDoctorDocumentWatcher::RetrieveOrCreate(aDocument);

  if (NS_WARN_IF(!watcher)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreDecodeError("
        "Document* aDocument=%p, aError='%s', aMediaSrc=<provided>,"
        " call site '%s') - Could not create document watcher",
        this, aDocument, aError.Description().get(), aCallSite);
    return;
  }

  mDecodeIssue = aError;
  mDecodeIssueMediaSrc = aMediaSrc;

  // StoreDecodeError should only be called once, after all data is
  // available, so it is safe to std::move() from this object.
  watcher->AddDiagnostics(std::move(*this), aCallSite);
  // Even though it's moved-from, the type should stay set
  // (Only used to ensure that we do store only once.)
  MOZ_ASSERT(mDiagnosticsType == eDecodeError);
}

void DecoderDoctorDiagnostics::StoreDecodeWarning(dom::Document* aDocument,
                                                  const MediaResult& aWarning,
                                                  const nsString& aMediaSrc,
                                                  const char* aCallSite) {
  MOZ_ASSERT(NS_IsMainThread());
  // Make sure we only store once.
  MOZ_ASSERT(mDiagnosticsType == eUnsaved);
  mDiagnosticsType = eDecodeWarning;

  if (NS_WARN_IF(!aDocument)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreDecodeWarning("
        "Document* aDocument=nullptr, aWarning=%s,"
        " aMediaSrc=<provided>, call site '%s')",
        this, aWarning.Description().get(), aCallSite);
    return;
  }

  RefPtr<DecoderDoctorDocumentWatcher> watcher =
      DecoderDoctorDocumentWatcher::RetrieveOrCreate(aDocument);

  if (NS_WARN_IF(!watcher)) {
    DD_WARN(
        "DecoderDoctorDiagnostics[%p]::StoreDecodeWarning("
        "Document* aDocument=%p, aWarning='%s', aMediaSrc=<provided>,"
        " call site '%s') - Could not create document watcher",
        this, aDocument, aWarning.Description().get(), aCallSite);
    return;
  }

  mDecodeIssue = aWarning;
  mDecodeIssueMediaSrc = aMediaSrc;

  // StoreDecodeWarning should only be called once, after all data is
  // available, so it is safe to std::move() from this object.
  watcher->AddDiagnostics(std::move(*this), aCallSite);
  // Even though it's moved-from, the type should stay set
  // (Only used to ensure that we do store only once.)
  MOZ_ASSERT(mDiagnosticsType == eDecodeWarning);
}

static const char* EventDomainString(DecoderDoctorEvent::Domain aDomain) {
  switch (aDomain) {
    case DecoderDoctorEvent::eAudioSinkStartup:
      return "audio-sink-startup";
  }
  return "?";
}

nsCString DecoderDoctorDiagnostics::GetDescription() const {
  nsCString s;
  switch (mDiagnosticsType) {
    case eUnsaved:
      s = "Unsaved diagnostics, cannot get accurate description";
      break;
    case eFormatSupportCheck:
      s = "format='";
      s += NS_ConvertUTF16toUTF8(mFormat).get();
      s += mCanPlay ? "', can play" : "', cannot play";
      if (mVideoNotSupported) {
        s += ", but video format not supported";
      }
      if (mAudioNotSupported) {
        s += ", but audio format not supported";
      }
      if (mWMFFailedToLoad) {
        s += ", Windows platform decoder failed to load";
      }
      if (mFFmpegFailedToLoad) {
        s += ", Linux platform decoder failed to load";
      }
      if (mGMPPDMFailedToStartup) {
        s += ", GMP PDM failed to startup";
      } else if (!mGMP.IsEmpty()) {
        s += ", Using GMP '";
        s += mGMP;
        s += "'";
      }
      break;
    case eMediaKeySystemAccessRequest:
      s = "key system='";
      s += NS_ConvertUTF16toUTF8(mKeySystem).get();
      s += mIsKeySystemSupported ? "', supported" : "', not supported";
      switch (mKeySystemIssue) {
        case eUnset:
          break;
        case eWidevineWithNoWMF:
          s += ", Widevine with no WMF";
          break;
      }
      break;
    case eEvent:
      s = nsPrintfCString("event domain %s result=%" PRIu32,
                          EventDomainString(mEvent.mDomain),
                          static_cast<uint32_t>(mEvent.mResult));
      break;
    case eDecodeError:
      s = "decode error: ";
      s += mDecodeIssue.Description();
      s += ", src='";
      s += mDecodeIssueMediaSrc.IsEmpty() ? "<none>" : "<provided>";
      s += "'";
      break;
    case eDecodeWarning:
      s = "decode warning: ";
      s += mDecodeIssue.Description();
      s += ", src='";
      s += mDecodeIssueMediaSrc.IsEmpty() ? "<none>" : "<provided>";
      s += "'";
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected DiagnosticsType");
      s = "?";
      break;
  }
  return s;
}

}  // namespace mozilla
