/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://www.w3.org/TR/credential-management-1/
 * and
 * https://w3c.github.io/webauthn/
 * and
 * https://fedidcg.github.io/FedCM/
 */

[Exposed=Window, SecureContext]
interface Credential {
  readonly attribute USVString id;
  readonly attribute DOMString type;
};

[Exposed=Window, SecureContext]
interface CredentialsContainer {
  [NewObject]
  Promise<Credential?> get(optional CredentialRequestOptions options = {});
  [NewObject]
  Promise<Credential?> create(optional CredentialCreationOptions options = {});
  [NewObject]
  Promise<Credential> store(Credential credential);
  [NewObject]
  Promise<void> preventSilentAccess();
};

dictionary CredentialRequestOptions {
  // This is taken from the partial definition in
  // https://w3c.github.io/webauthn/#sctn-credentialrequestoptions-extension
  [Pref="security.webauth.webauthn"]
  PublicKeyCredentialRequestOptions publicKey;
  // This is taken from the partial definition in
  // https://fedidcg.github.io/FedCM/#browser-api-credential-request-options
  [Pref="dom.security.credentialmanagement.identity.enabled"]
  IdentityCredentialRequestOptions identity;
  AbortSignal signal;
};

dictionary CredentialCreationOptions {
  // This is taken from the partial definition in
  // https://w3c.github.io/webauthn/#sctn-credentialcreationoptions-extension
  [Pref="security.webauth.webauthn"]
  PublicKeyCredentialCreationOptions publicKey;
  AbortSignal signal;
};
