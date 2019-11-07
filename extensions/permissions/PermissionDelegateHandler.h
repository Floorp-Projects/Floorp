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

#ifndef PermissionDelegateHandler_h__
#define PermissionDelegateHandler_h__

#include "nsISupports.h"

namespace mozilla {
namespace dom {
class Document;
}
}  // namespace mozilla

class PermissionDelegateHandler final : nsISupports {
 public:
  explicit PermissionDelegateHandler(mozilla::dom::Document* aDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(PermissionDelegateHandler)

  /*
   * Get permission state for permission api with aType, which applied
   * permission delegate policy.
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

 private:
  virtual ~PermissionDelegateHandler() = default;

  /*
   *  Helper function to return the delegate info value for aPermissionName.
   */
  const PermissionDelegateInfo* GetPermissionDelegateInfo(
      const nsAString& aPermissionName) const;

  // A weak pointer to our document. Nulled out by DropDocumentReference.
  mozilla::dom::Document* mDocument;
};

#endif  // PermissionDelegateHandler_h__
