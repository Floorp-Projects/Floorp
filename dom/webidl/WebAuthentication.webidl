/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/webauthn/
 */

/***** Interfaces to Data *****/

[SecureContext]
interface ScopedCredentialInfo {
    readonly attribute ScopedCredential    credential;
    readonly attribute WebAuthnAttestation attestation;
};

dictionary Account {
    required DOMString rpDisplayName;
    required DOMString displayName;
    required DOMString id;
    DOMString          name;
    DOMString          imageURL;
};

typedef (boolean or DOMString) WebAuthnAlgorithmID; // Fix when upstream there's a definition of how to serialize AlgorithmIdentifier

dictionary ScopedCredentialParameters {
    required ScopedCredentialType type;
    required WebAuthnAlgorithmID  algorithm; // NOTE: changed from AllgorithmIdentifier because typedef (object or DOMString) not serializable
};

dictionary ScopedCredentialOptions {
    unsigned long                        timeoutSeconds;
    USVString                            rpId;
    sequence<ScopedCredentialDescriptor> excludeList;
    WebAuthnExtensions                   extensions;
};

[SecureContext]
interface WebAuthnAssertion {
    readonly attribute ScopedCredential credential;
    readonly attribute ArrayBuffer      clientData;
    readonly attribute ArrayBuffer      authenticatorData;
    readonly attribute ArrayBuffer      signature;
};

dictionary AssertionOptions {
    unsigned long                        timeoutSeconds;
    USVString                            rpId;
    sequence<ScopedCredentialDescriptor> allowList;
    WebAuthnExtensions                   extensions;
};

dictionary WebAuthnExtensions {
};

[SecureContext]
interface WebAuthnAttestation {
    readonly    attribute USVString     format;
    readonly    attribute ArrayBuffer   clientData;
    readonly    attribute ArrayBuffer   authenticatorData;
    readonly    attribute any           attestation;
};

// Renamed from "ClientData" to avoid a collision with U2F
dictionary WebAuthnClientData {
    required DOMString           challenge;
    required DOMString           origin;
    required WebAuthnAlgorithmID hashAlg; // NOTE: changed from AllgorithmIdentifier because typedef (object or DOMString) not serializable
    DOMString                    tokenBinding;
    WebAuthnExtensions           extensions;
};

enum ScopedCredentialType {
    "ScopedCred"
};

[SecureContext]
interface ScopedCredential {
    readonly attribute ScopedCredentialType type;
    readonly attribute ArrayBuffer          id;
};

dictionary ScopedCredentialDescriptor {
    required ScopedCredentialType type;
    required BufferSource         id;
    sequence <WebAuthnTransport>  transports;
};

// Renamed from "Transport" to avoid a collision with U2F
enum WebAuthnTransport {
    "usb",
    "nfc",
    "ble"
};

/***** The Main API *****/

[SecureContext]
interface WebAuthentication {
    Promise<ScopedCredentialInfo> makeCredential (
        Account                                 accountInformation,
        sequence<ScopedCredentialParameters>    cryptoParameters,
        BufferSource                            attestationChallenge,
        optional ScopedCredentialOptions        options
    );

    Promise<WebAuthnAssertion> getAssertion (
        BufferSource               assertionChallenge,
        optional AssertionOptions  options
    );
};
