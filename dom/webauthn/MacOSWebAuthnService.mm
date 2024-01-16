/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <AuthenticationServices/AuthenticationServices.h>

#include "MacOSWebAuthnService.h"

#include "CFTypeRefPtr.h"
#include "WebAuthnAutoFillEntry.h"
#include "WebAuthnEnumStrings.h"
#include "WebAuthnResult.h"
#include "WebAuthnTransportIdentifiers.h"
#include "mozilla/Maybe.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "nsCocoaUtils.h"
#include "nsIWebAuthnPromise.h"
#include "nsThreadUtils.h"

// The documentation for the platform APIs used here can be found at:
// https://developer.apple.com/documentation/authenticationservices/public-private_key_authentication/supporting_passkeys

namespace {
static mozilla::LazyLogModule gMacOSWebAuthnServiceLog("macoswebauthnservice");
}  // namespace

namespace mozilla::dom {
class API_AVAILABLE(macos(13.3)) MacOSWebAuthnService;
}  // namespace mozilla::dom

// The following ASC* declarations are from the private framework
// AuthenticationServicesCore. The full definitions can be found in WebKit's
// source at Source/WebKit/Platform/spi/Cocoa/AuthenticationServicesCoreSPI.h.
// Overriding ASAuthorizationController's _requestContextWithRequests is
// currently the only way to provide the right information to the macOS
// WebAuthn API (namely, the clientDataHash for requests made to physical
// tokens).

NS_ASSUME_NONNULL_BEGIN

@class ASCPublicKeyCredentialDescriptor;
@interface ASCPublicKeyCredentialDescriptor : NSObject <NSSecureCoding>
- (instancetype)initWithCredentialID:(NSData*)credentialID
                          transports:
                              (nullable NSArray<NSString*>*)allowedTransports;
@end

@protocol ASCPublicKeyCredentialCreationOptions
@property(nonatomic, copy) NSData* clientDataHash;
@property(nonatomic, nullable, copy) NSData* challenge;
@property(nonatomic, copy)
    NSArray<ASCPublicKeyCredentialDescriptor*>* excludedCredentials;
@end

@protocol ASCPublicKeyCredentialAssertionOptions <NSCopying>
@property(nonatomic, copy) NSData* clientDataHash;
@end

@protocol ASCCredentialRequestContext
@property(nonatomic, nullable, copy) id<ASCPublicKeyCredentialCreationOptions>
    platformKeyCredentialCreationOptions;
@property(nonatomic, nullable, copy) id<ASCPublicKeyCredentialAssertionOptions>
    platformKeyCredentialAssertionOptions;
@end

@interface ASAuthorizationController (Secrets)
- (id<ASCCredentialRequestContext>)
    _requestContextWithRequests:(NSArray<ASAuthorizationRequest*>*)requests
                          error:(NSError**)outError;
@end

NSArray<NSString*>* TransportsByteToTransportsArray(const uint8_t aTransports)
    API_AVAILABLE(macos(13.3)) {
  NSMutableArray<NSString*>* transportsNS = [[NSMutableArray alloc] init];
  if ((aTransports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_USB) ==
      MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_USB) {
    [transportsNS
        addObject:
            ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportUSB];
  }
  if ((aTransports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_NFC) ==
      MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_NFC) {
    [transportsNS
        addObject:
            ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportNFC];
  }
  if ((aTransports & MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_BLE) ==
      MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_BLE) {
    [transportsNS
        addObject:
            ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportBluetooth];
  }
  // TODO (bug 1859367): the platform doesn't have a definition for "internal"
  // transport. When it does, this code should be updated to handle it.
  return transportsNS;
}

NSArray* CredentialListsToCredentialDescriptorArray(
    const nsTArray<nsTArray<uint8_t>>& aCredentialList,
    const nsTArray<uint8_t>& aCredentialListTransports,
    const Class credentialDescriptorClass) API_AVAILABLE(macos(13.3)) {
  MOZ_ASSERT(aCredentialList.Length() == aCredentialListTransports.Length());
  NSMutableArray* credentials = [[NSMutableArray alloc] init];
  for (size_t i = 0; i < aCredentialList.Length(); i++) {
    const nsTArray<uint8_t>& credentialId = aCredentialList[i];
    const uint8_t& credentialTransports = aCredentialListTransports[i];
    NSData* credentialIdNS = [NSData dataWithBytes:credentialId.Elements()
                                            length:credentialId.Length()];
    NSArray<NSString*>* credentialTransportsNS =
        TransportsByteToTransportsArray(credentialTransports);
    NSObject* credential = [[credentialDescriptorClass alloc]
        initWithCredentialID:credentialIdNS
                  transports:credentialTransportsNS];
    [credentials addObject:credential];
  }
  return credentials;
}

// MacOSAuthorizationController is an ASAuthorizationController that overrides
// _requestContextWithRequests so that the implementation can set some options
// that aren't directly settable using the public API.
API_AVAILABLE(macos(13.3))
@interface MacOSAuthorizationController : ASAuthorizationController
@end

@implementation MacOSAuthorizationController {
  nsTArray<uint8_t> mClientDataHash;
  nsTArray<nsTArray<uint8_t>> mCredentialList;
  nsTArray<uint8_t> mCredentialListTransports;
}

- (void)stashClientDataHash:(nsTArray<uint8_t>&&)clientDataHash
              andCredentialList:(nsTArray<nsTArray<uint8_t>>&&)credentialList
    andCredentialListTransports:(nsTArray<uint8_t>&&)credentialListTransports {
  mClientDataHash = std::move(clientDataHash);
  mCredentialList = std::move(credentialList);
  mCredentialListTransports = std::move(credentialListTransports);
}

