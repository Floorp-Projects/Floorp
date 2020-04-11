/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Permission delegate handler provides a policy of how top-level can
 * delegate permission to embedded iframes.
 *
 * This class includes a mechanism to delegate permission using feature
 * policy. Feature policy will assure that only cross-origin iframes which
 * have been explicitly granted access will have the opportunity to request
 * permission.
 *
 * For example if an iframe has not been granted access to geolocation by
 * Feature Policy, geolocation request from the iframe will be automatically
 * denied. if the top-level origin already has access to geolocation and the
 * iframe has been granted access to geolocation by Feature Policy, the iframe
 * will also have access to geolocation. If the top-level frame did not have
 * access to geolocation, and the iframe has been granted access to geolocation
 * by Feature Policy, a request from the cross-origin iframe would trigger a
 * prompt using of the top-level origin.
 */

#ifndef mozilla_PermissionDelegateHandler_h
#define mozilla_PermissionDelegateHandler_h

#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsIPermissionDelegateHandler.h"
#include "nsCOMPtr.h"

class nsIPermissionManager;
class nsIPrincipal;
class nsIContentPermissionRequest;

namespace mozilla {
namespace dom {
class Document;
}

class PermissionDelegateHandler final : public nsIPermissionDelegateHandler {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PermissionDelegateHandler)

  NS_DECL_NSIPERMISSIONDELEGATEHANDLER

  explicit PermissionDelegateHandler() = default;
  explicit PermissionDelegateHandler(mozilla::dom::Document* aDocument);

  bool Initialize();

  /*
   * Indicates if we has the right to make permission request with aType
   */
  bool HasPermissionDelegated(const nsACString& aType);

  /*
   * Get permission state, which applied permission delegate policy.
   *
   * @param aType the permission type to get
   * @param aPermission out argument which will be a permission type that we
   *                    will return from this function.
   * @param aExactHostMatch whether to look for the exact host name or also for
   *                        subdomains that can have the same permission.
   */
  nsresult GetPermission(const nsACString& aType, uint32_t* aPermission,
                         bool aExactHostMatch);

  /*
   * Get permission state for permission api, which applied
   * permission delegate policy.
   *
   * @param aType the permission type to get
   * @param aExactHostMatch whether to look for the exact host name or also for
   *                        subdomains that can have the same permission.
   * @param aPermission out argument which will be a permission type that we
   *                    will return from this function.
   */
  nsresult GetPermissionForPermissionsAPI(const nsACString& aType,
                                          uint32_t* aPermission);

  enum PermissionDelegatePolicy {
    /* Always delegate permission from top level to iframe and the iframe
     * should use top level origin to get/set permission.*/
    eDelegateUseTopOrigin,

    /* Permission is delegated using Feature Policy. Permission is denied by
     * default in cross origin iframe and the iframe only could get/set
     * permission if there's allow attribute set in iframe. e.g allow =
     * "geolocation" */
    eDelegateUseFeaturePolicy,

    /* Persistent denied permissions in cross origin iframe */
    ePersistDeniedCrossOrigin,

    /* This is the old behavior of cross origin iframe permission. The
     * permission delegation should not have an effect on iframe. The cross
     * origin iframe get/set permissions by its origin */
    eDelegateUseIframeOrigin,
  };

  /*
   * Indicates matching between Feature Policy and Permissions name defined in
   * Permissions Manager, not DOM Permissions API. Permissions API exposed in
   * DOM only supports "geo" at the moment but Permissions Manager also supports
   * "camera", "microphone".
   */
  typedef struct {
    const char* mPermissionName;
    const char16_t* mFeatureName;
    PermissionDelegatePolicy mPolicy;
  } PermissionDelegateInfo;

  /**
   * The loader maintains a weak reference to the document with
   * which it is initialized. This call forces the reference to
   * be dropped.
   */
  void DropDocumentReference() { mDocument = nullptr; }

  /*
   * Helper function to return the delegate info value for aPermissionName.
   * @param aPermissionName the permission name to get
   */
  static const PermissionDelegateInfo* GetPermissionDelegateInfo(
      const nsAString& aPermissionName);

  /*
   * Helper function to return the delegate principal. This will return nullptr,
   * or request's principal or top level principal based on the delegate policy
   * will be applied for a given type.
   * We use this function when prompting, no need to perform permission check
   * (deny/allow).
   *
   * @param aType the permission type to get
   * @param aRequest  The request which the principal is get from.
   * @param aResult out argument which will be a principal that we
   *                will return from this function.
   */
  static nsresult GetDelegatePrincipal(const nsACString& aType,
                                       nsIContentPermissionRequest* aRequest,
                                       nsIPrincipal** aResult);

 private:
  ~PermissionDelegateHandler() = default;

  /*
   * Check whether the permission is blocked by FeaturePolicy directive.
   * Default allowlist for a featureName of permission used in permissions
   * delegate should be set to eSelf, to ensure that permission is denied by
   * default and only have the opportunity to request permission with allow
   * attribute.
   */
  bool HasFeaturePolicyAllowed(const PermissionDelegateInfo* info) const;

  // A weak pointer to our document. Nulled out by DropDocumentReference.
  mozilla::dom::Document* mDocument;

  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mTopLevelPrincipal;
  RefPtr<nsIPermissionManager> mPermissionManager;
};

}  // namespace mozilla

#endif  // mozilla_PermissionDelegateHandler_h
