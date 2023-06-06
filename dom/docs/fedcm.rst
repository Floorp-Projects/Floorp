===============================
Federated Credential Management
===============================

FedCM, as it is abbreviated, is a platform feature that requires a full-stack implementation.
As such, its code is scattered throughout the codebase and it can be hard to follow the flow of execution.
This documentation aims to make those two points easier.

Code sites
==========

Code relevant to it can be found in all of the following places.

The webidl for this spec lives in ``dom/webidl/IdentityCredential.webidl``

Core spec algorithm logic and the implementation of the ``IdentityCredential`` live in ``dom/credentialmanagement/identity/IdentityCredential.{cpp,h}``. The static functions of ``IdentityCredential`` are the spec algorithm logic. Helpers for managing the ``IdentityCredential.webidl`` objects are in the other files in ``dom/credentialmanagement/identity/``. The IPC is defined on the WindowGlobal in ``dom/ipc/PWindowGlobal.ipdl`` and ``dom/ipc/WindowGlobalParent.cpp``, and is a very thin layer.

The service for managing state associated with IdentityCredentials is ``IdentityCredentialStorageService`` and the service for managing the UI prompts associated with IdentityCredentials is ``IdentityCredentialPromptService``. Both definitions and implementations are in ``toolkit/components/credentialmanagement``.

The UI panel is spread around a little. The actual DOM elements are in the HTML subtree with root at ``#identity-credential-notification`` in ``browser/base/content/popup-notifications.inc``. But the CSS describing it is spread through ``browser/themes/shared/customizableui/panelUI-shared.css``, ``browser/themes/shared/identity-credential-notification.css``, and ``browser/themes/shared/notification-icons.css``. Generally speaking, search for ``identity-credential`` in those files to find the relevant ids and classes.

Content strings: ``browser/locales/en-US/browser/identityCredentialNotification.ftl``.

All of this is entered from the ``navigator.credentials`` object, implemented in ``dom/credentialmanagement/CredentialsContainer.{cpp,h}``.

Flow of Execution
=================

This is the general flow through code relevant to the core spec algorithms, which happens to be the complicated parts imo.

A few notes:

- All functions without a class specified are in ``IdentityCredential``.
- Functions in ``IdentityCredentialPromptService`` mutate the Chrome DOM
- FetchT functions send network requests via ``mozilla::dom::FetchJSONStructure<T>``.
- A call to ``IdentityCredentialStorageService`` is made in ``PromptUserWithPolicy``

.. graphviz::

  digraph fedcm {
    "RP (visited page) calls ``navigator.credentials.get()``" -> "CredentialsContainer::Get"
    "CredentialsContainer::Get" -> "DiscoverFromExternalSource"
    "DiscoverFromExternalSource" -> "DiscoverFromExternalSourceInMainProcess" [label="IPC via WindowGlobal's DiscoverIdentityCredentialFromExternalSource"]
    "DiscoverFromExternalSourceInMainProcess" -> "anonymous timeout callback" -> "CloseUserInterface" -> "IdentityCredentialPromptService::Close"
    "DiscoverFromExternalSourceInMainProcess" -> "CheckRootManifest A"
    "CheckRootManifest A" -> "FetchInternalManifest A" [label="via promise chain in DiscoverFromExternalSourceInMainProcess"]
    "FetchInternalManifest A" -> "DiscoverFromExternalSourceInMainProcess inline anonymous callback (Promise::All)"
    "DiscoverFromExternalSourceInMainProcess" -> "CheckRootManifest N"
    "CheckRootManifest N" -> "FetchInternalManifest N" [label="via promise chain in DiscoverFromExternalSourceInMainProcess"]
    "FetchInternalManifest N" -> "DiscoverFromExternalSourceInMainProcess inline anonymous callback (Promise::All)"
    "DiscoverFromExternalSourceInMainProcess inline anonymous callback (Promise::All)" -> "PromptUserToSelectProvider"
    "PromptUserToSelectProvider" -> "IdentityCredentialPromptService::ShowProviderPrompt"
    "IdentityCredentialPromptService::ShowProviderPrompt" -> "CreateCredential" [label="via promise chain in DiscoverFromExternalSourceInMainProcess"]
    "CreateCredential" -> "FetchAccountList" [label="via promise chain in CreateCredential"]
    "FetchAccountList" -> "PromptUserToSelectAccount" [label="via promise chain in CreateCredential"]
    "PromptUserToSelectAccount" -> "IdentityCredentialPromptService::ShowAccountListPrompt"
    "IdentityCredentialPromptService::ShowAccountListPrompt" -> "PromptUserWithPolicy" [label="via promise chain in CreateCredential"]
    "PromptUserWithPolicy" -> "FetchMetadata"
    "FetchMetadata" -> "IdentityCredentialPromptService::ShowPolicyPrompt" [label="via promise chain in PromptUserWithPolicy"]
    "IdentityCredentialPromptService::ShowPolicyPrompt" -> "FetchToken" [label="via promise chain in CreateCredential"]
    "FetchToken" -> "cancel anonymous timeout callback"
    "FetchToken" -> "CreateCredential inline anonymous callback"
    "CreateCredential inline anonymous callback" -> "DiscoverFromExternalSourceInMainProcess inline anonymous callback"
    "DiscoverFromExternalSourceInMainProcess inline anonymous callback" -> "DiscoverFromExternalSource inline anonymous callback" [label="Resolving IPC via WindowGlobal's DiscoverIdentityCredentialFromExternalSource"]
    "DiscoverFromExternalSource inline anonymous callback" -> "CredentialsContainer::Get inline anonymous callback"
    "CredentialsContainer::Get inline anonymous callback" -> "RP (visited page) gets the credential"
  }
