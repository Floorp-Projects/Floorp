/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/webauthn/
 */

/***** Interfaces to Data *****/

[SecureContext, Pref="security.webauth.webauthn"]
interface PublicKeyCredential : Credential {
    [SameObject] readonly attribute ArrayBuffer              rawId;
    [SameObject] readonly attribute AuthenticatorResponse    response;
    // Extensions are not supported yet.
    // [SameObject] readonly attribute AuthenticationExtensions clientExtensionResults; // Add in Bug 1406458
};

[SecureContext]
partial interface PublicKeyCredential {
    static Promise<boolean> isPlatformAuthenticatorAvailable();
};

[SecureContext, Pref="security.webauth.webauthn"]
interface AuthenticatorResponse {
    [SameObject] readonly attribute ArrayBuffer clientDataJSON;
};

[SecureContext, Pref="security.webauth.webauthn"]
interface AuthenticatorAttestationResponse : AuthenticatorResponse {
    [SameObject] readonly attribute ArrayBuffer attestationObject;
};

[SecureContext, Pref="security.webauth.webauthn"]
interface AuthenticatorAssertionResponse : AuthenticatorResponse {
    [SameObject] readonly attribute ArrayBuffer      authenticatorData;
    [SameObject] readonly attribute ArrayBuffer      signature;
    readonly attribute DOMString                     userId;
};

dictionary PublicKeyCredentialParameters {
    required PublicKeyCredentialType  type;
    required WebAuthnAlgorithmID      alg; // Switch to COSE in Bug 1381190
};

dictionary MakePublicKeyCredentialOptions {
    required PublicKeyCredentialRpEntity   rp;
    required PublicKeyCredentialUserEntity user;

    required BufferSource                            challenge;
    required sequence<PublicKeyCredentialParameters> pubKeyCredParams;

    unsigned long                                timeout;
    sequence<PublicKeyCredentialDescriptor>      excludeCredentials = [];
    AuthenticatorSelectionCriteria               authenticatorSelection;
    // Extensions are not supported yet.
    // AuthenticationExtensions                  extensions; // Add in Bug 1406458
};

dictionary PublicKeyCredentialEntity {
    DOMString      name;
    USVString      icon;
};

dictionary PublicKeyCredentialRpEntity : PublicKeyCredentialEntity {
    DOMString      id;
};

dictionary PublicKeyCredentialUserEntity : PublicKeyCredentialEntity {
    BufferSource   id;
    DOMString      displayName;
};

dictionary AuthenticatorSelectionCriteria {
    AuthenticatorAttachment      authenticatorAttachment;
    boolean                      requireResidentKey = false;
    boolean                      requireUserVerification = false;
};

enum AuthenticatorAttachment {
    "platform",       // Platform attachment
    "cross-platform"  // Cross-platform attachment
};

dictionary PublicKeyCredentialRequestOptions {
    required BufferSource                challenge;
    unsigned long                        timeout;
    USVString                            rpId;
    sequence<PublicKeyCredentialDescriptor> allowCredentials = [];
    // Extensions are not supported yet.
    // AuthenticationExtensions             extensions; // Add in Bug 1406458
};

typedef record<DOMString, any>       AuthenticationExtensions;

dictionary CollectedClientData {
    required DOMString           challenge;
    required DOMString           origin;
    required DOMString           hashAlgorithm;
    DOMString                    tokenBindingId;
    // Extensions are not supported yet.
    // AuthenticationExtensions     clientExtensions; // Add in Bug 1406458
    // AuthenticationExtensions     authenticatorExtensions; // Add in Bug 1406458
};

enum PublicKeyCredentialType {
    "public-key"
};

dictionary PublicKeyCredentialDescriptor {
    required PublicKeyCredentialType type;
    required BufferSource id;
    sequence<AuthenticatorTransport>   transports;
};

enum AuthenticatorTransport {
    "usb",
    "nfc",
    "ble"
};

typedef long COSEAlgorithmIdentifier;

typedef sequence<AAGUID>      AuthenticatorSelectionList;

typedef BufferSource      AAGUID;

typedef (boolean or DOMString) WebAuthnAlgorithmID; // Switch to COSE in Bug 1381190
