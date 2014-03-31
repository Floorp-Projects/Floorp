# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Required Plugins:
# AppAssocReg
# CertCheck
# InetBgDL
# ShellLink
# UAC

; Set verbosity to 3 (e.g. no script) to lessen the noise in the build logs
!verbose 3

SetDatablockOptimize on
SetCompress off
CRCCheck on

RequestExecutionLevel user

; The commands inside this ifdef require NSIS 3.0a2 or greater so the ifdef can
; be removed after we require NSIS 3.0a2 or greater.
!ifdef NSIS_PACKEDVERSION
  Unicode true
  ManifestSupportedOS all
  ManifestDPIAware true
!endif

!addplugindir ./

Var Dialog
Var Progressbar
Var ProgressbarMarqueeIntervalMS
Var LabelDownloading
Var LabelInstalling
Var LabelFreeSpace
Var CheckboxSetAsDefault
Var CheckboxShortcutOnBar ; Used for Quicklaunch or Taskbar as appropriate
Var CheckboxShortcutInStartMenu
Var CheckboxShortcutOnDesktop
Var CheckboxSendPing
Var CheckboxInstallMaintSvc
Var DirRequest
Var ButtonBrowse
Var LabelBlurb1
Var LabelBlurb2
Var LabelBlurb3
Var BitmapBlurb1
Var BitmapBlurb2
Var BitmapBlurb3
Var HwndBitmapBlurb1
Var HwndBitmapBlurb2
Var HWndBitmapBlurb3

Var FontNormal
Var FontItalic
Var FontBlurb

Var WasOptionsButtonClicked
Var CanWriteToInstallDir
Var HasRequiredSpaceAvailable
Var IsDownloadFinished
Var DownloadSizeBytes
Var HalfOfDownload
Var DownloadReset
Var ExistingTopDir
Var SpaceAvailableBytes
Var InitialInstallDir
Var HandleDownload
Var CanSetAsDefault
Var InstallCounterStep
Var InstallStepSize
Var InstallTotalSteps
Var TmpVal

Var ExitCode
Var FirefoxLaunchCode

; The first three tick counts are for the start of a phase and equate equate to
; the display of individual installer pages.
Var StartIntroPhaseTickCount
Var StartOptionsPhaseTickCount
Var StartDownloadPhaseTickCount
; Since the Intro and Options pages can be displayed multiple times the total
; seconds spent on each of these pages is reported.
Var IntroPhaseSeconds
Var OptionsPhaseSeconds
; The tick count for the last download.
Var StartLastDownloadTickCount
; The number of seconds from the start of the download phase until the first
; bytes are received. This is only recorded for first request so it is possible
; to determine connection issues for the first request.
Var DownloadFirstTransferSeconds
; The last four tick counts are for the end of a phase in the installation page.
Var EndDownloadPhaseTickCount
Var EndPreInstallPhaseTickCount
Var EndInstallPhaseTickCount
Var EndFinishPhaseTickCount

Var InitialInstallRequirementsCode
Var ExistingProfile
Var ExistingVersion
Var ExistingBuildID
Var DownloadedBytes
Var DownloadRetryCount
Var OpenedDownloadPage
Var DownloadServerIP

Var ControlHeightPX
Var ControlRightPX

; Uncomment the following to prevent pinging the metrics server when testing
; the stub installer
;!define STUB_DEBUG

!define StubURLVersion "v6"

; Successful install exit code
!define ERR_SUCCESS 0

/**
 * The following errors prefixed with ERR_DOWNLOAD apply to the download phase.
 */
; The download was cancelled by the user
!define ERR_DOWNLOAD_CANCEL 10

; Too many attempts to download. The maximum attempts is defined in
; DownloadMaxRetries.
!define ERR_DOWNLOAD_TOO_MANY_RETRIES 11

/**
 * The following errors prefixed with ERR_PREINSTALL apply to the pre-install
 * check phase.
 */
; Unable to acquire a file handle to the downloaded file
!define ERR_PREINSTALL_INVALID_HANDLE 20

; The downloaded file's certificate is not trusted by the certificate store.
!define ERR_PREINSTALL_CERT_UNTRUSTED 21

; The downloaded file's certificate attribute values were incorrect.
!define ERR_PREINSTALL_CERT_ATTRIBUTES 22

; The downloaded file's certificate is not trusted by the certificate store and
; certificate attribute values were incorrect.
!define ERR_PREINSTALL_CERT_UNTRUSTED_AND_ATTRIBUTES 23

/**
 * The following errors prefixed with ERR_INSTALL apply to the install phase.
 */
; The installation timed out. The installation timeout is defined by the number
; of progress steps defined in InstallTotalSteps and the install timer
; interval defined in InstallIntervalMS
!define ERR_INSTALL_TIMEOUT 30

; Maximum times to retry the download before displaying an error
!define DownloadMaxRetries 9

; Minimum size expected to download in bytes
!define DownloadMinSizeBytes 15728640 ; 15 MB

; Maximum size expected to download in bytes
!define DownloadMaxSizeBytes 36700160 ; 35 MB

; Interval before retrying to download. 3 seconds is used along with 10
; attempted downloads (the first attempt along with 9 retries) to give a
; minimum of 30 seconds or retrying before giving up.
!define DownloadRetryIntervalMS 3000

; Interval for the download timer
!define DownloadIntervalMS 200

; Interval for the install timer
!define InstallIntervalMS 100

; The first step for the install progress bar. By starting with a large step
; immediate feedback is given to the user.
!define InstallProgressFirstStep 20

; The finish step size to quickly increment the progress bar after the
; installation has finished.
!define InstallProgressFinishStep 40

; Number of steps for the install progress.
; This might not be enough when installing on a slow network drive so it will
; fallback to downloading the full installer if it reaches this number. The size
; of the install progress step is increased when the full installer finishes
; instead of waiting.

; Approximately 150 seconds with a 100 millisecond timer and a first step of 20
; as defined by InstallProgressFirstStep.
!define /math InstallCleanTotalSteps ${InstallProgressFirstStep} + 1500

; Approximately 165 seconds (minus 0.2 seconds for each file that is removed)
; with a 100 millisecond timer and a first step of 20 as defined by
; InstallProgressFirstStep .
!define /math InstallPaveOverTotalSteps ${InstallProgressFirstStep} + 1800

; On Vista and above attempt to elevate Standard Users in addition to users that
; are a member of the Administrators group.
!define NONADMIN_ELEVATE

!define CONFIG_INI "config.ini"

!ifndef FILE_SHARE_READ
  !define FILE_SHARE_READ 1
!endif
!ifndef GENERIC_READ
  !define GENERIC_READ 0x80000000
!endif
!ifndef OPEN_EXISTING
  !define OPEN_EXISTING 3
!endif
!ifndef INVALID_HANDLE_VALUE
  !define INVALID_HANDLE_VALUE -1
!endif

!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "WinVer.nsh"
!include "WordFunc.nsh"

!insertmacro GetParameters
!insertmacro GetOptions
!insertmacro StrFilter

!include "locales.nsi"
!include "branding.nsi"

!include "defines.nsi"

; The OFFICIAL define is a workaround to support different urls for Release and
; Beta since they share the same branding when building with other branches that
; set the update channel to beta.
!ifdef OFFICIAL
!ifdef BETA_UPDATE_CHANNEL
!undef URLStubDownload
!define URLStubDownload "http://download.mozilla.org/?os=win&lang=${AB_CD}&product=firefox-beta-latest"
!undef URLManualDownload
!define URLManualDownload "https://www.mozilla.org/${AB_CD}/firefox/installer-help/?channel=beta&installer_lang=${AB_CD}"
!undef Channel
!define Channel "beta"
!endif
!endif

!include "common.nsh"

!insertmacro ElevateUAC
!insertmacro GetLongPath
!insertmacro GetPathFromString
!insertmacro GetParent
!insertmacro GetSingleInstallPath
!insertmacro GetTextWidthHeight
!insertmacro IsUserAdmin
!insertmacro OnStubInstallUninstall
!insertmacro SetBrandNameVars
!insertmacro UnloadUAC

VIAddVersionKey "FileDescription" "${BrandShortName} Stub Installer"
VIAddVersionKey "OriginalFilename" "setup-stub.exe"

Name "$BrandFullName"
OutFile "setup-stub.exe"
icon "setup.ico"
XPStyle on
BrandingText " "
ChangeUI all "nsisui.exe"
!ifdef HAVE_64BIT_OS
  InstallDir "$PROGRAMFILES64\${BrandFullName}\"
!else
  InstallDir "$PROGRAMFILES32\${BrandFullName}\"
!endif

!ifdef ${AB_CD}_rtl
  LoadLanguageFile "locale-rtl.nlf"
!else
  LoadLanguageFile "locale.nlf"
!endif

!include "nsisstrings.nlf"

!if "${AB_CD}" == "en-US"
  ; Custom strings for en-US. This is done here so they aren't translated.
  !include oneoff_en-US.nsh
!else
  !define INTRO_BLURB "$(INTRO_BLURB1)"
  !define INSTALL_BLURB1 "$(INSTALL_BLURB1)"
  !define INSTALL_BLURB2 "$(INSTALL_BLURB2)"
  !define INSTALL_BLURB3 "$(INSTALL_BLURB3)"
!endif

Caption "$(WIN_CAPTION)"

Page custom createDummy ; Needed to enable the Intro page's back button
Page custom createIntro leaveIntro ; Introduction page
Page custom createOptions leaveOptions ; Options page
Page custom createInstall ; Download / Installation page

Function .onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  StrCpy $LANGUAGE 0
  ; This macro is used to set the brand name variables but the ini file method
  ; isn't supported for the stub installer.
  ${SetBrandNameVars} "$PLUGINSDIR\ignored.ini"

!ifdef HAVE_64BIT_OS
  ; Restrict x64 builds from being installed on x86 and pre Vista
  ${Unless} ${RunningX64}
  ${OrUnless} ${AtLeastWinVista}
    MessageBox MB_OK|MB_ICONSTOP "$(WARN_MIN_SUPPORTED_OS_MSG)"
    Quit
  ${EndUnless}

  SetRegView 64
