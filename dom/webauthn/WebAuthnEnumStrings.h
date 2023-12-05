/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnEnumStrings_h
#define mozilla_dom_WebAuthnEnumStrings_h

// WARNING: This version number must match the WebAuthn level where the strings
// below are defined.
#define MOZ_WEBAUTHN_ENUM_STRINGS_VERSION 3

// https://www.w3.org/TR/webauthn-2/#enum-attestation-convey
#define MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_NONE "none"
#define MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_INDIRECT "indirect"
#define MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_DIRECT "direct"
#define MOZ_WEBAUTHN_ATTESTATION_CONVEYANCE_PREFERENCE_ENTERPRISE "enterprise"
// WARNING: Change version number when adding new values!

// https://www.w3.org/TR/webauthn-2/#enum-attachment
#define MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM "platform"
#define MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM "cross-platform"
// WARNING: Change version number when adding new values!

// https://www.w3.org/TR/webauthn-2/#enum-credentialType
#define MOZ_WEBAUTHN_PUBLIC_KEY_CREDENTIAL_TYPE_PUBLIC_KEY "public-key"
// WARNING: Change version number when adding new values!

// https://www.w3.org/TR/webauthn-2/#enum-residentKeyRequirement
#define MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_REQUIRED "required"
#define MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_PREFERRED "preferred"
#define MOZ_WEBAUTHN_RESIDENT_KEY_REQUIREMENT_DISCOURAGED "discouraged"
// WARNING: Change version number when adding new values!

// https://www.w3.org/TR/webauthn-2/#enum-userVerificationRequirement
#define MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED "required"
#define MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED "preferred"
#define MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED "discouraged"
// WARNING: Change version number when adding new values!

// https://www.w3.org/TR/webauthn-2/#enum-transport
#define MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_USB "usb"
#define MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_NFC "nfc"
#define MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_BLE "ble"
#define MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_INTERNAL "internal"
#define MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_HYBRID "hybrid"
// WARNING: Change version number when adding new values!

#endif  // mozilla_dom_WebAuthnEnumStrings_h
