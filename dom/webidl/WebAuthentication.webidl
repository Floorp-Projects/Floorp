/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/webauthn/
 */

/***** Interfaces to Data *****/

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface PublicKeyCredential : Credential {
    [SameObject] readonly attribute ArrayBuffer              rawId;
    [SameObject] readonly attribute AuthenticatorResponse    response;
    AuthenticationExtensionsClientOutputs getClientExtensionResults();
};

[SecureContext]
partial interface PublicKeyCredential {
    [NewObject] static Promise<boolean> isUserVerifyingPlatformAuthenticatorAvailable();
    // isExternalCTAP2SecurityKeySupported is non-standard; see Bug 1526023
    [NewObject] static Promise<boolean> isExternalCTAP2SecurityKeySupported();
};

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface AuthenticatorResponse {
    [SameObject] readonly attribute ArrayBuffer clientDataJSON;
};

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface AuthenticatorAttestationResponse : AuthenticatorResponse {
    [SameObject] readonly attribute ArrayBuffer attestationObject;
};

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface AuthenticatorAssertionResponse : AuthenticatorResponse {
    [SameObject] readonly attribute ArrayBuffer      authenticatorData;
    [SameObject] readonly attribute ArrayBuffer      signature;
    [SameObject] readonly attribute ArrayBuffer?     userHandle;
};

dictionary PublicKeyCredentialParameters {
    required PublicKeyCredentialType  type;
    required COSEAlgorithmIdentifier  alg;
};

dictionary PublicKeyCredentialCreationOptions {
    required PublicKeyCredentialRpEntity   rp;
    required PublicKeyCredentialUserEntity user;

    required BufferSource                            challenge;
    required sequence<PublicKeyCredentialParameters> pubKeyCredParams;

    unsigned long                                timeout;
    sequence<PublicKeyCredentialDescriptor>      excludeCredentials = [];
    // FIXME: bug 1493860: should this "= {}" be here?
    AuthenticatorSelectionCriteria               authenticatorSelection = {};
    AttestationConveyancePreference              attestation = "none";
    // FIXME: bug 1493860: should this "= {}" be here?
    AuthenticationExtensionsClientInputs         extensions = {};
};

dictionary PublicKeyCredentialEntity {
    required DOMString    name;
    USVString             icon;
};

dictionary PublicKeyCredentialRpEntity : PublicKeyCredentialEntity {
    DOMString      id;
};

dictionary PublicKeyCredentialUserEntity : PublicKeyCredentialEntity {
    required BufferSource   id;
    required DOMString      displayName;
};

dictionary AuthenticatorSelectionCriteria {
    AuthenticatorAttachment      authenticatorAttachment;
    boolean                      requireResidentKey = false;
    UserVerificationRequirement  userVerification = "preferred";
};

enum AuthenticatorAttachment {
    "platform",       // Platform attachment
    "cross-platform"  // Cross-platform attachment
};

enum AttestationConveyancePreference {
    "none",
    "indirect",
    "direct"
};

enum UserVerificationRequirement {
    "required",
    "preferred",
    "discouraged"
};

dictionary PublicKeyCredentialRequestOptions {
    required BufferSource                challenge;
    unsigned long                        timeout;
    USVString                            rpId;
    sequence<PublicKeyCredentialDescriptor> allowCredentials = [];
    UserVerificationRequirement          userVerification = "preferred";
    // FIXME: bug 1493860: should this "= {}" be here?
    AuthenticationExtensionsClientInputs extensions = {};
};

// TODO - Use partial dictionaries when bug 1436329 is fixed.
dictionary AuthenticationExtensionsClientInputs {
    // FIDO AppID Extension (appid)
    // <https://w3c.github.io/webauthn/#sctn-appid-extension>
    USVString appid;

    // hmac-secret
    // <https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension>
    boolean hmacCreateSecret;
};

// TODO - Use partial dictionaries when bug 1436329 is fixed.
dictionary AuthenticationExtensionsClientOutputs {
    // FIDO AppID Extension (appid)
    // <https://w3c.github.io/webauthn/#sctn-appid-extension>
    boolean appid;

    // <https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension>
    boolean hmacCreateSecret;
};

typedef record<DOMString, DOMString> AuthenticationExtensionsAuthenticatorInputs;

[GenerateToJSON]
dictionary CollectedClientData {
    required DOMString           type;
    required DOMString           challenge;
    required DOMString           origin;
    TokenBinding                 tokenBinding;
};

dictionary TokenBinding {
    required DOMString status;
    DOMString id;
};

enum TokenBindingStatus { "present", "supported" };

enum PublicKeyCredentialType {
    "public-key"
};

dictionary PublicKeyCredentialDescriptor {
    required PublicKeyCredentialType      type;
    required BufferSource                 id;
    // Transports is a string that is matched against the AuthenticatorTransport
    // enumeration so that we have forward-compatibility for new transports.
    sequence<DOMString>                   transports;
};

enum AuthenticatorTransport {
    "usb",
    "nfc",
    "ble",
    "internal"
};

typedef long COSEAlgorithmIdentifier;

typedef sequence<AAGUID>      AuthenticatorSelectionList;

typedef BufferSource      AAGUID;

/*
// FIDO AppID Extension (appid)
// <https://w3c.github.io/webauthn/#sctn-appid-extension>
partial dictionary AuthenticationExtensionsClientInputs {
    USVString appid;
};

// FIDO AppID Extension (appid)
// <https://w3c.github.io/webauthn/#sctn-appid-extension>
partial dictionary AuthenticationExtensionsClientOutputs {
  boolean appid;
};
*/
