/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_MEDIAKEYSYSTEMACCESSMANAGER_H_
#define DOM_MEDIA_MEDIAKEYSYSTEMACCESSMANAGER_H_

#include "mozilla/dom/MediaKeySystemAccess.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsISupportsImpl.h"
#include "nsITimer.h"

namespace mozilla {
namespace dom {

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
 * While the pipeline metaphor generally holds, the implementation details of
 * the manager mean that processing is not always linear: a request may be
 * re-injected earlier into the pipeline for reprocessing. This can happen
 * if the request was pending some other operation, e.g. CDM install, after
 * which we wish to reprocess that request.
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
 *            Provide access
 *
 */

class MediaKeySystemAccessManager final : public nsIObserver {
 public:
  explicit MediaKeySystemAccessManager(nsPIDOMWindowInner* aWindow);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(MediaKeySystemAccessManager,
                                           nsIObserver)
  NS_DECL_NSIOBSERVER

  // Entry point for the navigator to call into the manager.
  void Request(DetailedPromise* aPromise, const nsAString& aKeySystem,
               const Sequence<MediaKeySystemConfiguration>& aConfig);

  void Shutdown();

 private:
  // Encapsulates the information for a Navigator.requestMediaKeySystemAccess()
  // request that is being processed.
  struct PendingRequest {
    enum class RequestType { Initial, Subsequent };

    PendingRequest(DetailedPromise* aPromise, const nsAString& aKeySystem,
                   const Sequence<MediaKeySystemConfiguration>& aConfigs);
    ~PendingRequest();

    // The JS promise associated with this request.
    RefPtr<DetailedPromise> mPromise;
    // The KeySystem passed for this request.
    const nsString mKeySystem;
    // The config(s) passed for this request.
    const Sequence<MediaKeySystemConfiguration> mConfigs;
    // If the request is
    // - A first attempt request from JS: RequestType::Initial.
    // - A request we're reprocessing due to a GMP being installed:
    //   RequestType::Subsequent.
    RequestType mRequestType = RequestType::Initial;

    // Will be set to trigger a timeout and re-processing of the request if the
    // request is pending on some potentially time consuming operation, e.g.
    // CDM install.
    nsCOMPtr<nsITimer> mTimer = nullptr;

    // Convenience methods to reject the wrapped promise.
    void RejectPromiseWithInvalidAccessError(const nsAString& aReason);
    void RejectPromiseWithNotSupportedError(const nsAString& aReason);
    void RejectPromiseWithTypeError(const nsAString& aReason);

    void CancelTimer();
  };

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
  // request once the CDM is installed or a timeout is reached. This is the
  // last step in processing, and is the only one that can approve the request
  // (though it may still reject it).
  void RequestMediaKeySystemAccess(UniquePtr<PendingRequest> aRequest);

  ~MediaKeySystemAccessManager();

  bool EnsureObserversAdded();

  bool AwaitInstall(UniquePtr<PendingRequest> aRequest);

  void RetryRequest(UniquePtr<PendingRequest> aRequest);

  // Requests waiting on CDM installation to be processed.
  nsTArray<UniquePtr<PendingRequest>> mPendingInstallRequests;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mAddedObservers;
};

}  // namespace dom
}  // namespace mozilla

#endif  // DOM_MEDIA_MEDIAKEYSYSTEMACCESSMANAGER_H_
