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
 [Throws]
 static Promise<undefined> logoutRPs(sequence<IdentityCredentialLogoutRPsRequest> logoutRequests);
};

dictionary IdentityCredentialRequestOptions {
 sequence<IdentityProviderConfig> providers;
};

[GenerateConversionToJS]
dictionary IdentityProviderConfig {
 required UTF8String configURL;
 required USVString clientId;
 USVString nonce;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityproviderwellknown
[GenerateInit]
dictionary IdentityProviderWellKnown {
  required sequence<UTF8String> provider_urls;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityprovidericon
dictionary IdentityProviderIcon {
  required USVString url;
  unsigned long size;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityproviderbranding
dictionary IdentityProviderBranding {
  USVString background_color;
  USVString color;
  sequence<IdentityProviderIcon> icons;
  USVString name;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityproviderapiconfig
[GenerateInit, GenerateConversionToJS]
dictionary IdentityProviderAPIConfig {
  required UTF8String accounts_endpoint;
  required UTF8String client_metadata_endpoint;
  required UTF8String id_assertion_endpoint;
  IdentityProviderBranding branding;
};


// https://fedidcg.github.io/FedCM/#dictdef-identityprovideraccount
dictionary IdentityProviderAccount {
  required USVString id;
  required USVString name;
  required USVString email;
  USVString given_name;
  USVString picture;
  sequence<USVString> approved_clients;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityprovideraccountlist
[GenerateInit, GenerateConversionToJS]
dictionary IdentityProviderAccountList {
  sequence<IdentityProviderAccount> accounts;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityproviderclientmetadata
[GenerateInit, GenerateConversionToJS]
dictionary IdentityProviderClientMetadata {
  USVString privacy_policy_url;
  USVString terms_of_service_url;
};

// https://fedidcg.github.io/FedCM/#dictdef-identityprovidertoken
[GenerateInit]
dictionary IdentityProviderToken {
  required USVString token;
};

// https://fedidcg.github.io/FedCM/#dictdef-identitycredentiallogoutrpsrequest
dictionary IdentityCredentialLogoutRPsRequest {
  required UTF8String url;
  required UTF8String accountId;
};