!else
  StrCpy $R8 "0"
  ${If} ${AtMostWin2000}
    StrCpy $R8 "1"
  ${EndIf}

  ${If} ${IsWinXP}
  ${AndIf} ${AtMostServicePack} 1
    StrCpy $R8 "1"
  ${EndIf}

  ${If} $R8 == "1"
    ; XXX-rstrong - some systems failed the AtLeastWin2000 test that we
    ; used to use for an unknown reason and likely fail the AtMostWin2000
    ; and possibly the IsWinXP test as well. To work around this also
    ; check if the Windows NT registry Key exists and if it does if the
    ; first char in CurrentVersion is equal to 3 (Windows NT 3.5 and
    ; 3.5.1), 4 (Windows NT 4), or 5 (Windows 2000 and Windows XP).
    StrCpy $R8 ""
    ClearErrors
    ReadRegStr $R8 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion" "CurrentVersion"
    StrCpy $R8 "$R8" 1
    ${If} ${Errors}
    ${OrIf} "$R8" == "3"
    ${OrIf} "$R8" == "4"
    ${OrIf} "$R8" == "5"
      MessageBox MB_OK|MB_ICONSTOP "$(WARN_MIN_SUPPORTED_OS_MSG)"
      Quit
    ${EndIf}
  ${EndUnless}
!endif

  ; Require elevation if the user can elevate
  ${ElevateUAC}

; The commands inside this ifndef are needed prior to NSIS 3.0a2 and can be
; removed after we require NSIS 3.0a2 or greater.
!ifndef NSIS_PACKEDVERSION
  ${If} ${AtLeastWinVista}
    System::Call 'user32::SetProcessDPIAware()'
  ${EndIf}
!endif

  SetShellVarContext all ; Set SHCTX to HKLM
  ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9

  ${If} "$R9" == "false"
    SetShellVarContext current ; Set SHCTX to HKCU
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9
  ${EndIf}

  ${If} "$R9" != "false"
    StrCpy $INSTDIR "$R9"
  ${EndIf}

  ; Used to determine if the default installation directory was used.
  StrCpy $InitialInstallDir "$INSTDIR"

  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" \
                   "Write Test"

  ; Only display set as default when there is write access to HKLM and on Win7
  ; and below.
  ${If} ${Errors}
  ${OrIf} ${AtLeastWin8}
    StrCpy $CanSetAsDefault "false"
    StrCpy $CheckboxSetAsDefault "0"
  ${Else}
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    StrCpy $CanSetAsDefault "true"
  ${EndIf}

  ; The interval in MS used for the progress bars set as marquee.
  ${If} ${AtLeastWinVista}
    StrCpy $ProgressbarMarqueeIntervalMS "10"
  ${Else}
    StrCpy $ProgressbarMarqueeIntervalMS "50"
  ${EndIf}

  ; Initialize the majority of variables except those that need to be reset
  ; when a page is displayed.
  StrCpy $IntroPhaseSeconds "0"
  StrCpy $OptionsPhaseSeconds "0"
  StrCpy $EndPreInstallPhaseTickCount "0"
  StrCpy $EndInstallPhaseTickCount "0"
  StrCpy $InitialInstallRequirementsCode ""
  StrCpy $IsDownloadFinished ""
  StrCpy $FirefoxLaunchCode "0"
  StrCpy $CheckboxShortcutOnBar "1"
  StrCpy $CheckboxShortcutInStartMenu "1"
  StrCpy $CheckboxShortcutOnDesktop "1"
  StrCpy $CheckboxSendPing "1"
!ifdef MOZ_MAINTENANCE_SERVICE
  StrCpy $CheckboxInstallMaintSvc "1"
!else
  StrCpy $CheckboxInstallMaintSvc "0"
!endif
  StrCpy $WasOptionsButtonClicked "0"

  CreateFont $FontBlurb "$(^Font)" "12" "500"
  CreateFont $FontNormal "$(^Font)" "11" "500"
  CreateFont $FontItalic "$(^Font)" "11" "500" /ITALIC

  InitPluginsDir
  File /oname=$PLUGINSDIR\bgintro.bmp "bgintro.bmp"
  File /oname=$PLUGINSDIR\bgplain.bmp "bgplain.bmp"
  File /oname=$PLUGINSDIR\appname.bmp "appname.bmp"
  File /oname=$PLUGINSDIR\clock.bmp "clock.bmp"
  File /oname=$PLUGINSDIR\particles.bmp "particles.bmp"
!ifdef ${AB_CD}_rtl
  ; The horizontally flipped pencil looks better in RTL
  File /oname=$PLUGINSDIR\pencil.bmp "pencil-rtl.bmp"
!else
  File /oname=$PLUGINSDIR\pencil.bmp "pencil.bmp"
!endif
FunctionEnd

; .onGUIInit isn't needed except for RTL locales
!ifdef ${AB_CD}_rtl
Function .onGUIInit
  ; Since NSIS RTL support doesn't mirror progress bars use Windows mirroring.
  ${NSD_AddExStyle} $HWNDPARENT ${WS_EX_LAYOUTRTL}
  ${RemoveExStyle} $HWNDPARENT ${WS_EX_RTLREADING}
  ${RemoveExStyle} $HWNDPARENT ${WS_EX_RIGHT}
  ${NSD_AddExStyle} $HWNDPARENT ${WS_EX_LEFT}|${WS_EX_LTRREADING}
FunctionEnd
!endif

Function .onGUIEnd
  Delete "$PLUGINSDIR\_temp"
  Delete "$PLUGINSDIR\download.exe"
  Delete "$PLUGINSDIR\${CONFIG_INI}"

  ${UnloadUAC}
FunctionEnd

Function .onUserAbort
  ${NSD_KillTimer} StartDownload
  ${NSD_KillTimer} OnDownload
  ${NSD_KillTimer} CheckInstall
  ${NSD_KillTimer} FinishInstall
  ${NSD_KillTimer} FinishProgressBar
  ${NSD_KillTimer} DisplayDownloadError

  ${If} "$IsDownloadFinished" != ""
    Call DisplayDownloadError
    ; Aborting the abort will allow SendPing which is called by
    ; DisplayDownloadError to hide the installer window and close the installer
    ; after it sends the metrics ping.
    Abort
  ${EndIf}
FunctionEnd

Function SendPing
  HideWindow
  ; Try to send a ping if a download was attempted
  ${If} $CheckboxSendPing == 1
  ${AndIf} $IsDownloadFinished != ""
    ; Get the tick count for the completion of all phases.
    System::Call "kernel32::GetTickCount()l .s"
    Pop $EndFinishPhaseTickCount

    ; When the value of $IsDownloadFinished is false the download was started
    ; but didn't finish. In this case the tick count stored in
    ; $EndFinishPhaseTickCount is used to determine how long the download was
    ; in progress.
    ${If} "$IsDownloadFinished" == "false"
    ${OrIf} "$EndDownloadPhaseTickCount" == ""
      StrCpy $EndDownloadPhaseTickCount "$EndFinishPhaseTickCount"
      ; Cancel the download in progress
      InetBgDL::Get /RESET /END
    ${EndIf}


    ; When $DownloadFirstTransferSeconds equals an empty string the download
    ; never successfully started so set the value to 0. It will be possible to
    ; determine that the download didn't successfully start from the seconds for
    ; the last download.
    ${If} "$DownloadFirstTransferSeconds" == ""
      StrCpy $DownloadFirstTransferSeconds "0"
    ${EndIf}

    ; When $StartLastDownloadTickCount equals an empty string the download never
    ; successfully started so set the value to $EndDownloadPhaseTickCount to
    ; compute the correct value.
    ${If} $StartLastDownloadTickCount == ""
      ; This could happen if the download never successfully starts
      StrCpy $StartLastDownloadTickCount "$EndDownloadPhaseTickCount"
    ${EndIf}

    ; When $EndPreInstallPhaseTickCount equals 0 the installation phase was
    ; never completed so set its value to $EndFinishPhaseTickCount to compute
    ; the correct value.
    ${If} "$EndPreInstallPhaseTickCount" == "0"
      StrCpy $EndPreInstallPhaseTickCount "$EndFinishPhaseTickCount"
    ${EndIf}

    ; When $EndInstallPhaseTickCount equals 0 the installation phase was never
    ; completed so set its value to $EndFinishPhaseTickCount to compute the
    ; correct value.
    ${If} "$EndInstallPhaseTickCount" == "0"
      StrCpy $EndInstallPhaseTickCount "$EndFinishPhaseTickCount"
    ${EndIf}

    ; Get the seconds elapsed from the start of the download phase to the end of
    ; the download phase.
    ${GetSecondsElapsed} "$StartDownloadPhaseTickCount" "$EndDownloadPhaseTickCount" $0

    ; Get the seconds elapsed from the start of the last download to the end of
    ; the last download.
    ${GetSecondsElapsed} "$StartLastDownloadTickCount" "$EndDownloadPhaseTickCount" $1

    ; Get the seconds elapsed from the end of the download phase to the
    ; completion of the pre-installation check phase.
    ${GetSecondsElapsed} "$EndDownloadPhaseTickCount" "$EndPreInstallPhaseTickCount" $2

    ; Get the seconds elapsed from the end of the pre-installation check phase
    ; to the completion of the installation phase.
    ${GetSecondsElapsed} "$EndPreInstallPhaseTickCount" "$EndInstallPhaseTickCount" $3

    ; Get the seconds elapsed from the end of the installation phase to the
    ; completion of all phases.
    ${GetSecondsElapsed} "$EndInstallPhaseTickCount" "$EndFinishPhaseTickCount" $4

!ifdef HAVE_64BIT_OS
    StrCpy $R0 "1"
!else
    StrCpy $R0 "0"