- (id<ASCCredentialRequestContext>)
    _requestContextWithRequests:(NSArray<ASAuthorizationRequest*>*)requests
                          error:(NSError**)outError {
  id<ASCCredentialRequestContext> context =
      [super _requestContextWithRequests:requests error:outError];

  id<ASCPublicKeyCredentialCreationOptions> registrationOptions =
      context.platformKeyCredentialCreationOptions;
  if (registrationOptions) {
    registrationOptions.clientDataHash =
        [NSData dataWithBytes:mClientDataHash.Elements()
                       length:mClientDataHash.Length()];
    // Unset challenge so that the implementation uses clientDataHash (the API
    // returns an error otherwise).
    registrationOptions.challenge = nil;
    const Class publicKeyCredentialDescriptorClass =
        NSClassFromString(@"ASCPublicKeyCredentialDescriptor");
    NSArray<ASCPublicKeyCredentialDescriptor*>* excludedCredentials =
        CredentialListsToCredentialDescriptorArray(
            mCredentialList, mCredentialListTransports,
            publicKeyCredentialDescriptorClass);
    if ([excludedCredentials count] > 0) {
      registrationOptions.excludedCredentials = excludedCredentials;
    }
  }

  id<ASCPublicKeyCredentialAssertionOptions> signOptions =
      context.platformKeyCredentialAssertionOptions;
  if (signOptions) {
    signOptions.clientDataHash =
        [NSData dataWithBytes:mClientDataHash.Elements()
                       length:mClientDataHash.Length()];
    context.platformKeyCredentialAssertionOptions =
        [signOptions copyWithZone:nil];
  }

  return context;
}
@end

// MacOSAuthenticatorRequestDelegate is an ASAuthorizationControllerDelegate,
// which can be set as an ASAuthorizationController's delegate to be called
// back when a request for a platform authenticator attestation or assertion
// response completes either with an attestation or assertion
// (didCompleteWithAuthorization) or with an error (didCompleteWithError).
API_AVAILABLE(macos(13.3))
@interface MacOSAuthenticatorRequestDelegate
    : NSObject <ASAuthorizationControllerDelegate>
- (void)setCallback:(mozilla::dom::MacOSWebAuthnService*)callback;
@end

// MacOSAuthenticatorPresentationContextProvider is an
// ASAuthorizationControllerPresentationContextProviding, which can be set as
// an ASAuthorizationController's presentationContextProvider, and provides a
// presentation anchor for the ASAuthorizationController. Basically, this
// provides the API a handle to the window that made the request.
API_AVAILABLE(macos(13.3))
@interface MacOSAuthenticatorPresentationContextProvider
    : NSObject <ASAuthorizationControllerPresentationContextProviding>
@property(nonatomic, strong) NSWindow* window;
@end

