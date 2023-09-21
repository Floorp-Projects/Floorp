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
    [SameObject, Throws] readonly attribute ArrayBuffer      rawId;
    [SameObject] readonly attribute AuthenticatorResponse    response;
    AuthenticationExtensionsClientOutputs getClientExtensionResults();
    [NewObject] static Promise<boolean> isConditionalMediationAvailable();
    [Throws] object toJSON();
};

typedef DOMString Base64URLString;

[GenerateConversionToJS]
dictionary RegistrationResponseJSON {
    required Base64URLString id;
    required Base64URLString rawId;
    required AuthenticatorAttestationResponseJSON response;
    DOMString authenticatorAttachment;
    required AuthenticationExtensionsClientOutputsJSON clientExtensionResults;
    required DOMString type;
};

[GenerateConversionToJS]
dictionary AuthenticatorAttestationResponseJSON {
    required Base64URLString clientDataJSON;
    required Base64URLString authenticatorData;
    required sequence<DOMString> transports;
    // The publicKey field will be missing if pubKeyCredParams was used to
    // negotiate a public-key algorithm that the user agent doesn’t
    // understand. (See section “Easily accessing credential data” for a
    // list of which algorithms user agents must support.) If using such an
    // algorithm then the public key must be parsed directly from
    // attestationObject or authenticatorData.
    Base64URLString publicKey;
    required long long publicKeyAlgorithm;
    // This value contains copies of some of the fields above. See
    // section “Easily accessing credential data”.
    required Base64URLString attestationObject;
};

[GenerateConversionToJS]
dictionary AuthenticationResponseJSON {
    required Base64URLString id;
    required Base64URLString rawId;
    required AuthenticatorAssertionResponseJSON response;
    DOMString authenticatorAttachment;
    required AuthenticationExtensionsClientOutputsJSON clientExtensionResults;
    required DOMString type;
};

[GenerateConversionToJS]
dictionary AuthenticatorAssertionResponseJSON {
    required Base64URLString clientDataJSON;
    required Base64URLString authenticatorData;
    required Base64URLString signature;
    Base64URLString userHandle;
    Base64URLString attestationObject;
};

[GenerateConversionToJS]
dictionary AuthenticationExtensionsClientOutputsJSON {
    // FIDO AppID Extension (appid)
    // <https://w3c.github.io/webauthn/#sctn-appid-extension>
    boolean appid;

    // <https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension>
    boolean hmacCreateSecret;
};

[SecureContext]
partial interface PublicKeyCredential {
    [NewObject] static Promise<boolean> isUserVerifyingPlatformAuthenticatorAvailable();
    // isExternalCTAP2SecurityKeySupported is non-standard; see Bug 1526023
    [NewObject] static Promise<boolean> isExternalCTAP2SecurityKeySupported();
};

[SecureContext]
partial interface PublicKeyCredential {
    [Throws] static PublicKeyCredentialCreationOptions parseCreationOptionsFromJSON(PublicKeyCredentialCreationOptionsJSON options);
};

dictionary PublicKeyCredentialCreationOptionsJSON {
    required PublicKeyCredentialRpEntity                    rp;
    required PublicKeyCredentialUserEntityJSON              user;
    required Base64URLString                                challenge;
    required sequence<PublicKeyCredentialParameters>        pubKeyCredParams;
    unsigned long                                           timeout;
    sequence<PublicKeyCredentialDescriptorJSON>             excludeCredentials = [];
    AuthenticatorSelectionCriteria                          authenticatorSelection;
    sequence<DOMString>                                     hints = [];
    DOMString                                               attestation = "none";
    sequence<DOMString>                                     attestationFormats = [];
    AuthenticationExtensionsClientInputsJSON                extensions;
};

dictionary PublicKeyCredentialUserEntityJSON {
    required Base64URLString        id;
    required DOMString              name;
    required DOMString              displayName;
};

dictionary PublicKeyCredentialDescriptorJSON {
    required Base64URLString        id;
    required DOMString              type;
    sequence<DOMString>             transports;
};

dictionary AuthenticationExtensionsClientInputsJSON {
    // FIDO AppID Extension (appid)
    // <https://w3c.github.io/webauthn/#sctn-appid-extension>
    USVString appid;

    // hmac-secret
    // <https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#sctn-hmac-secret-extension>
    boolean hmacCreateSecret;
};

[SecureContext]
partial interface PublicKeyCredential {
    [Throws] static PublicKeyCredentialRequestOptions parseRequestOptionsFromJSON(PublicKeyCredentialRequestOptionsJSON options);
};

dictionary PublicKeyCredentialRequestOptionsJSON {
    required Base64URLString                                challenge;
    unsigned long                                           timeout;
    DOMString                                               rpId;
    sequence<PublicKeyCredentialDescriptorJSON>             allowCredentials = [];
    DOMString                                               userVerification = "preferred";
    sequence<DOMString>                                     hints = [];
    DOMString                                               attestation = "none";
    sequence<DOMString>                                     attestationFormats = [];
    AuthenticationExtensionsClientInputsJSON                extensions;
};

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface AuthenticatorResponse {
    [SameObject, Throws] readonly attribute ArrayBuffer clientDataJSON;
};

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface AuthenticatorAttestationResponse : AuthenticatorResponse {
    [SameObject, Throws] readonly attribute ArrayBuffer attestationObject;
    sequence<DOMString>                                 getTransports();
    [Throws] ArrayBuffer                                getAuthenticatorData();
    [Throws] ArrayBuffer?                               getPublicKey();
    [Throws] COSEAlgorithmIdentifier                    getPublicKeyAlgorithm();
};

[SecureContext, Pref="security.webauth.webauthn",
 Exposed=Window]
interface AuthenticatorAssertionResponse : AuthenticatorResponse {
    [SameObject, Throws] readonly attribute ArrayBuffer      authenticatorData;
    [SameObject, Throws] readonly attribute ArrayBuffer      signature;
    [SameObject, Throws] readonly attribute ArrayBuffer?     userHandle;
};

dictionary PublicKeyCredentialParameters {
    required DOMString                type;
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
    DOMString                                    attestation = "none";
    // FIXME: bug 1493860: should this "= {}" be here?
    AuthenticationExtensionsClientInputs         extensions = {};
};

dictionary PublicKeyCredentialEntity {
    required DOMString    name;
};

dictionary PublicKeyCredentialRpEntity : PublicKeyCredentialEntity {
    DOMString      id;
};

dictionary PublicKeyCredentialUserEntity : PublicKeyCredentialEntity {
    required BufferSource   id;
    required DOMString      displayName;
};

dictionary AuthenticatorSelectionCriteria {
    DOMString                    authenticatorAttachment;
    DOMString                    residentKey;
    boolean                      requireResidentKey = false;
    DOMString                    userVerification = "preferred";
};

dictionary PublicKeyCredentialRequestOptions {
    required BufferSource                challenge;
    unsigned long                        timeout;
    USVString                            rpId;
    sequence<PublicKeyCredentialDescriptor> allowCredentials = [];
    DOMString                            userVerification = "preferred";
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

dictionary PublicKeyCredentialDescriptor {
    required DOMString                    type;
    required BufferSource                 id;
    // Transports is a string that is matched against the AuthenticatorTransport
    // enumeration so that we have forward-compatibility for new transports.
    sequence<DOMString>                   transports;
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