!endif

    ${If} ${RunningX64}
      StrCpy $R1 "1"
    ${Else}
      StrCpy $R1 "0"
    ${EndIf}

    ; Though these values are sometimes incorrect due to bug 444664 it happens
    ; so rarely it isn't worth working around it by reading the registry values.
    ${WinVerGetMajor} $5
    ${WinVerGetMinor} $6
    ${WinVerGetBuild} $7
    ${WinVerGetServicePackLevel} $8
    ${If} ${IsServerOS}
      StrCpy $9 "1"
    ${Else}
      StrCpy $9 "0"
    ${EndIf}

    ${If} "$ExitCode" == "${ERR_SUCCESS}"
      ReadINIStr $R5 "$INSTDIR\application.ini" "App" "Version"
      ReadINIStr $R6 "$INSTDIR\application.ini" "App" "BuildID"
    ${Else}
      StrCpy $R5 "0"
      StrCpy $R6 "0"
    ${EndIf}

    ; Whether installed into the default installation directory
    ${GetLongPath} "$INSTDIR" $R7
    ${GetLongPath} "$InitialInstallDir" $R8
    ${If} "$R7" == "$R8"
      StrCpy $R7 "1"
    ${Else}
      StrCpy $R7 "0"
    ${EndIf}

    ClearErrors
    WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" \
                     "Write Test"
    ${If} ${Errors}
      StrCpy $R8 "0"
    ${Else}
      DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
      StrCpy $R8 "1"
    ${EndIf}

    ${If} "$DownloadServerIP" == ""
      StrCpy $DownloadServerIP "Unknown"
    ${EndIf}

    StrCpy $R2 ""
    SetShellVarContext current ; Set SHCTX to the current user
    ReadRegStr $R2 HKCU "Software\Classes\http\shell\open\command" ""
    ${If} $R2 != ""
      ${GetPathFromString} "$R2" $R2
      ${GetParent} "$R2" $R3
      ${GetLongPath} "$R3" $R3
      ${If} $R3 == $INSTDIR
        StrCpy $R2 "1" ; This Firefox install is set as default.
      ${Else}
        StrCpy $R2 "$R2" "" -11 # length of firefox.exe
        ${If} "$R2" == "${FileMainEXE}"
          StrCpy $R2 "2" ; Another Firefox install is set as default.
        ${Else}
          StrCpy $R2 "0"
        ${EndIf}
      ${EndIf}
    ${Else}
      StrCpy $R2 "0" ; Firefox is not set as default.
    ${EndIf}

    ${If} "$R2" == "0"
    ${AndIf} ${AtLeastWinVista}
      ; Check to see if this install location is currently set as the default
      ; browser by Default Programs which is only available on Vista and above.
      ClearErrors
      ReadRegStr $R3 HKLM "Software\RegisteredApplications" "${AppRegName}"
      ${Unless} ${Errors}
        AppAssocReg::QueryAppIsDefaultAll "${AppRegName}" "effective"
        Pop $R3
        ${If} $R3 == "1"
          StrCpy $R3 ""
          ReadRegStr $R2 HKLM "Software\Classes\http\shell\open\command" ""
          ${If} $R2 != ""
            ${GetPathFromString} "$R2" $R2
            ${GetParent} "$R2" $R3
            ${GetLongPath} "$R3" $R3
            ${If} $R3 == $INSTDIR
              StrCpy $R2 "1" ; This Firefox install is set as default.
            ${Else}
              StrCpy $R2 "$R2" "" -11 # length of firefox.exe
              ${If} "$R2" == "${FileMainEXE}"
                StrCpy $R2 "2" ; Another Firefox install is set as default.
              ${Else}
                StrCpy $R2 "0"
              ${EndIf}
            ${EndIf}
          ${Else}
            StrCpy $R2 "0" ; Firefox is not set as default.
          ${EndIf}
        ${EndIf}
      ${EndUnless}
    ${EndIf}

    ${If} $CanSetAsDefault == "true"
      ${If} $CheckboxSetAsDefault == "1"
        StrCpy $R3 "2"
      ${Else}
        StrCpy $R3 "3"
      ${EndIf}
    ${Else}
      ${If} ${AtLeastWin8}
        StrCpy $R3 "1"
      ${Else}
        StrCpy $R3 "0"
      ${EndIf}
    ${EndIf}

!ifdef STUB_DEBUG
    MessageBox MB_OK "${BaseURLStubPing} \
                      $\nStub URL Version = ${StubURLVersion}${StubURLVersionAppend} \
                      $\nBuild Channel = ${Channel} \
                      $\nUpdate Channel = ${UpdateChannel} \
                      $\nLocale = ${AB_CD} \
                      $\nFirefox x64 = $R0 \
                      $\nRunning x64 Windows = $R1 \
                      $\nMajor = $5 \
                      $\nMinor = $6 \
                      $\nBuild = $7 \
                      $\nServicePack = $8 \
                      $\nIsServer = $9 \
                      $\nExit Code = $ExitCode \
                      $\nFirefox Launch Code = $FirefoxLaunchCode \
                      $\nDownload Retry Count = $DownloadRetryCount \
                      $\nDownloaded Bytes = $DownloadedBytes \
                      $\nDownload Size Bytes = $DownloadSizeBytes \
                      $\nIntroduction Phase Seconds = $IntroPhaseSeconds \
                      $\nOptions Phase Seconds = $OptionsPhaseSeconds \
                      $\nDownload Phase Seconds = $0 \
                      $\nLast Download Seconds = $1 \
                      $\nDownload First Transfer Seconds = $DownloadFirstTransferSeconds \
                      $\nPreinstall Phase Seconds = $2 \
                      $\nInstall Phase Seconds = $3 \
                      $\nFinish Phase Seconds = $4 \
                      $\nInitial Install Requirements Code = $InitialInstallRequirementsCode \
                      $\nOpened Download Page = $OpenedDownloadPage \
                      $\nExisting Profile = $ExistingProfile \
                      $\nExisting Version = $ExistingVersion \
                      $\nExisting Build ID = $ExistingBuildID \
                      $\nNew Version = $R5 \
                      $\nNew Build ID = $R6 \
                      $\nDefault Install Dir = $R7 \
                      $\nHas Admin = $R8 \
                      $\nDefault Status = $R2 \
                      $\nSet As Sefault Status = $R3 \
                      $\nDownload Server IP = $DownloadServerIP"
    ; The following will exit the installer
    SetAutoClose true
    StrCpy $R9 "2"
    Call RelativeGotoPage
!else
    ${NSD_CreateTimer} OnPing ${DownloadIntervalMS}
    InetBgDL::Get "${BaseURLStubPing}/${StubURLVersion}/${Channel}/${UpdateChannel}/${AB_CD}/$R0/$R1/$5/$6/$7/$8/$9/$ExitCode/$FirefoxLaunchCode/$DownloadRetryCount/$DownloadedBytes/$DownloadSizeBytes/$IntroPhaseSeconds/$OptionsPhaseSeconds/$0/$1/$DownloadFirstTransferSeconds/$2/$3/$4/$InitialInstallRequirementsCode/$OpenedDownloadPage/$ExistingProfile/$ExistingVersion/$ExistingBuildID/$R5/$R6/$R7/$R8/$R2/$R3/$DownloadServerIP" \
                  "$PLUGINSDIR\_temp" /END
!endif
  ${Else}
    ${If} "$IsDownloadFinished" == "false"
      ; Cancel the download in progress
      InetBgDL::Get /RESET /END
    ${EndIf}
    ; The following will exit the installer
    SetAutoClose true
    StrCpy $R9 "2"
    Call RelativeGotoPage
  ${EndIf}
FunctionEnd

Function createDummy
FunctionEnd

Function createIntro
  nsDialogs::Create /NOUNLOAD 1018
  Pop $Dialog

  GetFunctionAddress $0 OnBack
  nsDialogs::OnBack /NOUNLOAD $0

!ifdef ${AB_CD}_rtl
  ; For RTL align the text with the top of the F in the Firefox bitmap
  StrCpy $0 "${INTRO_BLURB_RTL_TOP_DU}"
!else
  ; For LTR align the text with the top of the x in the Firefox bitmap
  StrCpy $0 "${INTRO_BLURB_LTR_TOP_DU}"
!endif
  ${NSD_CreateLabel} ${INTRO_BLURB_EDGE_DU} $0 ${INTRO_BLURB_WIDTH_DU} 76u "${INTRO_BLURB}"
  Pop $0
  SendMessage $0 ${WM_SETFONT} $FontBlurb 0
  SetCtlColors $0 ${INTRO_BLURB_TEXT_COLOR} transparent

  SetCtlColors $HWNDPARENT ${FOOTER_CONTROL_TEXT_COLOR_NORMAL} ${FOOTER_BKGRD_COLOR}
  GetDlgItem $0 $HWNDPARENT 10 ; Default browser checkbox
  ${If} "$CanSetAsDefault" == "true"
    ; The uxtheme must be disabled on checkboxes in order to override the
    ; system font color.
    System::Call 'uxtheme::SetWindowTheme(i $0 , w " ", w " ")'
    SendMessage $0 ${WM_SETFONT} $FontNormal 0
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(MAKE_DEFAULT)"
    SendMessage $0 ${BM_SETCHECK} 1 0
    SetCtlColors $0 ${FOOTER_CONTROL_TEXT_COLOR_NORMAL} ${FOOTER_BKGRD_COLOR}
  ${Else}
    ShowWindow $0 ${SW_HIDE}
  ${EndIf}
  GetDlgItem $0 $HWNDPARENT 11
  ShowWindow $0 ${SW_HIDE}

  ${NSD_CreateBitmap} ${APPNAME_BMP_EDGE_DU} ${APPNAME_BMP_TOP_DU} \
                      ${APPNAME_BMP_WIDTH_DU} ${APPNAME_BMP_HEIGHT_DU} ""
  Pop $2
  ${SetStretchedTransparentImage} $2 $PLUGINSDIR\appname.bmp $0

  ${NSD_CreateBitmap} 0 0 100% 100% ""
  Pop $2
  ${NSD_SetStretchedImage} $2 $PLUGINSDIR\bgintro.bmp $1

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(UPGRADE_BUTTON)"
  ${Else}
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(INSTALL_BUTTON)"
  ${EndIf}
  ${NSD_SetFocus} $0

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(CANCEL_BUTTON)"

  GetDlgItem $0 $HWNDPARENT 3 ; Back button used for Options
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(OPTIONS_BUTTON)"

  System::Call "kernel32::GetTickCount()l .s"
  Pop $StartIntroPhaseTickCount

  LockWindow off
  nsDialogs::Show

  ${NSD_FreeImage} $0
  ${NSD_FreeImage} $1