namespace mozilla::dom {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
class API_AVAILABLE(macos(13.3)) MacOSWebAuthnService final
    : public nsIWebAuthnService {
 public:
  MacOSWebAuthnService()
      : mTransactionState(Nothing(),
                          "MacOSWebAuthnService::mTransactionState") {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSERVICE

  void FinishMakeCredential(const nsTArray<uint8_t>& aRawAttestationObject,
                            const nsTArray<uint8_t>& aCredentialId,
                            const nsTArray<nsString>& aTransports,
                            const Maybe<nsString>& aAuthenticatorAttachment);

  void FinishGetAssertion(const nsTArray<uint8_t>& aCredentialId,
                          const nsTArray<uint8_t>& aSignature,
                          const nsTArray<uint8_t>& aAuthenticatorData,
                          const nsTArray<uint8_t>& aUserHandle,
                          const Maybe<nsString>& aAuthenticatorAttachment);
  void ReleasePlatformResources();
  void AbortTransaction(nsresult aError);

 private:
  ~MacOSWebAuthnService() = default;

  void PerformRequests(NSArray<ASAuthorizationRequest*>* aRequests,
                       nsTArray<uint8_t>&& aClientDataHash,
                       nsTArray<nsTArray<uint8_t>>&& aCredentialList,
                       nsTArray<uint8_t>&& aCredentialListTransports,
                       uint64_t aBrowsingContextId);

  struct TransactionState {
    uint64_t transactionId;
    uint64_t browsingContextId;
    Maybe<RefPtr<nsIWebAuthnSignArgs>> pendingSignArgs;
    Maybe<RefPtr<nsIWebAuthnSignPromise>> pendingSignPromise;
    Maybe<nsTArray<RefPtr<nsIWebAuthnAutoFillEntry>>> autoFillEntries;
  };

  using TransactionStateMutex = DataMutex<Maybe<TransactionState>>;
  void DoGetAssertion(Maybe<nsTArray<uint8_t>>&& aSelectedCredentialId,
                      const TransactionStateMutex::AutoLock& aGuard);

  TransactionStateMutex mTransactionState;

  // Main thread only:
  ASAuthorizationWebBrowserPublicKeyCredentialManager* mCredentialManager = nil;
  nsCOMPtr<nsIWebAuthnRegisterPromise> mRegisterPromise;
  nsCOMPtr<nsIWebAuthnSignPromise> mSignPromise;
  MacOSAuthorizationController* mAuthorizationController = nil;
  MacOSAuthenticatorRequestDelegate* mRequestDelegate = nil;
  MacOSAuthenticatorPresentationContextProvider* mPresentationContextProvider =
      nil;
};
#pragma clang diagnostic pop

}  // namespace mozilla::dom

nsTArray<uint8_t> NSDataToArray(NSData* data) {
  nsTArray<uint8_t> array(reinterpret_cast<const uint8_t*>(data.bytes),
                          data.length);
  return array;
}

@implementation MacOSAuthenticatorRequestDelegate {
  RefPtr<mozilla::dom::MacOSWebAuthnService> mCallback;
}

- (void)setCallback:(mozilla::dom::MacOSWebAuthnService*)callback {
  mCallback = callback;
}

- (void)authorizationController:(ASAuthorizationController*)controller
    didCompleteWithAuthorization:(ASAuthorization*)authorization {
  if ([authorization.credential
          conformsToProtocol:
              @protocol(ASAuthorizationPublicKeyCredentialRegistration)]) {
    MOZ_LOG(gMacOSWebAuthnServiceLog, mozilla::LogLevel::Debug,
            ("MacOSAuthenticatorRequestDelegate::didCompleteWithAuthorization: "
             "got registration"));
    id<ASAuthorizationPublicKeyCredentialRegistration> credential =
        (id<ASAuthorizationPublicKeyCredentialRegistration>)
            authorization.credential;
    nsTArray<uint8_t> rawAttestationObject(
        NSDataToArray(credential.rawAttestationObject));
    nsTArray<uint8_t> credentialId(NSDataToArray(credential.credentialID));
    nsTArray<nsString> transports;
    mozilla::Maybe<nsString> authenticatorAttachment;
    if ([credential isKindOfClass:
                        [ASAuthorizationPlatformPublicKeyCredentialRegistration
                            class]]) {
      transports.AppendElement(u"internal"_ns);
#if defined(MAC_OS_VERSION_13_5) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_13_5
      if (__builtin_available(macos 13.5, *)) {
        ASAuthorizationPlatformPublicKeyCredentialRegistration*
            platformCredential =
                (ASAuthorizationPlatformPublicKeyCredentialRegistration*)
                    credential;
        switch (platformCredential.attachment) {
          case ASAuthorizationPublicKeyCredentialAttachmentCrossPlatform:
            authenticatorAttachment.emplace(u"cross-platform"_ns);
            break;
          case ASAuthorizationPublicKeyCredentialAttachmentPlatform:
            authenticatorAttachment.emplace(u"platform"_ns);
            break;
          default:
            break;
        }
      }
#endif
    } else {
      authenticatorAttachment.emplace(u"cross-platform"_ns);
    }
    mCallback->FinishMakeCredential(rawAttestationObject, credentialId,
                                    transports, authenticatorAttachment);
  } else if ([authorization.credential
                 conformsToProtocol:
                     @protocol(ASAuthorizationPublicKeyCredentialAssertion)]) {
    MOZ_LOG(gMacOSWebAuthnServiceLog, mozilla::LogLevel::Debug,
            ("MacOSAuthenticatorRequestDelegate::didCompleteWithAuthorization: "
             "got assertion"));
    id<ASAuthorizationPublicKeyCredentialAssertion> credential =
        (id<ASAuthorizationPublicKeyCredentialAssertion>)
            authorization.credential;
    nsTArray<uint8_t> credentialId(NSDataToArray(credential.credentialID));
    nsTArray<uint8_t> signature(NSDataToArray(credential.signature));
    nsTArray<uint8_t> rawAuthenticatorData(
        NSDataToArray(credential.rawAuthenticatorData));
    nsTArray<uint8_t> userHandle(NSDataToArray(credential.userID));
    mozilla::Maybe<nsString> authenticatorAttachment;
    if ([credential
            isKindOfClass:[ASAuthorizationPlatformPublicKeyCredentialAssertion
                              class]]) {
#if defined(MAC_OS_VERSION_13_5) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_13_5
      if (__builtin_available(macos 13.5, *)) {
        ASAuthorizationPlatformPublicKeyCredentialAssertion*
            platformCredential =
                (ASAuthorizationPlatformPublicKeyCredentialAssertion*)
                    credential;
        switch (platformCredential.attachment) {
          case ASAuthorizationPublicKeyCredentialAttachmentCrossPlatform:
            authenticatorAttachment.emplace(u"cross-platform"_ns);
            break;
          case ASAuthorizationPublicKeyCredentialAttachmentPlatform:
            authenticatorAttachment.emplace(u"platform"_ns);
            break;
          default:
            break;
        }
      }
#endif
    } else {
      authenticatorAttachment.emplace(u"cross-platform"_ns);
    }
    mCallback->FinishGetAssertion(credentialId, signature, rawAuthenticatorData,
                                  userHandle, authenticatorAttachment);
  } else {
    MOZ_LOG(
        gMacOSWebAuthnServiceLog, mozilla::LogLevel::Error,
        ("MacOSAuthenticatorRequestDelegate::didCompleteWithAuthorization: "
         "authorization.credential is neither registration nor assertion!"));
    MOZ_ASSERT_UNREACHABLE(
        "should have ASAuthorizationPublicKeyCredentialRegistration or "
        "ASAuthorizationPublicKeyCredentialAssertion");
  }
  mCallback->ReleasePlatformResources();
  mCallback = nullptr;
}

- (void)authorizationController:(ASAuthorizationController*)controller
           didCompleteWithError:(NSError*)error {
  nsAutoString errorDescription;
  nsCocoaUtils::GetStringForNSString(error.localizedDescription,
                                     errorDescription);
  MOZ_LOG(gMacOSWebAuthnServiceLog, mozilla::LogLevel::Warning,
          ("MacOSAuthenticatorRequestDelegate::didCompleteWithError: %ld (%s)",
           error.code, NS_ConvertUTF16toUTF8(errorDescription).get()));
  nsresult rv;
  switch (error.code) {
    case ASAuthorizationErrorCanceled:
      rv = NS_ERROR_DOM_ABORT_ERR;
      break;
    case ASAuthorizationErrorFailed:
      // The message is right, but it's not about indexeddb.
      // See https://webidl.spec.whatwg.org/#constrainterror
      rv = NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
      break;
    case ASAuthorizationErrorUnknown:
      rv = NS_ERROR_DOM_UNKNOWN_ERR;
      break;
    default:
      rv = NS_ERROR_DOM_NOT_ALLOWED_ERR;
      break;
  }
  mCallback->AbortTransaction(rv);
  mCallback = nullptr;
}
@end

@implementation MacOSAuthenticatorPresentationContextProvider
@synthesize window = window;

- (ASPresentationAnchor)presentationAnchorForAuthorizationController:
    (ASAuthorizationController*)controller {
  return window;
}
@end

namespace mozilla::dom {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
// Given a browsingContextId, attempts to determine the NSWindow associated
// with that browser.
nsresult BrowsingContextIdToNSWindow(uint64_t browsingContextId,
                                     NSWindow** window) {
  *window = nullptr;
  RefPtr<BrowsingContext> browsingContext(
      BrowsingContext::Get(browsingContextId));
  if (!browsingContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  CanonicalBrowsingContext* canonicalBrowsingContext =
      browsingContext->Canonical();
  if (!canonicalBrowsingContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsCOMPtr<nsIWidget> widget(
      canonicalBrowsingContext->GetParentProcessWidgetContaining());
  if (!widget) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *window = static_cast<NSWindow*>(widget->GetNativeData(NS_NATIVE_WINDOW));
  return NS_OK;
}
#pragma clang diagnostic pop

already_AddRefed<nsIWebAuthnService> NewMacOSWebAuthnServiceIfAvailable() {
  MOZ_ASSERT(XRE_IsParentProcess());
  if (!StaticPrefs::security_webauthn_enable_macos_passkeys()) {
    MOZ_LOG(
        gMacOSWebAuthnServiceLog, LogLevel::Debug,
        ("macOS platform support for webauthn (passkeys) disabled by pref"));
    return nullptr;
  }
  // This code checks for the entitlement
  // 'com.apple.developer.web-browser.public-key-credential', which must be
  // true to be able to use the platform APIs.
  CFTypeRefPtr<SecTaskRef> entitlementTask(
      CFTypeRefPtr<SecTaskRef>::WrapUnderCreateRule(
          SecTaskCreateFromSelf(nullptr)));
  CFTypeRefPtr<CFBooleanRef> entitlementValue(
      CFTypeRefPtr<CFBooleanRef>::WrapUnderCreateRule(
          reinterpret_cast<CFBooleanRef>(SecTaskCopyValueForEntitlement(
              entitlementTask.get(),
              CFSTR("com.apple.developer.web-browser.public-key-credential"),
              nullptr))));
  if (!entitlementValue || !CFBooleanGetValue(entitlementValue.get())) {
    MOZ_LOG(
        gMacOSWebAuthnServiceLog, LogLevel::Warning,
        ("entitlement com.apple.developer.web-browser.public-key-credential "
         "not present: platform passkey APIs will not be available"));
    return nullptr;
  }
  nsCOMPtr<nsIWebAuthnService> service(new MacOSWebAuthnService());
  return service.forget();
}

void MacOSWebAuthnService::AbortTransaction(nsresult aError) {
  MOZ_ASSERT(NS_IsMainThread());
  if (mRegisterPromise) {
    Unused << mRegisterPromise->Reject(aError);
    mRegisterPromise = nullptr;
  }
  if (mSignPromise) {
    Unused << mSignPromise->Reject(aError);
    mSignPromise = nullptr;
  }
  ReleasePlatformResources();
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
NS_IMPL_ISUPPORTS(MacOSWebAuthnService, nsIWebAuthnService)
#pragma clang diagnostic pop

NS_IMETHODIMP
MacOSWebAuthnService::MakeCredential(uint64_t aTransactionId,
                                     uint64_t aBrowsingContextId,
                                     nsIWebAuthnRegisterArgs* aArgs,
                                     nsIWebAuthnRegisterPromise* aPromise) {
  Reset();
  auto guard = mTransactionState.Lock();
  *guard = Some(TransactionState{
      aTransactionId,
      aBrowsingContextId,
      Nothing(),
      Nothing(),
      Nothing(),
  });
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MacOSWebAuthnService::MakeCredential",
      [self = RefPtr{this}, browsingContextId(aBrowsingContextId),
       aArgs = nsCOMPtr{aArgs}, aPromise = nsCOMPtr{aPromise}]() {
        self->mRegisterPromise = aPromise;

        nsAutoString rpId;
        Unused << aArgs->GetRpId(rpId);
        NSString* rpIdNS = nsCocoaUtils::ToNSString(rpId);

        nsTArray<uint8_t> challenge;
        Unused << aArgs->GetChallenge(challenge);
        NSData* challengeNS = [NSData dataWithBytes:challenge.Elements()
                                             length:challenge.Length()];

        nsTArray<uint8_t> userId;
        Unused << aArgs->GetUserId(userId);
        NSData* userIdNS = [NSData dataWithBytes:userId.Elements()
                                          length:userId.Length()];

        nsAutoString userName;
        Unused << aArgs->GetUserName(userName);
        NSString* userNameNS = nsCocoaUtils::ToNSString(userName);

        nsAutoString userDisplayName;
        Unused << aArgs->GetUserName(userDisplayName);
        NSString* userDisplayNameNS = nsCocoaUtils::ToNSString(userDisplayName);

        nsTArray<int32_t> coseAlgs;
        Unused << aArgs->GetCoseAlgs(coseAlgs);
        NSMutableArray* credentialParameters = [[NSMutableArray alloc] init];
        for (const auto& coseAlg : coseAlgs) {
          ASAuthorizationPublicKeyCredentialParameters* credentialParameter =
              [[ASAuthorizationPublicKeyCredentialParameters alloc]
                  initWithAlgorithm:coseAlg];
          [credentialParameters addObject:credentialParameter];
        }

        nsTArray<nsTArray<uint8_t>> excludeList;
        Unused << aArgs->GetExcludeList(excludeList);
        nsTArray<uint8_t> excludeListTransports;
        Unused << aArgs->GetExcludeListTransports(excludeListTransports);
        if (excludeList.Length() != excludeListTransports.Length()) {
          self->mRegisterPromise->Reject(NS_ERROR_INVALID_ARG);
          return;
        }

        Maybe<ASAuthorizationPublicKeyCredentialUserVerificationPreference>
            userVerificationPreference = Nothing();
        nsAutoString userVerification;
        Unused << aArgs->GetUserVerification(userVerification);
        if (userVerification.EqualsLiteral(
                MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED)) {
          userVerificationPreference.emplace(
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired);
        } else if (userVerification.EqualsLiteral(
                       MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED)) {
          userVerificationPreference.emplace(
              ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred);
        } else if (
            userVerification.EqualsLiteral(
                MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED)) {
          userVerificationPreference.emplace(
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged);
        }

        // The API doesn't support attestation for platform passkeys and shows
        // no consent UI for non-none attestation for cross-platform devices,
        // so this must always be none.
        ASAuthorizationPublicKeyCredentialAttestationKind
            attestationPreference =
                ASAuthorizationPublicKeyCredentialAttestationKindNone;

        // Initialize the platform provider with the rpId.
        ASAuthorizationPlatformPublicKeyCredentialProvider* platformProvider =
            [[ASAuthorizationPlatformPublicKeyCredentialProvider alloc]
                initWithRelyingPartyIdentifier:rpIdNS];
        // Make a credential registration request with the challenge, userName,
        // and userId.
        ASAuthorizationPlatformPublicKeyCredentialRegistrationRequest*
            platformRegistrationRequest = [platformProvider
                createCredentialRegistrationRequestWithChallenge:challengeNS
                                                            name:userNameNS
                                                          userID:userIdNS];
        [platformProvider release];
        platformRegistrationRequest.attestationPreference =
            attestationPreference;
        if (userVerificationPreference.isSome()) {
          platformRegistrationRequest.userVerificationPreference =
              *userVerificationPreference;
        }

        // Initialize the cross-platform provider with the rpId.
        ASAuthorizationSecurityKeyPublicKeyCredentialProvider*
            crossPlatformProvider =
                [[ASAuthorizationSecurityKeyPublicKeyCredentialProvider alloc]
                    initWithRelyingPartyIdentifier:rpIdNS];
        // Make a credential registration request with the challenge,
        // userDisplayName, userName, and userId.
        ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest*
            crossPlatformRegistrationRequest = [crossPlatformProvider
                createCredentialRegistrationRequestWithChallenge:challengeNS
                                                     displayName:
                                                         userDisplayNameNS
                                                            name:userNameNS
                                                          userID:userIdNS];
        [crossPlatformProvider release];
        crossPlatformRegistrationRequest.attestationPreference =
            attestationPreference;
        crossPlatformRegistrationRequest.credentialParameters =
            credentialParameters;
        if (userVerificationPreference.isSome()) {
          crossPlatformRegistrationRequest.userVerificationPreference =
              *userVerificationPreference;
        }
        nsTArray<uint8_t> clientDataHash;
        nsresult rv = aArgs->GetClientDataHash(clientDataHash);
        if (NS_FAILED(rv)) {
          self->mRegisterPromise->Reject(rv);
          return;
        }
        nsAutoString authenticatorAttachment;
        rv = aArgs->GetAuthenticatorAttachment(authenticatorAttachment);
        if (NS_FAILED(rv) && rv != NS_ERROR_NOT_AVAILABLE) {
          self->mRegisterPromise->Reject(rv);
          return;
        }
        NSMutableArray* requests = [[NSMutableArray alloc] init];
        if (authenticatorAttachment.EqualsLiteral(
                MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_PLATFORM)) {
          [requests addObject:platformRegistrationRequest];
        } else if (authenticatorAttachment.EqualsLiteral(
                       MOZ_WEBAUTHN_AUTHENTICATOR_ATTACHMENT_CROSS_PLATFORM)) {
          [requests addObject:crossPlatformRegistrationRequest];
        } else {
          // Regarding the value of authenticator attachment, according to the
          // specification, "client platforms MUST ignore unknown values,
          // treating an unknown value as if the member does not exist".
          [requests addObject:platformRegistrationRequest];
          [requests addObject:crossPlatformRegistrationRequest];
        }
        self->PerformRequests(
            requests, std::move(clientDataHash), std::move(excludeList),
            std::move(excludeListTransports), browsingContextId);
      }));
  return NS_OK;
}

void MacOSWebAuthnService::PerformRequests(
    NSArray<ASAuthorizationRequest*>* aRequests,
    nsTArray<uint8_t>&& aClientDataHash,
    nsTArray<nsTArray<uint8_t>>&& aCredentialList,
    nsTArray<uint8_t>&& aCredentialListTransports,
    uint64_t aBrowsingContextId) {
  MOZ_ASSERT(NS_IsMainThread());
  // Create a MacOSAuthorizationController and initialize it with the requests.
  MOZ_ASSERT(!mAuthorizationController);
  mAuthorizationController = [[MacOSAuthorizationController alloc]
      initWithAuthorizationRequests:aRequests];
  [mAuthorizationController
              stashClientDataHash:std::move(aClientDataHash)
                andCredentialList:std::move(aCredentialList)
      andCredentialListTransports:std::move(aCredentialListTransports)];

  // Set up the delegate to run when the operation completes.
  MOZ_ASSERT(!mRequestDelegate);
  mRequestDelegate = [[MacOSAuthenticatorRequestDelegate alloc] init];
  [mRequestDelegate setCallback:this];
  mAuthorizationController.delegate = mRequestDelegate;

  // Create a presentation context provider so the API knows which window
  // made the request.
  NSWindow* window = nullptr;
  nsresult rv = BrowsingContextIdToNSWindow(aBrowsingContextId, &window);
  if (NS_FAILED(rv)) {
    AbortTransaction(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  MOZ_ASSERT(!mPresentationContextProvider);
  mPresentationContextProvider =
      [[MacOSAuthenticatorPresentationContextProvider alloc] init];
  mPresentationContextProvider.window = window;
  mAuthorizationController.presentationContextProvider =
      mPresentationContextProvider;

  // Finally, perform the request.
  [mAuthorizationController performRequests];
}

void MacOSWebAuthnService::FinishMakeCredential(
    const nsTArray<uint8_t>& aRawAttestationObject,
    const nsTArray<uint8_t>& aCredentialId,
    const nsTArray<nsString>& aTransports,
    const Maybe<nsString>& aAuthenticatorAttachment) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mRegisterPromise) {
    return;
  }

  RefPtr<WebAuthnRegisterResult> result(new WebAuthnRegisterResult(
      aRawAttestationObject, Nothing(), aCredentialId, aTransports,
      aAuthenticatorAttachment));
  Unused << mRegisterPromise->Resolve(result);
  mRegisterPromise = nullptr;
}

NS_IMETHODIMP
MacOSWebAuthnService::GetAssertion(uint64_t aTransactionId,
                                   uint64_t aBrowsingContextId,
                                   nsIWebAuthnSignArgs* aArgs,
                                   nsIWebAuthnSignPromise* aPromise) {
  Reset();

  auto guard = mTransactionState.Lock();
  *guard = Some(TransactionState{
      aTransactionId,
      aBrowsingContextId,
      Some(RefPtr{aArgs}),
      Some(RefPtr{aPromise}),
      Nothing(),
  });

  bool conditionallyMediated;
  Unused << aArgs->GetConditionallyMediated(&conditionallyMediated);
  if (!conditionallyMediated) {
    DoGetAssertion(Nothing(), guard);
    return NS_OK;
  }

  // Using conditional mediation, so dispatch a task to collect any available
  // passkeys.
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "platformCredentialsForRelyingParty",
      [self = RefPtr{this}, aTransactionId, aArgs = nsCOMPtr{aArgs}]() {
        // This handler is called when platformCredentialsForRelyingParty
        // completes.
        auto credentialsCompletionHandler = ^(
            NSArray<ASAuthorizationWebBrowserPlatformPublicKeyCredential*>*
                credentials) {
          nsTArray<RefPtr<nsIWebAuthnAutoFillEntry>> autoFillEntries;
          for (NSUInteger i = 0; i < credentials.count; i++) {
            const auto& credential = credentials[i];
            nsAutoString userName;
            nsCocoaUtils::GetStringForNSString(credential.name, userName);
            nsAutoString rpId;
            nsCocoaUtils::GetStringForNSString(credential.relyingParty, rpId);
            autoFillEntries.AppendElement(new WebAuthnAutoFillEntry(
                nsIWebAuthnAutoFillEntry::PROVIDER_PLATFORM_MACOS, userName,
                rpId, NSDataToArray(credential.credentialID)));
          }
          auto guard = self->mTransactionState.Lock();
          if (guard->isSome() && guard->ref().transactionId == aTransactionId) {
            guard->ref().autoFillEntries.emplace(std::move(autoFillEntries));
          }
        };
        // This handler is called when
        // requestAuthorizationForPublicKeyCredentials completes.
        auto authorizationHandler = ^(
            ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationState
                authorizationState) {
          // If authorized, list any available passkeys.
          if (authorizationState ==
              ASAuthorizationWebBrowserPublicKeyCredentialManagerAuthorizationStateAuthorized) {
            nsAutoString rpId;
            Unused << aArgs->GetRpId(rpId);
            [self->mCredentialManager
                platformCredentialsForRelyingParty:nsCocoaUtils::ToNSString(
                                                       rpId)
                                 completionHandler:
                                     credentialsCompletionHandler];
          }
        };
        if (!self->mCredentialManager) {
          self->mCredentialManager =
              [[ASAuthorizationWebBrowserPublicKeyCredentialManager alloc]
                  init];
        }
        // Request authorization to examine any available passkeys. This will
        // cause a permission prompt to appear once.
        [self->mCredentialManager
            requestAuthorizationForPublicKeyCredentials:authorizationHandler];
      }));

