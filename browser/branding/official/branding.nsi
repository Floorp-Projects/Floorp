# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# NSIS branding defines for official release builds.
# The nightly build branding.nsi is located in browser/installer/windows/nsis/
# The unofficial build branding.nsi is located in browser/branding/unofficial/

# BrandFullNameInternal is used for some registry and file system values
# instead of BrandFullName and typically should not be modified.
!define BrandFullNameInternal "Mozilla Firefox"
!define CompanyName           "Mozilla Corporation"
!define URLInfoAbout          "http://www.mozilla.com/${AB_CD}/"
!define URLUpdateInfo         "http://www.mozilla.com/${AB_CD}/firefox/"

!define URLStubDownload "http://download.mozilla.org/?product=firefox-latest&os=win&lang=${AB_CD}"
!define URLManualDownload "http://download.mozilla.org/?product=firefox-latest&os=win&lang=${AB_CD}"

# The installer's certificate name and issuer expected by the stub installer
!define CertNameDownload   "Mozilla Corporation"
!define CertIssuerDownload "Thawte Code Signing CA - G2"