FunctionEnd

Function leaveIntro
  LockWindow on

  System::Call "kernel32::GetTickCount()l .s"
  Pop $0
  ${GetSecondsElapsed} "$StartIntroPhaseTickCount" "$0" $IntroPhaseSeconds
  ; It is possible for this value to be 0 if the user clicks fast enough so
  ; increment the value by 1 if it is 0.
  ${If} $IntroPhaseSeconds == 0
    IntOp $IntroPhaseSeconds $IntroPhaseSeconds + 1
  ${EndIf}

  SetShellVarContext all ; Set SHCTX to All Users
  ; If the user doesn't have write access to the installation directory set
  ; the installation directory to a subdirectory of the All Users application
  ; directory and if the user can't write to that location set the installation
  ; directory to a subdirectory of the users local application directory
  ; (e.g. non-roaming).
  Call CanWrite
  ${If} "$CanWriteToInstallDir" == "false"
    StrCpy $INSTDIR "$APPDATA\${BrandFullName}\"
    Call CanWrite
    ${If} "$CanWriteToInstallDir" == "false"
      ; This should never happen but just in case.
      StrCpy $CanWriteToInstallDir "false"
    ${Else}
      StrCpy $INSTDIR "$LOCALAPPDATA\${BrandFullName}\"
      Call CanWrite
    ${EndIf}
  ${EndIf}

  Call CheckSpace

  ${If} ${FileExists} "$INSTDIR"
    ; Always display the long path if the path exists.
    ${GetLongPath} "$INSTDIR" $INSTDIR
  ${EndIf}

FunctionEnd

Function createOptions
  ; Check whether the install requirements are satisfied using the default
  ; values for metrics.
  ${If} "$InitialInstallRequirementsCode" == ""
    ${If} "$CanWriteToInstallDir" != "true"
    ${AndIf} "$HasRequiredSpaceAvailable" != "true"
      StrCpy $InitialInstallRequirementsCode "1"
    ${ElseIf} "$CanWriteToInstallDir" != "true"
      StrCpy $InitialInstallRequirementsCode "2"
    ${ElseIf} "$HasRequiredSpaceAvailable" != "true"
      StrCpy $InitialInstallRequirementsCode "3"
    ${Else}
      StrCpy $InitialInstallRequirementsCode "0"
    ${EndIf}
  ${EndIf}

  ; Skip the options page unless the Options button was clicked as long as the
  ; installation directory can be written to and there is the minimum required
  ; space available.
  ${If} "$WasOptionsButtonClicked" != "1"
    ${If} "$CanWriteToInstallDir" == "true"
    ${AndIf} "$HasRequiredSpaceAvailable" == "true"
      Abort ; Skip the options page
    ${EndIf}
  ${EndIf}

  StrCpy $ExistingTopDir ""

  nsDialogs::Create /NOUNLOAD 1018
  Pop $Dialog
  ; Since the text color for controls is set in this Dialog the foreground and
  ; background colors of the Dialog must also be hardcoded.
  SetCtlColors $Dialog ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}

  ${NSD_CreateLabel} ${OPTIONS_ITEM_EDGE_DU} 18u ${OPTIONS_ITEM_WIDTH_DU} \
                     12u "$(CREATE_SHORTCUTS)"
  Pop $0
  SetCtlColors $0 ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $0 ${WM_SETFONT} $FontNormal 0

  ${If} ${AtLeastWin7}
    StrCpy $0 "$(ADD_SC_TASKBAR)"
  ${Else}
    StrCpy $0 "$(ADD_SC_QUICKLAUNCHBAR)"
  ${EndIf}
  ${NSD_CreateCheckbox} ${OPTIONS_SUBITEM_EDGE_DU} 38u \
                        ${OPTIONS_SUBITEM_WIDTH_DU} 12u "$0"
  Pop $CheckboxShortcutOnBar
  ; The uxtheme must be disabled on checkboxes in order to override the system
  ; font color.
  System::Call 'uxtheme::SetWindowTheme(i $CheckboxShortcutOnBar, w " ", w " ")'
  SetCtlColors $CheckboxShortcutOnBar ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $CheckboxShortcutOnBar ${WM_SETFONT} $FontNormal 0
  ${NSD_Check} $CheckboxShortcutOnBar

  ${NSD_CreateCheckbox} ${OPTIONS_SUBITEM_EDGE_DU} 54u ${OPTIONS_SUBITEM_WIDTH_DU} \
                        12u "$(ADD_CheckboxShortcutInStartMenu)"
  Pop $CheckboxShortcutInStartMenu
  ; The uxtheme must be disabled on checkboxes in order to override the system
  ; font color.
  System::Call 'uxtheme::SetWindowTheme(i $CheckboxShortcutInStartMenu, w " ", w " ")'
  SetCtlColors $CheckboxShortcutInStartMenu ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $CheckboxShortcutInStartMenu ${WM_SETFONT} $FontNormal 0
  ${NSD_Check} $CheckboxShortcutInStartMenu

  ${NSD_CreateCheckbox} ${OPTIONS_SUBITEM_EDGE_DU} 70u ${OPTIONS_SUBITEM_WIDTH_DU} \
                        12u "$(ADD_CheckboxShortcutOnDesktop)"
  Pop $CheckboxShortcutOnDesktop
  ; The uxtheme must be disabled on checkboxes in order to override the system
  ; font color.
  System::Call 'uxtheme::SetWindowTheme(i $CheckboxShortcutOnDesktop, w " ", w " ")'
  SetCtlColors $CheckboxShortcutOnDesktop ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $CheckboxShortcutOnDesktop ${WM_SETFONT} $FontNormal 0
  ${NSD_Check} $CheckboxShortcutOnDesktop

  ${NSD_CreateLabel} ${OPTIONS_ITEM_EDGE_DU} 100u ${OPTIONS_ITEM_WIDTH_DU} \
                     12u "$(DEST_FOLDER)"
  Pop $0
  SetCtlColors $0 ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $0 ${WM_SETFONT} $FontNormal 0

  ${NSD_CreateDirRequest} ${OPTIONS_SUBITEM_EDGE_DU} 116u 159u 14u "$INSTDIR"
  Pop $DirRequest
  SetCtlColors $DirRequest ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $DirRequest ${WM_SETFONT} $FontNormal 0
  System::Call shlwapi::SHAutoComplete(i $DirRequest, i ${SHACF_FILESYSTEM})
  ${NSD_OnChange} $DirRequest OnChange_DirRequest

!ifdef ${AB_CD}_rtl
  ; Remove the RTL styling from the directory request text box
  ${RemoveStyle} $DirRequest ${SS_RIGHT}
  ${RemoveExStyle} $DirRequest ${WS_EX_RIGHT}
  ${RemoveExStyle} $DirRequest ${WS_EX_RTLREADING}
  ${NSD_AddStyle} $DirRequest ${SS_LEFT}
  ${NSD_AddExStyle} $DirRequest ${WS_EX_LTRREADING}|${WS_EX_LEFT}
!endif

  ${NSD_CreateBrowseButton} 280u 116u 50u 14u "$(BROWSE_BUTTON)"
  Pop $ButtonBrowse
  SetCtlColors $ButtonBrowse "" ${OPTIONS_BKGRD_COLOR}
  ${NSD_OnClick} $ButtonBrowse OnClick_ButtonBrowse

  ; Get the number of pixels from the left of the Dialog to the right side of
  ; the "Space Required:" and "Space Available:" labels prior to setting RTL so
  ; the correct position of the controls can be set by NSIS for RTL locales.

  ; Get the width and height of both labels and use the tallest for the height
  ; and the widest to calculate where to place the labels after these labels.
  ${GetTextExtent} "$(SPACE_REQUIRED)" $FontItalic $0 $1
  ${GetTextExtent} "$(SPACE_AVAILABLE)" $FontItalic $2 $3
  ${If} $1 > $3
    StrCpy $ControlHeightPX "$1"
  ${Else}
    StrCpy $ControlHeightPX "$3"
  ${EndIf}

  IntOp $0 $0 + 8 ; Add padding to the control's width
  ; Make both controls the same width as the widest control
  ${NSD_CreateLabelCenter} ${OPTIONS_SUBITEM_EDGE_DU} 134u $0 $ControlHeightPX "$(SPACE_REQUIRED)"
  Pop $5
  SetCtlColors $5 ${OPTIONS_TEXT_COLOR_FADED} ${OPTIONS_BKGRD_COLOR}
  SendMessage $5 ${WM_SETFONT} $FontItalic 0

  IntOp $2 $2 + 8 ; Add padding to the control's width
  ${NSD_CreateLabelCenter} ${OPTIONS_SUBITEM_EDGE_DU} 145u $2 $ControlHeightPX "$(SPACE_AVAILABLE)"
  Pop $6
  SetCtlColors $6 ${OPTIONS_TEXT_COLOR_FADED} ${OPTIONS_BKGRD_COLOR}
  SendMessage $6 ${WM_SETFONT} $FontItalic 0

  ; Use the widest label for aligning the labels next to them
  ${If} $0 > $2
    StrCpy $6 "$5"
  ${EndIf}
  FindWindow $1 "#32770" "" $HWNDPARENT
  ${GetDlgItemEndPX} $6 $ControlRightPX

  IntOp $ControlRightPX $ControlRightPX + 6

  ${NSD_CreateLabel} $ControlRightPX 134u 100% $ControlHeightPX \
                     "${APPROXIMATE_REQUIRED_SPACE_MB} $(MEGA)$(BYTE)"
  Pop $7
  SetCtlColors $7 ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $7 ${WM_SETFONT} $FontNormal 0

  ; Create the free space label with an empty string and update it by calling
  ; UpdateFreeSpaceLabel
  ${NSD_CreateLabel} $ControlRightPX 145u 100% $ControlHeightPX " "
  Pop $LabelFreeSpace
  SetCtlColors $LabelFreeSpace ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $LabelFreeSpace ${WM_SETFONT} $FontNormal 0

  Call UpdateFreeSpaceLabel

  ${NSD_CreateCheckbox} ${OPTIONS_ITEM_EDGE_DU} 168u ${OPTIONS_SUBITEM_WIDTH_DU} \
                        12u "$(SEND_PING)"
  Pop $CheckboxSendPing
  ; The uxtheme must be disabled on checkboxes in order to override the system
  ; font color.
  System::Call 'uxtheme::SetWindowTheme(i $CheckboxSendPing, w " ", w " ")'
  SetCtlColors $CheckboxSendPing ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $CheckboxSendPing ${WM_SETFONT} $FontNormal 0
  ${NSD_Check} $CheckboxSendPing

