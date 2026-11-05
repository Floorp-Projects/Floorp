# macOS privileged helper signing

Floorp's macOS app and privileged helpers must authorize each other's Developer
ID signatures. A valid bundle signature or notarization result alone does not
establish that ServiceManagement will accept the helper.

The production policy binds the app's bundle identifier and each helper's full
identifier to Floorp team `DV2U35YBHT`, the Apple anchor, and the Developer ID
certificate extensions. `org.mozilla.updater` and `org.mozilla.dmgInstallHelper`
are executable identifiers, not filenames with an extension to strip. The
standalone LaunchServices copies receive explicit scoped rcodesign identifiers.
The nested updater bundle keeps its own bundle identifier.

The client requirement is embedded into each native helper's `__TEXT,__info_plist`
at link time. The Runtime source patch must therefore be included in a new native
build before changing `floorp-runtime.lock.json`. Applying that patch during an
artifact-only package build or re-signing an old helper does not update the
embedded requirement. Runtime counterpart:
https://github.com/Floorp-Projects/Floorp-Runtime/pull/101

The native signing job runs `tools/validate_macos_helper_trust.py` before creating
the signed stage artifact. It validates both updater copies and a DMG helper when
present, all app architectures, the expected `app.floorp.Floorp` bundle ID for both
release and Daylight, exact production policies, and the two directions
of authorization using Apple's `codesign` and `csreq`. Missing or mismatched data
fails the package. An old Runtime with Mozilla-team requirements is intentionally
rejected; do not disable this check to package it.

The unit tests cover the signing command and validator failure conditions. The
official 12.16.2, 12.16.4, and 12.17.0 binaries fail the mutual requirement checks;
a VM also records an smd helper-check failure after actual Install Helper
authorization. This does not establish the cause of the reporter's errorCode 90.
Qualifying the fix still requires a newly built and Developer ID-signed app,
successful validator results, and a GUI update that actually uses the privileged
helper. Ordinary writable-copy updates and mocked signature checks are not that
qualification. No existing Runtime lock or release is changed by this patch.