  return NS_OK;
}

void MacOSWebAuthnService::DoGetAssertion(
    Maybe<nsTArray<uint8_t>>&& aSelectedCredentialId,
    const TransactionStateMutex::AutoLock& aGuard) {
  if (aGuard->isNothing() || aGuard->ref().pendingSignArgs.isNothing() ||
      aGuard->ref().pendingSignPromise.isNothing()) {
    return;
  }

  // Take the pending Args and Promise to prevent repeated calls to
  // DoGetAssertion for this transaction.
  RefPtr<nsIWebAuthnSignArgs> aArgs = aGuard->ref().pendingSignArgs.extract();
  RefPtr<nsIWebAuthnSignPromise> aPromise =
      aGuard->ref().pendingSignPromise.extract();
  uint64_t aBrowsingContextId = aGuard->ref().browsingContextId;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MacOSWebAuthnService::MakeCredential",
      [self = RefPtr{this}, browsingContextId(aBrowsingContextId), aArgs,
       aPromise,
       aSelectedCredentialId = std::move(aSelectedCredentialId)]() mutable {
        self->mSignPromise = aPromise;

        nsAutoString rpId;
        Unused << aArgs->GetRpId(rpId);
        NSString* rpIdNS = nsCocoaUtils::ToNSString(rpId);

        nsTArray<uint8_t> challenge;
        Unused << aArgs->GetChallenge(challenge);
        NSData* challengeNS = [NSData dataWithBytes:challenge.Elements()
                                             length:challenge.Length()];

        nsTArray<nsTArray<uint8_t>> allowList;
        nsTArray<uint8_t> allowListTransports;
        if (aSelectedCredentialId.isSome()) {
          allowList.AppendElement(aSelectedCredentialId.extract());
          allowListTransports.AppendElement(
              MOZ_WEBAUTHN_AUTHENTICATOR_TRANSPORT_ID_INTERNAL);
        } else {
          Unused << aArgs->GetAllowList(allowList);
          Unused << aArgs->GetAllowListTransports(allowListTransports);
        }
        NSMutableArray* platformAllowedCredentials =
            [[NSMutableArray alloc] init];
        for (const auto& allowedCredentialId : allowList) {
          NSData* allowedCredentialIdNS =
              [NSData dataWithBytes:allowedCredentialId.Elements()
                             length:allowedCredentialId.Length()];
          ASAuthorizationPlatformPublicKeyCredentialDescriptor*
              allowedCredential =
                  [[ASAuthorizationPlatformPublicKeyCredentialDescriptor alloc]
                      initWithCredentialID:allowedCredentialIdNS];
          [platformAllowedCredentials addObject:allowedCredential];
        }
        const Class securityKeyPublicKeyCredentialDescriptorClass =
            NSClassFromString(
                @"ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor");
        NSArray<ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor*>*
            crossPlatformAllowedCredentials =
                CredentialListsToCredentialDescriptorArray(
                    allowList, allowListTransports,
                    securityKeyPublicKeyCredentialDescriptorClass);

        Maybe<ASAuthorizationPublicKeyCredentialUserVerificationPreference>
            userVerificationPreference = Nothing();
        nsAutoString userVerification;
        Unused << aArgs->GetUserVerification(userVerification);
        if (userVerification.EqualsLiteral(
                MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_REQUIRED)) {
          userVerificationPreference.emplace(
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired);
        } else if (userVerification.EqualsLiteral(
                       MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_PREFERRED)) {
          userVerificationPreference.emplace(
              ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred);
        } else if (
            userVerification.EqualsLiteral(
                MOZ_WEBAUTHN_USER_VERIFICATION_REQUIREMENT_DISCOURAGED)) {
          userVerificationPreference.emplace(
              ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged);
        }

        // Initialize the platform provider with the rpId.
        ASAuthorizationPlatformPublicKeyCredentialProvider* platformProvider =
            [[ASAuthorizationPlatformPublicKeyCredentialProvider alloc]
                initWithRelyingPartyIdentifier:rpIdNS];
        // Make a credential assertion request with the challenge.
        ASAuthorizationPlatformPublicKeyCredentialAssertionRequest*
            platformAssertionRequest = [platformProvider
                createCredentialAssertionRequestWithChallenge:challengeNS];
        [platformProvider release];
        platformAssertionRequest.allowedCredentials =
            platformAllowedCredentials;
        if (userVerificationPreference.isSome()) {
          platformAssertionRequest.userVerificationPreference =
              *userVerificationPreference;
        }

        // Initialize the cross-platform provider with the rpId.
        ASAuthorizationSecurityKeyPublicKeyCredentialProvider*
            crossPlatformProvider =
                [[ASAuthorizationSecurityKeyPublicKeyCredentialProvider alloc]
                    initWithRelyingPartyIdentifier:rpIdNS];
        // Make a credential assertion request with the challenge.
        ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest*
            crossPlatformAssertionRequest = [crossPlatformProvider
                createCredentialAssertionRequestWithChallenge:challengeNS];
        [crossPlatformProvider release];
        crossPlatformAssertionRequest.allowedCredentials =
            crossPlatformAllowedCredentials;
        if (userVerificationPreference.isSome()) {
          crossPlatformAssertionRequest.userVerificationPreference =
              *userVerificationPreference;
        }
        nsTArray<uint8_t> clientDataHash;
        nsresult rv = aArgs->GetClientDataHash(clientDataHash);
        if (NS_FAILED(rv)) {
          self->mSignPromise->Reject(rv);
          return;
        }
        nsTArray<nsTArray<uint8_t>> unusedCredentialIds;
        nsTArray<uint8_t> unusedTransports;
        // allowList and allowListTransports won't actually get used.
        self->PerformRequests(
            @[ platformAssertionRequest, crossPlatformAssertionRequest ],
            std::move(clientDataHash), std::move(allowList),
            std::move(allowListTransports), browsingContextId);
      }));
}