!ifdef MOZ_MAINTENANCE_SERVICE
  ; Only show the maintenance service checkbox if we have write access to HKLM
  Call IsUserAdmin
  Pop $0
  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" \
                   "Write Test"
  ${If} ${Errors}
  ${OrIf} $0 != "true"
    StrCpy $CheckboxInstallMaintSvc "0"
  ${Else}
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    ; Read the registry instead of using ServicesHelper::IsInstalled so the
    ; plugin isn't included in the stub installer to lessen its size.
    ClearErrors
    ReadRegStr $0 HKLM "SYSTEM\CurrentControlSet\services\MozillaMaintenance" "ImagePath"
    ${If} ${Errors}
      ${NSD_CreateCheckbox} ${OPTIONS_ITEM_EDGE_DU} 184u ${OPTIONS_ITEM_WIDTH_DU} \
                            12u "$(INSTALL_MAINT_SERVICE)"
      Pop $CheckboxInstallMaintSvc
      System::Call 'uxtheme::SetWindowTheme(i $CheckboxInstallMaintSvc, w " ", w " ")'
      SetCtlColors $CheckboxInstallMaintSvc ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
      SendMessage $CheckboxInstallMaintSvc ${WM_SETFONT} $FontNormal 0
      ${NSD_Check} $CheckboxInstallMaintSvc
    ${EndIf}
  ${EndIf}
!endif

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(UPGRADE_BUTTON)"
  ${Else}
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(INSTALL_BUTTON)"
  ${EndIf}
  ${NSD_SetFocus} $0

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(CANCEL_BUTTON)"

  GetDlgItem $0 $HWNDPARENT 3 ; Back button used for Options
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}

  ; If the option button was not clicked display the reason for what needs to be
  ; resolved to continue the installation.
  ${If} "$WasOptionsButtonClicked" != "1"
    ${If} "$CanWriteToInstallDir" == "false"
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_WRITE_ACCESS)"
    ${ElseIf} "$HasRequiredSpaceAvailable" == "false"
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_DISK_SPACE)"
    ${EndIf}
  ${EndIf}

  System::Call "kernel32::GetTickCount()l .s"
  Pop $StartOptionsPhaseTickCount

  LockWindow off
  nsDialogs::Show
FunctionEnd

Function leaveOptions
  LockWindow on

  ${GetRoot} "$INSTDIR" $0
  ${GetLongPath} "$INSTDIR" $INSTDIR
  ${GetLongPath} "$0" $0
  ${If} "$INSTDIR" == "$0"
    LockWindow off
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_ROOT_INSTALL)"
    Abort ; Stay on the page
  ${EndIf}

  Call CanWrite
  ${If} "$CanWriteToInstallDir" == "false"
    LockWindow off
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_WRITE_ACCESS)"
    Abort ; Stay on the page
  ${EndIf}

  Call CheckSpace
  ${If} "$HasRequiredSpaceAvailable" == "false"
    LockWindow off
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_DISK_SPACE)"
    Abort ; Stay on the page
  ${EndIf}

  System::Call "kernel32::GetTickCount()l .s"
  Pop $0
  ${GetSecondsElapsed} "$StartOptionsPhaseTickCount" "$0" $OptionsPhaseSeconds
  ; It is possible for this value to be 0 if the user clicks fast enough so
  ; increment the value by 1 if it is 0.
  ${If} $OptionsPhaseSeconds == 0
    IntOp $OptionsPhaseSeconds $OptionsPhaseSeconds + 1
  ${EndIf}

  ${NSD_GetState} $CheckboxShortcutOnBar $CheckboxShortcutOnBar
  ${NSD_GetState} $CheckboxShortcutInStartMenu $CheckboxShortcutInStartMenu
  ${NSD_GetState} $CheckboxShortcutOnDesktop $CheckboxShortcutOnDesktop
  ${NSD_GetState} $CheckboxSendPing $CheckboxSendPing
!ifdef MOZ_MAINTENANCE_SERVICE
  ${NSD_GetState} $CheckboxInstallMaintSvc $CheckboxInstallMaintSvc
!endif

FunctionEnd

Function createInstall
  nsDialogs::Create /NOUNLOAD 1018
  Pop $Dialog

  ${NSD_CreateLabel} 0 0 49u 64u ""
  Pop $0
  ${GetDlgItemWidthHeight} $0 $1 $2
  System::Call 'user32::DestroyWindow(i r0)'

  ${NSD_CreateLabel} 0 0 11u 16u ""
  Pop $0
  ${GetDlgItemWidthHeight} $0 $3 $4
  System::Call 'user32::DestroyWindow(i r0)'

  FindWindow $7 "#32770" "" $HWNDPARENT
  ${GetDlgItemWidthHeight} $7 $8 $9

  ; Allow a maximum text width of half of the Dialog's width
  IntOp $R0 $8 / 2

  ${GetTextWidthHeight} "${INSTALL_BLURB1}" $FontBlurb $R0 $5 $6
  IntOp $R1 $1 + $3
  IntOp $R1 $R1 + $5
  IntOp $R1 $8 - $R1
  IntOp $R1 $R1 / 2
  ${NSD_CreateBitmap} $R1 ${INSTALL_BLURB_TOP_DU} 49u 64u ""
  Pop $BitmapBlurb1
  ${SetStretchedTransparentImage} $BitmapBlurb1 $PLUGINSDIR\clock.bmp $HwndBitmapBlurb1
  IntOp $R1 $R1 + $1
  IntOp $R1 $R1 + $3
  ${NSD_CreateLabel} $R1 ${INSTALL_BLURB_TOP_DU} $5 $6 "${INSTALL_BLURB1}"
  Pop $LabelBlurb1
  SendMessage $LabelBlurb1 ${WM_SETFONT} $FontBlurb 0
  SetCtlColors $LabelBlurb1 ${INSTALL_BLURB_TEXT_COLOR} transparent

  ${GetTextWidthHeight} "${INSTALL_BLURB2}" $FontBlurb $R0 $5 $6
  IntOp $R1 $1 + $3
  IntOp $R1 $R1 + $5
  IntOp $R1 $8 - $R1
  IntOp $R1 $R1 / 2
  ${NSD_CreateBitmap} $R1 ${INSTALL_BLURB_TOP_DU} 49u 64u ""
  Pop $BitmapBlurb2
  ${SetStretchedTransparentImage} $BitmapBlurb2 $PLUGINSDIR\particles.bmp $HwndBitmapBlurb2
  IntOp $R1 $R1 + $1
  IntOp $R1 $R1 + $3
  ${NSD_CreateLabel} $R1 ${INSTALL_BLURB_TOP_DU} $5 $6 "${INSTALL_BLURB2}"
  Pop $LabelBlurb2
  SendMessage $LabelBlurb2 ${WM_SETFONT} $FontBlurb 0
  SetCtlColors $LabelBlurb2 ${INSTALL_BLURB_TEXT_COLOR} transparent
  ShowWindow $BitmapBlurb2 ${SW_HIDE}
  ShowWindow $LabelBlurb2 ${SW_HIDE}

  ${GetTextWidthHeight} "${INSTALL_BLURB3}" $FontBlurb $R0 $5 $6
  IntOp $R1 $1 + $3
  IntOp $R1 $R1 + $5
  IntOp $R1 $8 - $R1
  IntOp $R1 $R1 / 2
  ${NSD_CreateBitmap} $R1 ${INSTALL_BLURB_TOP_DU} 49u 64u ""
  Pop $BitmapBlurb3
  ${SetStretchedTransparentImage} $BitmapBlurb3 $PLUGINSDIR\pencil.bmp $HWndBitmapBlurb3
  IntOp $R1 $R1 + $1
  IntOp $R1 $R1 + $3
  ${NSD_CreateLabel} $R1 ${INSTALL_BLURB_TOP_DU} $5 $6 "${INSTALL_BLURB3}"
  Pop $LabelBlurb3
  SendMessage $LabelBlurb3 ${WM_SETFONT} $FontBlurb 0
  SetCtlColors $LabelBlurb3 ${INSTALL_BLURB_TEXT_COLOR} transparent
  ShowWindow $BitmapBlurb3 ${SW_HIDE}
  ShowWindow $LabelBlurb3 ${SW_HIDE}

  ${NSD_CreateProgressBar} 103u 166u 241u 9u ""
  Pop $Progressbar
  ${NSD_AddStyle} $Progressbar ${PBS_MARQUEE}
  SendMessage $Progressbar ${PBM_SETMARQUEE} 1 \
              $ProgressbarMarqueeIntervalMS ; start=1|stop=0 interval(ms)=+N

  ${NSD_CreateLabelCenter} 103u 180u 241u 20u "$(DOWNLOADING_LABEL)"
  Pop $LabelDownloading
  SendMessage $LabelDownloading ${WM_SETFONT} $FontNormal 0
  SetCtlColors $LabelDownloading ${INSTALL_PROGRESS_TEXT_COLOR_NORMAL} transparent

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ${NSD_CreateLabelCenter} 103u 180u 241u 20u "$(UPGRADING_LABEL)"
  ${Else}
    ${NSD_CreateLabelCenter} 103u 180u 241u 20u "$(INSTALLING_LABEL)"
  ${EndIf}
  Pop $LabelInstalling
  SendMessage $LabelInstalling ${WM_SETFONT} $FontNormal 0
  SetCtlColors $LabelInstalling ${INSTALL_PROGRESS_TEXT_COLOR_NORMAL} transparent
  ShowWindow $LabelInstalling ${SW_HIDE}

  ${NSD_CreateBitmap} ${APPNAME_BMP_EDGE_DU} ${APPNAME_BMP_TOP_DU} \
                      ${APPNAME_BMP_WIDTH_DU} ${APPNAME_BMP_HEIGHT_DU} ""
  Pop $2
  ${SetStretchedTransparentImage} $2 $PLUGINSDIR\appname.bmp $0

  ${NSD_CreateBitmap} 0 0 100% 100% ""
  Pop $3
  ${NSD_SetStretchedImage} $3 $PLUGINSDIR\bgplain.bmp $1

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 3 ; Back button used for Options
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(CANCEL_BUTTON)"
  ; Focus the Cancel button otherwise it isn't possible to tab to it since it is
  ; the only control that can be tabbed to.
  ${NSD_SetFocus} $0
  ; Kill the Cancel button's focus so pressing enter won't cancel the install.
  SendMessage $0 ${WM_KILLFOCUS} 0 0

  ${If} "$CanSetAsDefault" == "true"
    GetDlgItem $0 $HWNDPARENT 10 ; Default browser checkbox
    SendMessage $0 ${BM_GETCHECK} 0 0 $CheckboxSetAsDefault
    EnableWindow $0 0
    ShowWindow $0 ${SW_HIDE}
  ${EndIf}

  GetDlgItem $0 $HWNDPARENT 11
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(ONE_MOMENT_UPGRADE)"
  ${Else}
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(ONE_MOMENT_INSTALL)"
  ${EndIf}
  SendMessage $0 ${WM_SETFONT} $FontNormal 0
  SetCtlColors $0 ${FOOTER_CONTROL_TEXT_COLOR_FADED} ${FOOTER_BKGRD_COLOR}
  ShowWindow $0 ${SW_SHOW}

  ; Set $DownloadReset to true so the first download tick count is measured.
  StrCpy $DownloadReset "true"
  StrCpy $IsDownloadFinished "false"
  StrCpy $DownloadRetryCount "0"
  StrCpy $DownloadedBytes "0"
  StrCpy $StartLastDownloadTickCount ""
  StrCpy $EndDownloadPhaseTickCount ""
  StrCpy $DownloadFirstTransferSeconds ""
  StrCpy $ExitCode "${ERR_DOWNLOAD_CANCEL}"
  StrCpy $OpenedDownloadPage "0"

  ClearErrors
  ReadINIStr $ExistingVersion "$INSTDIR\application.ini" "App" "Version"
  ${If} ${Errors}
    StrCpy $ExistingVersion "0"
  ${EndIf}

  ClearErrors
  ReadINIStr $ExistingBuildID "$INSTDIR\application.ini" "App" "BuildID"
  ${If} ${Errors}
    StrCpy $ExistingBuildID "0"
  ${EndIf}

  ${If} ${FileExists} "$LOCALAPPDATA\Mozilla\Firefox"
    StrCpy $ExistingProfile "1"
  ${Else}
    StrCpy $ExistingProfile "0"
  ${EndIf}

  StrCpy $DownloadServerIP ""

  System::Call "kernel32::GetTickCount()l .s"
  Pop $StartDownloadPhaseTickCount

  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.log"
    StrCpy $InstallTotalSteps ${InstallPaveOverTotalSteps}
  ${Else}
    StrCpy $InstallTotalSteps ${InstallCleanTotalSteps}
  ${EndIf}

  ${NSD_CreateTimer} StartDownload ${DownloadIntervalMS}

  LockWindow off
  nsDialogs::Show

  ${NSD_FreeImage} $0
  ${NSD_FreeImage} $1
  ${NSD_FreeImage} $HwndBitmapBlurb1
  ${NSD_FreeImage} $HwndBitmapBlurb2
  ${NSD_FreeImage} $HWndBitmapBlurb3
