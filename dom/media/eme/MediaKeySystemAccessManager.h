/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIAKEYSYSTEMACCESSMANAGER_H_
#define DOM_MEDIA_MEDIAKEYSYSTEMACCESSMANAGER_H_

#include "mozilla/dom/MediaKeySystemAccess.h"
#include "mozilla/MozPromise.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"

namespace mozilla::dom {

class DetailedPromise;
class TestGMPVideoDecoder;

/**
 * MediaKeySystemAccessManager implements the functionality for
 * Navigator.requestMediaKeySystemAccess(). The navigator may perform its own
 * logic before passing the request to this class, but the majority of
 * processing happens the MediaKeySystemAccessManager. The manager is expected
 * to be run entirely on the main thread of the content process for whichever
 * window it is associated with.
 *
 * As well as implementing the Navigator.requestMediaKeySystemAccess()
 * algorithm, the manager performs Gecko specific logic. For example, the EME
 * specification does not specify a process to check if a CDM is installed as
 * part of requesting access, but that is an important part of obtaining access
 * for Gecko, and is handled by the manager.
 *
 * A request made to the manager can be thought of as entering a pipeline.
 * In this pipeline the request must pass through various stages that can
 * reject the request and remove it from the pipeline. If a request is not
 * rejected by the end of the pipeline it is approved/resolved.
 *
 * The pipeline is structured in such a way that each step should be executed
 * even if it will quickly be exited. For example, the step that checks if a
 * window supports protected media is an instant approve on non-Windows OSes,
 * but we want to execute the function representing that step to ensure a
 * deterministic execution and logging path. The hope is this reduces
 * complexity for when we need to debug or change the code.
 *
 * While the pipeline metaphor generally holds, the implementation details of
 * the manager mean that processing is not always linear: a request may be
 * re-injected earlier into the pipeline for reprocessing. This can happen
 * if the request was pending some other operation, e.g. CDM install, after
 * which we wish to reprocess that request. However, we strive to keep it
 * as linear as possible.
 *
 * A high level version of the happy path pipeline is depicted below. If a
 * request were to fail any of the steps below it would be rejected and ejected
 * from the pipeline.
 *
 *       Request arrives from navigator
 *                   +
 *                   |
 *                   v
 *  Check if window supports protected media
 *                   +
 *                   +<-------------------+
 *                   v                    |
 *       Check request args are sane      |
 *                   +                    |
 *                   |           Wait for CDM and retry
 *                   v                    |
 *        Check if CDM is installed       |
 *                   +                    |
 *                   |                    |
 *                   +--------------------+
 *                   |
 *                   v
 *       Check if CDM supports args
 *                   +
 *                   |
 *                   v
 *    Check if app allows protected media
 *           (used by GeckoView)
 *                   +
 *                   |
 *                   v
 *            Provide access
 *
 */

struct MediaKeySystemAccessRequest {
  MediaKeySystemAccessRequest(
      const nsAString& aKeySystem,
      const Sequence<MediaKeySystemConfiguration>& aConfigs)
      : mKeySystem(aKeySystem), mConfigs(aConfigs) {}
  virtual ~MediaKeySystemAccessRequest() = default;
  // The KeySystem passed for this request.
  const nsString mKeySystem;
  // The config(s) passed for this request.
  const Sequence<MediaKeySystemConfiguration> mConfigs;
};

class MediaKeySystemAccessManager final : public nsIObserver, public nsINamed {
 public:
  explicit MediaKeySystemAccessManager(nsPIDOMWindowInner* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(MediaKeySystemAccessManager,
                                           nsIObserver)
  NS_DECL_NSIOBSERVER
  NS_DECL_NSINAMED

  // Entry point for the navigator to call into the manager.
  void Request(DetailedPromise* aPromise, const nsAString& aKeySystem,
               const Sequence<MediaKeySystemConfiguration>& aConfig);

  void Shutdown();

 private:
  // Encapsulates the information for a Navigator.requestMediaKeySystemAccess()
  // request that is being processed.
  struct PendingRequest : public MediaKeySystemAccessRequest {
    enum class RequestType { Initial, Subsequent };

    PendingRequest(DetailedPromise* aPromise, const nsAString& aKeySystem,
                   const Sequence<MediaKeySystemConfiguration>& aConfigs);
    ~PendingRequest();

    // The JS promise associated with this request.
    RefPtr<DetailedPromise> mPromise;

    // If the request is
    // - A first attempt request from JS: RequestType::Initial.
    // - A request we're reprocessing due to a GMP being installed:
    //   RequestType::Subsequent.
    RequestType mRequestType = RequestType::Initial;

    // If we find a supported config for this request during processing it
    // should be stored here. Only if we have a supported config should a
    // request have access provided.
    Maybe<MediaKeySystemConfiguration> mSupportedConfig;

    // Will be set to trigger a timeout and re-processing of the request if the
    // request is pending on some potentially time consuming operation, e.g.
    // CDM install.
    nsCOMPtr<nsITimer> mTimer = nullptr;

    // Convenience methods to reject the wrapped promise.
    void RejectPromiseWithInvalidAccessError(const nsACString& aReason);
    void RejectPromiseWithNotSupportedError(const nsACString& aReason);
    void RejectPromiseWithTypeError(const nsACString& aReason);

    void CancelTimer();
  };

  // Check if the application (e.g. a GeckoView app) allows protected media in
  // this window.
  //
  // This function is always expected to be executed as part of the pipeline of
  // processing a request, but its behavior differs depending on prefs set.
  //
  // If the `media_eme_require_app_approval` pref is false, then the function
  // assumes app approval and early returns. Otherwise the function will
  // create a permission request to be approved by the embedding app. If the
  // test prefs detailed in MediaKeySystemAccessPermissionRequest.h are set
  // then they will control handling, otherwise it is up to the embedding
  // app to handle the request.
  //
  // At the time of writing, only GeckoView based apps are expected to pref
  // on this behavior.
  //
  // This function is expected to run late/last in the pipeline so that if we
  // ask the app for permission we don't fail after the app okays the request.
  // This is to avoid cases where a user may be prompted by the app to approve
  // eme, this check then passes, but we fail later in the pipeline, leaving
  // the user wondering why their approval didn't work.
  void CheckDoesAppAllowProtectedMedia(UniquePtr<PendingRequest> aRequest);

  // Handles the result of the app allowing or disallowing protected media.
  // If there are pending requests in mPendingAppApprovalRequests then this
  // needs to be called on each.
  void OnDoesAppAllowProtectedMedia(bool aIsAllowed,
                                    UniquePtr<PendingRequest> aRequest);

  // Checks if the Window associated with this manager supports protected media
  // and calls into OnDoesWindowSupportEncryptedMedia with the result.
  void CheckDoesWindowSupportProtectedMedia(UniquePtr<PendingRequest> aRequest);

  // Handle the result of checking if the window associated with this manager
  // supports protected media. If the window doesn't support protected media
  // this will reject the request, otherwise the request will continue to be
  // processed.
  void OnDoesWindowSupportProtectedMedia(bool aIsSupportedInWindow,
                                         UniquePtr<PendingRequest> aRequest);

  // Performs the 'requestMediaKeySystemAccess' algorithm detailed in the EME
  // specification. Gecko may need to install a CDM to satisfy this check. If
  // CDM install is needed this function may be called again for the same
  // request once the CDM is installed or a timeout is reached.
  void RequestMediaKeySystemAccess(UniquePtr<PendingRequest> aRequest);

  // Approves aRequest and provides MediaKeySystemAccess by resolving the
  // promise associated with the request.
  void ProvideAccess(UniquePtr<PendingRequest> aRequest);

  ~MediaKeySystemAccessManager();

  bool EnsureObserversAdded();

  bool AwaitInstall(UniquePtr<PendingRequest> aRequest);

  void RetryRequest(UniquePtr<PendingRequest> aRequest);

  // Requests waiting on approval from the application to be processed.
  nsTArray<UniquePtr<PendingRequest>> mPendingAppApprovalRequests;

  // Requests waiting on CDM installation to be processed.
  nsTArray<UniquePtr<PendingRequest>> mPendingInstallRequests;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mAddedObservers = false;

  // Has the app approved protected media playback? If it has we cache the
  // value so we don't need to check again.
  Maybe<bool> mAppAllowsProtectedMedia;

  // If we're waiting for permission from the app to enable EME this holder
  // should contain the request.
  //
  // Note the type in the holder should match
  // MediaKeySystemAccessPermissionRequest::RequestPromise, but we can't
  // include MediaKeySystemAccessPermissionRequest's header here without
  // breaking the build, so we do this hack.
  MozPromiseRequestHolder<MozPromise<bool, bool, true>>
      mAppAllowsProtectedMediaPromiseRequest;
};

}  // namespace mozilla::dom

#endif  // DOM_MEDIA_MEDIAKEYSYSTEMACCESSMANAGER_H_