void MacOSWebAuthnService::FinishGetAssertion(
    const nsTArray<uint8_t>& aCredentialId, const nsTArray<uint8_t>& aSignature,
    const nsTArray<uint8_t>& aAuthenticatorData,
    const nsTArray<uint8_t>& aUserHandle,
    const Maybe<nsString>& aAuthenticatorAttachment) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mSignPromise) {
    return;
  }

  RefPtr<WebAuthnSignResult> result(new WebAuthnSignResult(
      aAuthenticatorData, Nothing(), aCredentialId, aSignature, aUserHandle,
      aAuthenticatorAttachment));
  Unused << mSignPromise->Resolve(result);
  mSignPromise = nullptr;
}

void MacOSWebAuthnService::ReleasePlatformResources() {
  MOZ_ASSERT(NS_IsMainThread());
  [mCredentialManager release];
  mCredentialManager = nil;
  [mAuthorizationController release];
  mAuthorizationController = nil;
  [mRequestDelegate release];
  mRequestDelegate = nil;
  [mPresentationContextProvider release];
  mPresentationContextProvider = nil;
}

NS_IMETHODIMP
MacOSWebAuthnService::Reset() {
  auto guard = mTransactionState.Lock();
  if (guard->isSome()) {
    if (guard->ref().pendingSignPromise.isSome()) {
      // This request was never dispatched to the platform API, so
      // we need to reject the promise ourselves.
      guard->ref().pendingSignPromise.ref()->Reject(
          NS_ERROR_DOM_NOT_ALLOWED_ERR);
    }
    guard->reset();
  }
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "MacOSWebAuthnService::Cancel", [self = RefPtr{this}] {
        // cancel results in the delegate's didCompleteWithError method being
        // called, which will release all platform resources.
        [self->mAuthorizationController cancel];
      }));
  return NS_OK;
}

