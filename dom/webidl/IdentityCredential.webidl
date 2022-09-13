/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=Window, SecureContext,
 Pref="dom.security.credentialmanagement.identity.enabled"]
interface IdentityCredential : Credential {
 readonly attribute USVString? token;
};

dictionary IdentityCredentialRequestOptions {
 sequence<IdentityProvider> providers;
};

dictionary IdentityProvider {
 required USVString configURL;
 required USVString clientId;
 USVString nonce;
};