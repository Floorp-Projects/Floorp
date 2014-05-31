/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

callback IdentityOnReadyCallback = void();
callback IdentityOnLoginCallback = void(DOMString identityAssertion);
callback IdentityOnLogoutCallback = void();
callback IdentityOnCancelCallback = void(DOMString? error);
callback IdentityOnErrorCallback = void(DOMString error);

dictionary IdentityWatchOptions {
  // Required callback
  IdentityOnLoginCallback onlogin;

  // Optional parameters
  DOMString wantIssuer;
  DOMString loggedInUser;

  // Optional callbacks
  IdentityOnReadyCallback onready;
  IdentityOnLogoutCallback onlogout;
  IdentityOnErrorCallback onerror;

  // Certified apps can specify this
  DOMString audience;
};

dictionary IdentityRequestOptions {
  // Optional parameters
  long refreshAuthentication;
  DOMString termsOfService;
  DOMString privacyPolicy;
  DOMString backgroundColor;
  DOMString siteLogo;
  DOMString siteName;
  DOMString returnTo;

  IdentityOnCancelCallback oncancel;

  // Certified apps can specify this
  DOMString origin;
};

dictionary IdentityGetOptions {
  DOMString privacyPolicy;
  DOMString termsOfService;
  DOMString privacyURL;
  DOMString tosURL;
  DOMString siteName;
  DOMString siteLogo;
};

[JSImplementation="@mozilla.org/identity/manager;1",
 NoInterfaceObject,
 NavigatorProperty="mozId",
 Pref="dom.identity.enabled"]
interface IdentityManager {
  void watch(optional IdentityWatchOptions options);
  void request(optional IdentityRequestOptions options);
  void logout();

  [Pref="dom.identity.exposeLegacyGetAPI"]
  void get(IdentityOnLoginCallback callback, optional IdentityGetOptions options);

  [Pref="dom.identity.exposeLegacyGetVerifiedEmailAPI"]
  void getVerifiedEmail(IdentityOnLoginCallback callback);
};

