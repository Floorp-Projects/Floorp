/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://fedidcg.github.io/FedCM
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

// https://fedidcg.github.io/FedCM/#check-the-root-manifest
[GenerateInit]
dictionary IdentityRootManifest {
  required sequence<USVString> provider_urls;
};

// https://fedidcg.github.io/FedCM/#icon-json
dictionary IdentityIcon {
  required USVString url;
  unsigned long size;
};

// https://fedidcg.github.io/FedCM/#branding-json
dictionary IdentityBranding {
  USVString background_color;
  USVString color;
  sequence<IdentityIcon> icons;
};

// https://fedidcg.github.io/FedCM/#manifest
[GenerateInit]
dictionary IdentityInternalManifest {
  required USVString accounts_endpoint;
  required USVString client_metadata_endpoint;
  required USVString id_token_endpoint;
  IdentityBranding branding;
};


// https://fedidcg.github.io/FedCM/#account-json
dictionary IdentityAccount {
  required USVString id;
  required USVString name;
  required USVString email;
  USVString given_name;
  sequence<USVString> approved_clients;
};

// https://fedidcg.github.io/FedCM/#idp-api-accounts-endpoint
[GenerateInit]
dictionary IdentityAccountList {
  sequence<IdentityAccount> accounts;
};

// https://fedidcg.github.io/FedCM/#idp-api-client-id-metadata-endpoint
dictionary IdentityClientMetadata {
  USVString privacy_policy_url;
  USVString terms_of_service_url;
};

// https://fedidcg.github.io/FedCM/#dom-id_token_endpoint_response-token
[GenerateInit]
dictionary IdentityToken {
  required USVString token;
};