NS_IMETHODIMP
MacOSWebAuthnService::GetIsUVPAA(bool* aAvailable) {
  *aAvailable = true;
  return NS_OK;
}

NS_IMETHODIMP
MacOSWebAuthnService::Cancel(uint64_t aTransactionId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::HasPendingConditionalGet(uint64_t aBrowsingContextId,
                                               const nsAString& aOrigin,
                                               uint64_t* aRv) {
  auto guard = mTransactionState.Lock();
  if (guard->isNothing() ||
      guard->ref().browsingContextId != aBrowsingContextId ||
      guard->ref().pendingSignArgs.isNothing()) {
    *aRv = 0;
    return NS_OK;
  }

  nsString origin;
  Unused << guard->ref().pendingSignArgs.ref()->GetOrigin(origin);
  if (origin != aOrigin) {
    *aRv = 0;
    return NS_OK;
  }

  *aRv = guard->ref().transactionId;
  return NS_OK;
}

NS_IMETHODIMP
MacOSWebAuthnService::GetAutoFillEntries(
    uint64_t aTransactionId, nsTArray<RefPtr<nsIWebAuthnAutoFillEntry>>& aRv) {
  auto guard = mTransactionState.Lock();
  if (guard->isNothing() || guard->ref().transactionId != aTransactionId ||
      guard->ref().pendingSignArgs.isNothing() ||
      guard->ref().autoFillEntries.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aRv.Assign(guard->ref().autoFillEntries.ref());
  return NS_OK;
}

NS_IMETHODIMP
MacOSWebAuthnService::SelectAutoFillEntry(
    uint64_t aTransactionId, const nsTArray<uint8_t>& aCredentialId) {
  auto guard = mTransactionState.Lock();
  if (guard->isNothing() || guard->ref().transactionId != aTransactionId ||
      guard->ref().pendingSignArgs.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsTArray<uint8_t>> allowList;
  Unused << guard->ref().pendingSignArgs.ref()->GetAllowList(allowList);
  if (!allowList.IsEmpty() && !allowList.Contains(aCredentialId)) {
    return NS_ERROR_FAILURE;
  }

  Maybe<nsTArray<uint8_t>> id;
  id.emplace();
  id.ref().Assign(aCredentialId);
  DoGetAssertion(std::move(id), guard);

  return NS_OK;
}

NS_IMETHODIMP
MacOSWebAuthnService::ResumeConditionalGet(uint64_t aTransactionId) {
  auto guard = mTransactionState.Lock();
  if (guard->isNothing() || guard->ref().transactionId != aTransactionId ||
      guard->ref().pendingSignArgs.isNothing()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  DoGetAssertion(Nothing(), guard);
  return NS_OK;
}

NS_IMETHODIMP
MacOSWebAuthnService::PinCallback(uint64_t aTransactionId,
                                  const nsACString& aPin) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::ResumeMakeCredential(uint64_t aTransactionId,
                                           bool aForceNoneAttestation) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::SelectionCallback(uint64_t aTransactionId,
                                        uint64_t aIndex) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::AddVirtualAuthenticator(
    const nsACString& protocol, const nsACString& transport,
    bool hasResidentKey, bool hasUserVerification, bool isUserConsenting,
    bool isUserVerified, uint64_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::RemoveVirtualAuthenticator(uint64_t authenticatorId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::AddCredential(uint64_t authenticatorId,
                                    const nsACString& credentialId,
                                    bool isResidentCredential,
                                    const nsACString& rpId,
                                    const nsACString& privateKey,
                                    const nsACString& userHandle,
                                    uint32_t signCount) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::GetCredentials(
    uint64_t authenticatorId,
    nsTArray<RefPtr<nsICredentialParameters>>& _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::RemoveCredential(uint64_t authenticatorId,
                                       const nsACString& credentialId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::RemoveAllCredentials(uint64_t authenticatorId) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::SetUserVerified(uint64_t authenticatorId,
                                      bool isUserVerified) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MacOSWebAuthnService::Listen() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
MacOSWebAuthnService::RunCommand(const nsACString& cmd) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::dom

NS_ASSUME_NONNULL_END