FunctionEnd

Function StartDownload
  ${NSD_KillTimer} StartDownload
  InetBgDL::Get "${URLStubDownload}${URLStubDownloadAppend}" "$PLUGINSDIR\download.exe" \
                /CONNECTTIMEOUT 120 /RECEIVETIMEOUT 120 /END
  StrCpy $4 ""
  ${NSD_CreateTimer} OnDownload ${DownloadIntervalMS}
  ${If} ${FileExists} "$INSTDIR\${TO_BE_DELETED}"
    RmDir /r "$INSTDIR\${TO_BE_DELETED}"
  ${EndIf}
FunctionEnd

Function OnDownload
  InetBgDL::GetStats
  # $0 = HTTP status code, 0=Completed
  # $1 = Completed files
  # $2 = Remaining files
  # $3 = Number of downloaded bytes for the current file
  # $4 = Size of current file (Empty string if the size is unknown)
  # /RESET must be used if status $0 > 299 (e.g. failure)
  # When status is $0 =< 299 it is handled by InetBgDL
  StrCpy $DownloadServerIP "$5"
  ${If} $0 > 299
    ${NSD_KillTimer} OnDownload
    IntOp $DownloadRetryCount $DownloadRetryCount + 1
    ${If} "$DownloadReset" != "true"
      StrCpy $DownloadedBytes "0"
      ${NSD_AddStyle} $Progressbar ${PBS_MARQUEE}
      SendMessage $Progressbar ${PBM_SETMARQUEE} 1 \
                  $ProgressbarMarqueeIntervalMS ; start=1|stop=0 interval(ms)=+N
    ${EndIf}
    InetBgDL::Get /RESET /END
    StrCpy $DownloadSizeBytes ""
    StrCpy $DownloadReset "true"

    ${If} $DownloadRetryCount >= ${DownloadMaxRetries}
      StrCpy $ExitCode "${ERR_DOWNLOAD_TOO_MANY_RETRIES}"
      ; Use a timer so the UI has a chance to update
      ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
    ${Else}
      ${NSD_CreateTimer} StartDownload ${DownloadRetryIntervalMS}
    ${EndIf}
    Return
  ${EndIf}

  ${If} "$DownloadReset" == "true"
    System::Call "kernel32::GetTickCount()l .s"
    Pop $StartLastDownloadTickCount
    StrCpy $DownloadReset "false"
    ; The seconds elapsed from the start of the download phase until the first
    ; bytes are received are only recorded for the first request so it is
    ; possible to determine connection issues for the first request.
    ${If} "$DownloadFirstTransferSeconds" == ""
      ; Get the seconds elapsed from the start of the download phase until the
      ; first bytes are received.
      ${GetSecondsElapsed} "$StartDownloadPhaseTickCount" "$StartLastDownloadTickCount" $DownloadFirstTransferSeconds
    ${EndIf}
  ${EndIf}

  ${If} "$DownloadSizeBytes" == ""
  ${AndIf} "$4" != ""
    ; Handle the case where the size of the file to be downloaded is less than
    ; the minimum expected size or greater than the maximum expected size at the
    ; beginning of the download.
    ${If} $4 < ${DownloadMinSizeBytes}
    ${OrIf} $4 > ${DownloadMaxSizeBytes}
      ${NSD_KillTimer} OnDownload
      InetBgDL::Get /RESET /END
      StrCpy $DownloadReset "true"

      ${If} $DownloadRetryCount >= ${DownloadMaxRetries}
        ; Use a timer so the UI has a chance to update
        ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
      ${Else}
        ${NSD_CreateTimer} StartDownload ${DownloadIntervalMS}
      ${EndIf}
      Return
    ${EndIf}

    StrCpy $DownloadSizeBytes "$4"
    System::Int64Op $4 / 2
    Pop $HalfOfDownload
    System::Int64Op $HalfOfDownload / $InstallTotalSteps
    Pop $InstallStepSize
    SendMessage $Progressbar ${PBM_SETMARQUEE} 0 0 ; start=1|stop=0 interval(ms)=+N
    ${RemoveStyle} $Progressbar ${PBS_MARQUEE}
    System::Int64Op $HalfOfDownload + $DownloadSizeBytes
    Pop $R9
    SendMessage $Progressbar ${PBM_SETRANGE32} 0 $R9
  ${EndIf}

  ; Don't update the status until after the download starts
  ${If} $2 != 0
  ${AndIf} "$4" == ""
    Return
  ${EndIf}

  ; Handle the case where the downloaded size is greater than the maximum
  ; expected size during the download.
  ${If} $DownloadedBytes > ${DownloadMaxSizeBytes}
    InetBgDL::Get /RESET /END
    StrCpy $DownloadReset "true"

    ${If} $DownloadRetryCount >= ${DownloadMaxRetries}
      ; Use a timer so the UI has a chance to update
      ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
    ${Else}
      ${NSD_CreateTimer} StartDownload ${DownloadIntervalMS}
    ${EndIf}
    Return
  ${EndIf}

  ${If} $IsDownloadFinished != "true"
    ${If} $2 == 0
      ${NSD_KillTimer} OnDownload
      StrCpy $IsDownloadFinished "true"
      ; The first step of the install progress bar is determined by the
      ; InstallProgressFirstStep define and provides the user with immediate
      ; feedback.
      StrCpy $InstallCounterStep "${InstallProgressFirstStep}"
      System::Call "kernel32::GetTickCount()l .s"
      Pop $EndDownloadPhaseTickCount

      StrCpy $DownloadedBytes "$DownloadSizeBytes"

      ; When a download has finished handle the case where the  downloaded size
      ; is less than the minimum expected size or greater than the maximum
      ; expected size during the download.
      ${If} $DownloadedBytes < ${DownloadMinSizeBytes}
      ${OrIf} $DownloadedBytes > ${DownloadMaxSizeBytes}
        InetBgDL::Get /RESET /END
        StrCpy $DownloadReset "true"

        ${If} $DownloadRetryCount >= ${DownloadMaxRetries}
          ; Use a timer so the UI has a chance to update
          ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
        ${Else}
          ${NSD_CreateTimer} StartDownload ${DownloadIntervalMS}
        ${EndIf}
        Return
      ${EndIf}

      LockWindow on
      ; Update the progress bars first in the UI change so they take affect
      ; before other UI changes.
      SendMessage $Progressbar ${PBM_SETPOS} $DownloadSizeBytes 0
      System::Int64Op $InstallStepSize * ${InstallProgressFirstStep}
      Pop $R9
      SendMessage $Progressbar ${PBM_SETSTEP} $R9 0
      SendMessage $Progressbar ${PBM_STEPIT} 0 0
      SendMessage $Progressbar ${PBM_SETSTEP} $InstallStepSize 0
      ShowWindow $LabelDownloading ${SW_HIDE}
      ShowWindow $LabelInstalling ${SW_SHOW}
      ShowWindow $LabelBlurb2 ${SW_HIDE}
      ShowWindow $BitmapBlurb2 ${SW_HIDE}
      ShowWindow $LabelBlurb3 ${SW_SHOW}
      ShowWindow $BitmapBlurb3 ${SW_SHOW}
      ; Disable the Cancel button during the install
      GetDlgItem $5 $HWNDPARENT 2
      EnableWindow $5 0
      LockWindow off

      ; Open a handle to prevent modification of the full installer
      StrCpy $R9 "${INVALID_HANDLE_VALUE}"
      System::Call 'kernel32::CreateFileW(w "$PLUGINSDIR\download.exe", \
                                          i ${GENERIC_READ}, \
                                          i ${FILE_SHARE_READ}, i 0, \
                                          i ${OPEN_EXISTING}, i 0, i 0) i .R9'
      StrCpy $HandleDownload "$R9"

      ${If} $HandleDownload == ${INVALID_HANDLE_VALUE}
        StrCpy $ExitCode "${ERR_PREINSTALL_INVALID_HANDLE}"
        StrCpy $0 "0"
        StrCpy $1 "0"
      ${Else}
        CertCheck::VerifyCertTrust "$PLUGINSDIR\download.exe"
        Pop $0
        CertCheck::VerifyCertNameIssuer "$PLUGINSDIR\download.exe" \
                                        "${CertNameDownload}" "${CertIssuerDownload}"
        Pop $1
        ${If} $0 == 0
        ${AndIf} $1 == 0
          StrCpy $ExitCode "${ERR_PREINSTALL_CERT_UNTRUSTED_AND_ATTRIBUTES}"
        ${ElseIf} $0 == 0
          StrCpy $ExitCode "${ERR_PREINSTALL_CERT_UNTRUSTED}"
        ${ElseIf}  $1 == 0
          StrCpy $ExitCode "${ERR_PREINSTALL_CERT_ATTRIBUTES}"
        ${EndIf}
      ${EndIf}

      System::Call "kernel32::GetTickCount()l .s"
      Pop $EndPreInstallPhaseTickCount

      ${If} $0 == 0
      ${OrIf} $1 == 0
        ; Use a timer so the UI has a chance to update
        ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
        Return
      ${EndIf}

      ; Instead of extracting the files we use the downloaded installer to
      ; install in case it needs to perform operations that the stub doesn't
      ; know about.
      WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "InstallDirectoryPath" "$INSTDIR"
      ; Don't create the QuickLaunch or Taskbar shortcut from the launched installer
      WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "QuickLaunchShortcut" "false"
      ${If} $CheckboxShortcutOnDesktop == 1
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "DesktopShortcut" "true"
      ${Else}
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "DesktopShortcut" "false"
      ${EndIf}

      ${If} $CheckboxShortcutInStartMenu == 1
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "StartMenuShortcuts" "true"
      ${Else}
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "StartMenuShortcuts" "false"
      ${EndIf}

