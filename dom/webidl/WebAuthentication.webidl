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
    readonly attribute ArrayBuffer           rawId;
    readonly attribute AuthenticatorResponse response;
    // Extensions are not supported yet.
    // readonly attribute AuthenticationExtensions clientExtensionResults;
};

[SecureContext, Pref="security.webauth.webauthn"]
interface AuthenticatorResponse {
    readonly attribute ArrayBuffer clientDataJSON;
};

[SecureContext, Pref="security.webauth.webauthn"]
interface AuthenticatorAttestationResponse : AuthenticatorResponse {
    readonly attribute ArrayBuffer attestationObject;
};

dictionary PublicKeyCredentialParameters {
    required PublicKeyCredentialType  type;
    required WebAuthnAlgorithmID algorithm; // NOTE: changed from AllgorithmIdentifier because typedef (object or DOMString) not serializable
};

dictionary PublicKeyCredentialUserEntity : PublicKeyCredentialEntity {
    DOMString displayName;
};

dictionary MakeCredentialOptions {
    required PublicKeyCredentialEntity rp;
    required PublicKeyCredentialUserEntity user;

    required BufferSource                         challenge;
    required sequence<PublicKeyCredentialParameters> parameters;

    unsigned long                        timeout;
    sequence<PublicKeyCredentialDescriptor> excludeList;
    AuthenticatorSelectionCriteria       authenticatorSelection;
    // Extensions are not supported yet.
    // AuthenticationExtensions             extensions;
};

dictionary PublicKeyCredentialEntity {
    DOMString id;
    DOMString name;
    USVString icon;
};

dictionary AuthenticatorSelectionCriteria {
    Attachment    attachment;
    boolean       requireResidentKey = false;
};

enum Attachment {
    "platform",
    "cross-platform"
};

dictionary PublicKeyCredentialRequestOptions {
    required BufferSource                challenge;
    unsigned long                        timeout;
    USVString                            rpId;
    sequence<PublicKeyCredentialDescriptor> allowList = [];
    // Extensions are not supported yet.
    // AuthenticationExtensions             extensions;
};

dictionary CollectedClientData {
    required DOMString           challenge;
    required DOMString           origin;
    required DOMString           hashAlg;
    DOMString                    tokenBinding;
    // Extensions are not supported yet.
    // AuthenticationExtensions     clientExtensions;
    // AuthenticationExtensions     authenticatorExtensions;
};

enum PublicKeyCredentialType {
    "public-key"
};

dictionary PublicKeyCredentialDescriptor {
    required PublicKeyCredentialType type;
    required BufferSource id;
    sequence<WebAuthnTransport>   transports;
};

typedef (boolean or DOMString) WebAuthnAlgorithmID; // Fix when upstream there's a definition of how to serialize AlgorithmIdentifier

[SecureContext, Pref="security.webauth.webauthn"]
interface AuthenticatorAssertionResponse : AuthenticatorResponse {
    readonly attribute ArrayBuffer      authenticatorData;
    readonly attribute ArrayBuffer      signature;
};

// Renamed from "Transport" to avoid a collision with U2F
enum WebAuthnTransport {
    "usb",
    "nfc",
    "ble"
};
