# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# NSIS branding defines for Aurora builds.
# The official release build branding.nsi is located in other-license/branding/firefox/
# The unofficial build branding.nsi is located in browser/branding/unofficial/

# BrandFullNameInternal is used for some registry and file system values
# instead of BrandFullName and typically should not be modified.
!define BrandFullNameInternal "Aurora"
!define CompanyName           "mozilla.org"
!define URLInfoAbout          "http://www.mozilla.org"
!define URLUpdateInfo         "http://www.mozilla.org/projects/firefox"

!define URLStubDownload "http://download.mozilla.org/?product=firefox-aurora-latest&os=win&lang=${AB_CD}"
!define URLManualDownload "https://www.mozilla.org/${AB_CD}/firefox/installer-help/?channel=aurora&installer_lang=${AB_CD}"
!define Channel "aurora"

# The installer's certificate name and issuer expected by the stub installer
!define CertNameDownload   "Mozilla Corporation"
!define CertIssuerDownload "Thawte Code Signing CA - G2"

# Dialog units are used so the UI displays correctly with the system's DPI
# settings.
# The dialog units for the bitmap's dimensions should match exactly with the
# bitmap's width and height in pixels.
!define APPNAME_BMP_WIDTH_DU 158u
!define APPNAME_BMP_HEIGHT_DU 21u
!define INTRO_BLURB_WIDTH_DU "232u"
!define INTRO_BLURB_EDGE_DU "196u"
!define INTRO_BLURB_LTR_TOP_DU "16u"
!define INTRO_BLURB_RTL_TOP_DU "11u"

# UI Colors that can be customized for each channel
!define FOOTER_CONTROL_TEXT_COLOR_NORMAL 0x000000
!define FOOTER_CONTROL_TEXT_COLOR_FADED 0x999999
!define FOOTER_BKGRD_COLOR 0xFFFFFF
!define INTRO_BLURB_TEXT_COLOR 0xFFFFFF
!define OPTIONS_TEXT_COLOR_NORMAL 0xFFFFFF
!define OPTIONS_TEXT_COLOR_FADED 0xA1AAB3
!define OPTIONS_BKGRD_COLOR 0x0F1B26
!define INSTALL_BLURB_TEXT_COLOR 0xFFFFFF
!define INSTALL_PROGRESS_TEXT_COLOR_NORMAL 0xFFFFFF
!define INSTALL_PROGRESS_TEXT_COLOR_FADED 0xA1AAB3