!ifdef MOZ_MAINTENANCE_SERVICE
      ${If} $CheckboxInstallMaintSvc == 1
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "MaintenanceService" "true"
      ${Else}
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "MaintenanceService" "false"
      ${EndIf}
!else
      WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "MaintenanceService" "false"
!endif

      ; Write migrated to the shortcuts.ini file to prevent the installer
      ; from creating a taskbar shortcut (Bug 791613).
      ${GetShortcutsLogPath} $0
      Delete "$0"
      ; Workaround to prevent pinning to the taskbar.
      ${If} $CheckboxShortcutOnBar == 0
        WriteIniStr "$0" "TASKBAR" "Migrated" "true"
      ${EndIf}

      ${OnStubInstallUninstall} $Progressbar $InstallCounterStep

      ; Delete the install.log and let the full installer create it. When the
      ; installer closes it we can detect that it has completed.
      Delete "$INSTDIR\install.log"

      ; Delete firefox.exe.moz-upgrade if it exists since it being present will
      ; require an OS restart for the full installer.
      Delete "$INSTDIR\${FileMainEXE}.moz-upgrade"

      System::Call "kernel32::GetTickCount()l .s"
      Pop $EndPreInstallPhaseTickCount

      Exec "$\"$PLUGINSDIR\download.exe$\" /INI=$PLUGINSDIR\${CONFIG_INI}"
      ${NSD_CreateTimer} CheckInstall ${InstallIntervalMS}
    ${Else}
      ${If} $HalfOfDownload != "true"
      ${AndIf} $3 > $HalfOfDownload
        StrCpy $HalfOfDownload "true"
        LockWindow on
        ShowWindow $LabelBlurb1 ${SW_HIDE}
        ShowWindow $BitmapBlurb1 ${SW_HIDE}
        ShowWindow $LabelBlurb2 ${SW_SHOW}
        ShowWindow $BitmapBlurb2 ${SW_SHOW}
        LockWindow off
      ${EndIf}
      StrCpy $DownloadedBytes "$3"
      SendMessage $Progressbar ${PBM_SETPOS} $3 0
    ${EndIf}
  ${EndIf}
FunctionEnd

Function OnPing
  InetBgDL::GetStats
  # $0 = HTTP status code, 0=Completed
  # $1 = Completed files
  # $2 = Remaining files
  # $3 = Number of downloaded bytes for the current file
  # $4 = Size of current file (Empty string if the size is unknown)
  # /RESET must be used if status $0 > 299 (e.g. failure)
  # When status is $0 =< 299 it is handled by InetBgDL
  ${If} $2 == 0
  ${OrIf} $0 > 299
    ${NSD_KillTimer} OnPing
    ${If} $0 > 299
      InetBgDL::Get /RESET /END
    ${EndIf}
    ; The following will exit the installer
    SetAutoClose true
    StrCpy $R9 "2"
    Call RelativeGotoPage
  ${EndIf}
FunctionEnd

Function CheckInstall
  IntOp $InstallCounterStep $InstallCounterStep + 1
  ${If} $InstallCounterStep >= $InstallTotalSteps
    ${NSD_KillTimer} CheckInstall
    ; Close the handle that prevents modification of the full installer
    System::Call 'kernel32::CloseHandle(i $HandleDownload)'
    StrCpy $ExitCode "${ERR_INSTALL_TIMEOUT}"
    ; Use a timer so the UI has a chance to update
    ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
    Return
  ${EndIf}

  SendMessage $Progressbar ${PBM_STEPIT} 0 0

  ${If} ${FileExists} "$INSTDIR\install.log"
    Delete "$INSTDIR\install.tmp"
    CopyFiles /SILENT "$INSTDIR\install.log" "$INSTDIR\install.tmp"

    ; The unfocus and refocus that happens approximately here is caused by the
    ; installer calling SHChangeNotify to refresh the shortcut icons.

    ; When the full installer completes the installation the install.log will no
    ; longer be in use.
    ClearErrors
    Delete "$INSTDIR\install.log"
    ${Unless} ${Errors}
      ${NSD_KillTimer} CheckInstall
      ; Close the handle that prevents modification of the full installer
      System::Call 'kernel32::CloseHandle(i $HandleDownload)'
      Rename "$INSTDIR\install.tmp" "$INSTDIR\install.log"
      Delete "$PLUGINSDIR\download.exe"
      Delete "$PLUGINSDIR\${CONFIG_INI}"
      System::Call "kernel32::GetTickCount()l .s"
      Pop $EndInstallPhaseTickCount
      System::Int64Op $InstallStepSize * ${InstallProgressFinishStep}
      Pop $InstallStepSize
      SendMessage $Progressbar ${PBM_SETSTEP} $InstallStepSize 0
      ${NSD_CreateTimer} FinishInstall ${InstallIntervalMS}
    ${EndUnless}
  ${EndIf}
FunctionEnd

Function FinishInstall
  ; The full installer has completed but the progress bar still needs to finish
  ; so increase the size of the step.
  IntOp $InstallCounterStep $InstallCounterStep + ${InstallProgressFinishStep}
  ${If} $InstallTotalSteps < $InstallCounterStep
    StrCpy $InstallCounterStep "$InstallTotalSteps"
  ${EndIf}

  ${If} $InstallTotalSteps != $InstallCounterStep
    SendMessage $Progressbar ${PBM_STEPIT} 0 0
    Return
  ${EndIf}

  ${NSD_KillTimer} FinishInstall

  SendMessage $Progressbar ${PBM_GETRANGE} 0 0 $R9
  SendMessage $Progressbar ${PBM_SETPOS} $R9 0

  ${If} "$CheckboxSetAsDefault" == "1"
    ${GetParameters} $0
    ClearErrors
    ${GetOptions} "$0" "/UAC:" $0
    ${If} ${Errors} ; Not elevated
      Call ExecSetAsDefaultAppUser
    ${Else} ; Elevated - execute the function in the unelevated process
      GetFunctionAddress $0 ExecSetAsDefaultAppUser
      UAC::ExecCodeSegment $0
    ${EndIf}
  ${EndIf}

  ${If} $CheckboxShortcutOnBar == 1
    ${If} ${AtMostWinVista}
      ClearErrors
      ${GetParameters} $0
      ClearErrors
      ${GetOptions} "$0" "/UAC:" $0
      ${If} ${Errors}
        Call AddQuickLaunchShortcut
      ${Else}
        GetFunctionAddress $0 AddQuickLaunchShortcut
        UAC::ExecCodeSegment $0
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}.moz-upgrade"
    Delete "$INSTDIR\${FileMainEXE}"
    Rename "$INSTDIR\${FileMainEXE}.moz-upgrade" "$INSTDIR\${FileMainEXE}"
  ${EndIf}

  StrCpy $ExitCode "${ERR_SUCCESS}"

  StrCpy $InstallCounterStep 0
  ${NSD_CreateTimer} FinishProgressBar ${InstallIntervalMS}
FunctionEnd

Function FinishProgressBar
  IntOp $InstallCounterStep $InstallCounterStep + 1

  ${If} $InstallCounterStep < 10
    Return
  ${EndIf}

  ${NSD_KillTimer} FinishProgressBar

  Call LaunchApp

  Call SendPing
FunctionEnd

Function OnBack
  StrCpy $WasOptionsButtonClicked "1"
  StrCpy $R9 "1" ; Goto the next page
  Call RelativeGotoPage
  ; The call to Abort prevents NSIS from trying to move to the previous or the
  ; next page.
  Abort
FunctionEnd

Function RelativeGotoPage
  IntCmp $R9 0 0 Move Move
  StrCmp $R9 "X" 0 Move
  StrCpy $R9 "120"

  Move:
  SendMessage $HWNDPARENT "0x408" "$R9" ""
FunctionEnd

Function UpdateFreeSpaceLabel
  ; Only update when $ExistingTopDir isn't set
  ${If} "$ExistingTopDir" != ""
    StrLen $5 "$ExistingTopDir"
    StrLen $6 "$INSTDIR"
    ${If} $5 <= $6
      StrCpy $7 "$INSTDIR" $5
      ${If} "$7" == "$ExistingTopDir"
        Return
      ${EndIf}
    ${EndIf}
  ${EndIf}

  Call CheckSpace

  StrCpy $0 "$SpaceAvailableBytes"

  StrCpy $1 "$(BYTE)"

  ${If} $0 > 1024
  ${OrIf} $0 < 0
    ; Multiply by 10 so it is possible to display a decimal in the size
    System::Int64Op $0 * 10
    Pop $0
    System::Int64Op $0 / 1024
    Pop $0
    StrCpy $1 "$(KILO)$(BYTE)"
    ${If} $0 > 10240
    ${OrIf} $0 < 0
      System::Int64Op $0 / 1024
      Pop $0
      StrCpy $1 "$(MEGA)$(BYTE)"
      ${If} $0 > 10240
      ${OrIf} $0 < 0
        System::Int64Op $0 / 1024
        Pop $0
        StrCpy $1 "$(GIGA)$(BYTE)"
      ${EndIf}
    ${EndIf}
    StrLen $3 "$0"
    ${If} $3 > 1
      StrCpy $2 "$0" -1 ; All characters except the last one
      StrCpy $0 "$0" "" -1 ; The last character
      ${If} "$0" == "0"
        StrCpy $0 "$2" ; Don't display the decimal if it is 0
      ${Else}
        StrCpy $0 "$2.$0"
      ${EndIf}
    ${ElseIf} $3 == 1
      StrCpy $0 "0.$0"
    ${Else}
      ; This should never happen
      System::Int64Op $0 / 10
      Pop $0
    ${EndIf}
  ${EndIf}

  SendMessage $LabelFreeSpace ${WM_SETTEXT} 0 "STR:$0 $1"

FunctionEnd

Function OnChange_DirRequest
  Pop $0
  System::Call 'user32::GetWindowTextW(i $DirRequest, w .r0, i ${NSIS_MAX_STRLEN})'
  StrCpy $1 "$0" 1 ; the first character
  ${If} "$1" == "$\""
    StrCpy $1 "$0" "" -1 ; the last character
    ${If} "$1" == "$\""
      StrCpy $0 "$0" "" 1 ; all but the first character
      StrCpy $0 "$0" -1 ; all but the last character
    ${EndIf}
  ${EndIf}

  StrCpy $INSTDIR "$0"
  Call UpdateFreeSpaceLabel

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(UPGRADE_BUTTON)"
  ${Else}
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(INSTALL_BUTTON)"
  ${EndIf}
FunctionEnd

Function OnClick_ButtonBrowse
  StrCpy $0 "$INSTDIR"
  nsDialogs::SelectFolderDialog /NOUNLOAD "$(SELECT_FOLDER_TEXT)" $0
  Pop $0
  ${If} $0 == "error" ; returns 'error' if 'cancel' was pressed?
    Return
  ${EndIf}

  ${If} $0 != ""
    StrCpy $INSTDIR "$0"
    System::Call 'user32::SetWindowTextW(i $DirRequest, w "$INSTDIR")'
  ${EndIf}
FunctionEnd

Function CheckSpace
  ${If} "$ExistingTopDir" != ""
    StrLen $0 "$ExistingTopDir"
    StrLen $1 "$INSTDIR"
    ${If} $0 <= $1
      StrCpy $2 "$INSTDIR" $3
      ${If} "$2" == "$ExistingTopDir"
        Return
      ${EndIf}
    ${EndIf}
  ${EndIf}

  StrCpy $ExistingTopDir "$INSTDIR"
  ${DoUntil} ${FileExists} "$ExistingTopDir"
    ${GetParent} "$ExistingTopDir" $ExistingTopDir
    ${If} "$ExistingTopDir" == ""
      StrCpy $SpaceAvailableBytes "0"
      StrCpy $HasRequiredSpaceAvailable "false"
      Return
    ${EndIf}
  ${Loop}

  ${GetLongPath} "$ExistingTopDir" $ExistingTopDir

  ; GetDiskFreeSpaceExW requires a backslash.
  StrCpy $0 "$ExistingTopDir" "" -1 ; the last character
  ${If} "$0" != "\"
    StrCpy $0 "\"
  ${Else}
    StrCpy $0 ""
  ${EndIf}

  System::Call 'kernel32::GetDiskFreeSpaceExW(w, *l, *l, *l) i("$ExistingTopDir$0", .r1, .r2, .r3) .'
  StrCpy $SpaceAvailableBytes "$1"

  System::Int64Op $SpaceAvailableBytes / 1048576
  Pop $1
  System::Int64Op $1 > ${APPROXIMATE_REQUIRED_SPACE_MB}
  Pop $1
  ${If} $1 == 1
    StrCpy $HasRequiredSpaceAvailable "true"
  ${Else}
    StrCpy $HasRequiredSpaceAvailable "false"
  ${EndIf}
FunctionEnd

Function CanWrite
  StrCpy $CanWriteToInstallDir "false"

  StrCpy $0 "$INSTDIR"
  ; Use the existing directory when it exists
  ${Unless} ${FileExists} "$INSTDIR"
    ; Get the topmost directory that exists for new installs
    ${DoUntil} ${FileExists} "$0"
      ${GetParent} "$0" $0
      ${If} "$0" == ""
        Return
      ${EndIf}
    ${Loop}
  ${EndUnless}

  GetTempFileName $2 "$0"
  Delete $2
  CreateDirectory "$2"
  ${If} ${FileExists} "$2"
    ${If} ${FileExists} "$INSTDIR"
      GetTempFileName $3 "$INSTDIR"
    ${Else}
      GetTempFileName $3 "$2"
    ${EndIf}
    ${If} ${FileExists} "$3"
      Delete "$3"
      StrCpy $CanWriteToInstallDir "true"
    ${EndIf}
    RmDir "$2"
  ${EndIf}
FunctionEnd

Function AddQuickLaunchShortcut
  CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}"
  ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    ShellLink::SetShortCutWorkingDirectory "$QUICKLAUNCH\${BrandFullName}.lnk" \
                                           "$INSTDIR"
  ${EndIf}
FunctionEnd

Function ExecSetAsDefaultAppUser
  ; Using the helper.exe lessens the stub installer size.
  ; This could ask for elevatation when the user doesn't install as admin.
  Exec "$\"$INSTDIR\uninstall\helper.exe$\" /SetAsDefaultAppUser"
FunctionEnd

Function LaunchApp
  FindWindow $0 "${WindowClass}"
  ${If} $0 <> 0 ; integer comparison
    StrCpy $FirefoxLaunchCode "1"
    MessageBox MB_OK|MB_ICONQUESTION "$(WARN_MANUALLY_CLOSE_APP_LAUNCH)"
    Return
  ${EndIf}

  StrCpy $FirefoxLaunchCode "2"

  ; Set the current working directory to the installation directory
  SetOutPath "$INSTDIR"
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $1
  ${If} ${Errors}
    Exec "$\"$INSTDIR\${FileMainEXE}$\""
  ${Else}
    GetFunctionAddress $0 LaunchAppFromElevatedProcess
    UAC::ExecCodeSegment $0
  ${EndIf}
FunctionEnd

Function LaunchAppFromElevatedProcess
  ; Find the installation directory when launching using GetFunctionAddress
  ; from an elevated installer since $INSTDIR will not be set in this installer
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${GetPathFromString} "$0" $0
  ; Set the current working directory to the installation directory
  ${GetParent} "$0" $1
  SetOutPath "$1"
  Exec "$\"$0$\""
FunctionEnd

Function DisplayDownloadError
  ${NSD_KillTimer} DisplayDownloadError
  MessageBox MB_OKCANCEL|MB_ICONSTOP "$(ERROR_DOWNLOAD)" IDCANCEL +2 IDOK +1
  StrCpy $OpenedDownloadPage "1" ; Already initialized to 0

  ${If} "$OpenedDownloadPage" == "1"
    ClearErrors
    ${GetParameters} $0
    ${GetOptions} "$0" "/UAC:" $1
    ${If} ${Errors}
      Call OpenManualDownloadURL
    ${Else}
      GetFunctionAddress $0 OpenManualDownloadURL
      UAC::ExecCodeSegment $0
    ${EndIf}
  ${EndIf}

  Call SendPing
FunctionEnd

Function OpenManualDownloadURL
  ExecShell "open" "${URLManualDownload}${URLManualDownloadAppend}"
FunctionEnd

Section
SectionEnd
