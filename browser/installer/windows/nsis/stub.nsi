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

Unicode true
ManifestSupportedOS all
ManifestDPIAware true

!addplugindir ./

Var Dialog
Var Progressbar
Var ProgressbarMarqueeIntervalMS
Var CheckboxSetAsDefault
Var CheckboxShortcuts
Var CheckboxSendPing
Var CheckboxInstallMaintSvc
Var DroplistArch
Var LabelBlurb
Var BgBitmapImage
Var HwndBgBitmapControl
Var CurrentBlurbIdx
Var CheckboxCleanupProfile

Var FontInstalling
Var FontBlurb
Var FontFooter
Var FontCheckbox

Var CanWriteToInstallDir
Var HasRequiredSpaceAvailable
Var IsDownloadFinished
Var DownloadSizeBytes
Var DownloadReset
Var ExistingTopDir
Var SpaceAvailableBytes
Var InitialInstallDir
Var HandleDownload
Var CanSetAsDefault
Var InstallCounterStep
Var InstallTotalSteps
Var ProgressCompleted

Var ExitCode
Var FirefoxLaunchCode

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
Var PostSigningData
Var PreviousInstallDir
Var PreviousInstallArch
Var ProfileCleanupPromptType
Var ProfileCleanupHeaderString
Var ProfileCleanupButtonString
Var AppLaunchWaitTickCount

; Uncomment the following to prevent pinging the metrics server when testing
; the stub installer
;!define STUB_DEBUG

!define StubURLVersion "v8"

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
!define DownloadMaxSizeBytes 73400320 ; 70 MB

; Interval before retrying to download. 3 seconds is used along with 10
; attempted downloads (the first attempt along with 9 retries) to give a
; minimum of 30 seconds or retrying before giving up.
!define DownloadRetryIntervalMS 3000

; Interval for the download timer
!define DownloadIntervalMS 200

; Interval for the install timer
!define InstallIntervalMS 100

; Number of steps for the install progress.
; This might not be enough when installing on a slow network drive so it will
; fallback to downloading the full installer if it reaches this number.

; Approximately 150 seconds with a 100 millisecond timer.
!define InstallCleanTotalSteps 1500

; Approximately 165 seconds with a 100 millisecond timer.
!define InstallPaveOverTotalSteps 1650

; Blurb duty cycle
!define BlurbDisplayMS 19500
!define BlurbBlankMS 500

; Interval between checks for the application window and progress bar updates.
!define AppLaunchWaitIntervalMS 100

; Total time to wait for the application to start before just exiting.
!define AppLaunchWaitTimeoutMS 10000

; Maximum value of the download/install/launch progress bar, and the end values
; for each individual stage.
!define PROGRESS_BAR_TOTAL_STEPS 500 
!define PROGRESS_BAR_DOWNLOAD_END_STEP 300
!define PROGRESS_BAR_INSTALL_END_STEP 475
!define PROGRESS_BAR_APP_LAUNCH_END_STEP 500

; Amount of physical memory required for the 64-bit build to be selected (2 GB).
; Machines with this or less RAM get the 32-bit build, even with a 64-bit OS.
!define RAM_NEEDED_FOR_64BIT 0x80000000

; Attempt to elevate Standard Users in addition to users that
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

!define DefaultInstDir32bit "$PROGRAMFILES32\${BrandFullName}"
!define DefaultInstDir64bit "$PROGRAMFILES64\${BrandFullName}"

!include "nsDialogs.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "TextFunc.nsh"
!include "WinVer.nsh"
!include "WordFunc.nsh"

!insertmacro GetParameters
!insertmacro GetOptions
!insertmacro LineFind
!insertmacro StrFilter

!include "locales.nsi"
!include "branding.nsi"

!include "defines.nsi"

; Must be included after defines.nsi
!include "locale-fonts.nsh"

; The OFFICIAL define is a workaround to support different urls for Release and
; Beta since they share the same branding when building with other branches that
; set the update channel to beta.
!ifdef OFFICIAL
!ifdef BETA_UPDATE_CHANNEL
!undef URLStubDownload32
!undef URLStubDownload64
!define URLStubDownload32 "http://download.mozilla.org/?os=win&lang=${AB_CD}&product=firefox-beta-latest"
!define URLStubDownload64 "http://download.mozilla.org/?os=win64&lang=${AB_CD}&product=firefox-beta-latest"
!undef URLManualDownload
!define URLManualDownload "https://www.mozilla.org/${AB_CD}/firefox/installer-help/?channel=beta&installer_lang=${AB_CD}"
!undef Channel
!define Channel "beta"
!endif
!endif

!undef INSTALL_BLURB_TEXT_COLOR
!define INSTALL_BLURB_TEXT_COLOR 0xFFFFFF

!include "common.nsh"

!insertmacro ElevateUAC
!insertmacro GetLongPath
!insertmacro GetPathFromString
!insertmacro GetParent
!insertmacro GetSingleInstallPath
!insertmacro GetTextWidthHeight
!insertmacro IsUserAdmin
!insertmacro RemovePrecompleteEntries
!insertmacro SetBrandNameVars
!insertmacro ITBL3Create
!insertmacro UnloadUAC

VIAddVersionKey "FileDescription" "${BrandShortName} Installer"
VIAddVersionKey "OriginalFilename" "setup-stub.exe"

Name "$BrandFullName"
OutFile "setup-stub.exe"
Icon "firefox64.ico"
XPStyle on
BrandingText " "
ChangeUI all "nsisui.exe"

!ifdef ${AB_CD}_rtl
  LoadLanguageFile "locale-rtl.nlf"
!else
  LoadLanguageFile "locale.nlf"
!endif

!include "nsisstrings.nlf"

Caption "$(INSTALLER_WIN_CAPTION)"

Page custom createProfileCleanup
Page custom createInstall ; Download / Installation page

Function .onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  StrCpy $LANGUAGE 0
  ; This macro is used to set the brand name variables but the ini file method
  ; isn't supported for the stub installer.
  ${SetBrandNameVars} "$PLUGINSDIR\ignored.ini"

  ; Don't install on systems that don't support SSE2. The parameter value of
  ; 10 is for PF_XMMI64_INSTRUCTIONS_AVAILABLE which will check whether the
  ; SSE2 instruction set is available.
  System::Call "kernel32::IsProcessorFeaturePresent(i 10)i .R7"

  ; Windows NT 6.0 (Vista/Server 2008) and lower are not supported.
  ${Unless} ${AtLeastWin7}
    ${If} "$R7" == "0"
      strCpy $R7 "$(WARN_MIN_SUPPORTED_OSVER_CPU_MSG)"
    ${Else}
      strCpy $R7 "$(WARN_MIN_SUPPORTED_OSVER_MSG)"
    ${EndIf}
    MessageBox MB_OKCANCEL|MB_ICONSTOP "$R7" IDCANCEL +2
    ExecShell "open" "${URLSystemRequirements}"
    Quit
  ${EndUnless}

  ; SSE2 CPU support
  ${If} "$R7" == "0"
    MessageBox MB_OKCANCEL|MB_ICONSTOP "$(WARN_MIN_SUPPORTED_CPU_MSG)" IDCANCEL +2
    ExecShell "open" "${URLSystemRequirements}"
    Quit
  ${EndIf}

  ; Check if we meet the RAM requirement for the 64-bit build.
  System::Call "*(i 64, i, l 0, l, l, l, l, l, l)p.r0"
  System::Call "Kernel32::GlobalMemoryStatusEx(p r0)"
  System::Call "*$0(i, i, l.r1, l, l, l, l, l, l)"
  System::Free $0

  ${If} ${RunningX64}
  ${AndIf} $1 L> ${RAM_NEEDED_FOR_64BIT}
    StrCpy $DroplistArch "$(VERSION_64BIT)"
    StrCpy $INSTDIR "${DefaultInstDir64bit}"
  ${Else}
    StrCpy $DroplistArch "$(VERSION_32BIT)"
    StrCpy $INSTDIR "${DefaultInstDir32bit}"
  ${EndIf}

  ; Require elevation if the user can elevate
  ${ElevateUAC}

  ; If we have any existing installation, use its location as the default
  ; path for this install, even if it's not the same architecture.
  SetRegView 32
  SetShellVarContext all ; Set SHCTX to HKLM
  ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9

  ${If} "$R9" == "false"
  ${AndIf} ${RunningX64}
    SetRegView 64
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9
  ${EndIf}

  ${If} "$R9" == "false"
    SetShellVarContext current ; Set SHCTX to HKCU
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9

    ${If} ${RunningX64}
      ; In HKCU there is no WOW64 redirection, which means we may have gotten
      ; the path to a 32-bit install even though we're 64-bit.
      ; In that case, just use the default path instead of offering an upgrade.
      ; But only do that override if the existing install is in Program Files,
      ; because that's the only place we can be sure is specific
      ; to either 32 or 64 bit applications.
      ; The WordFind syntax below searches for the first occurence of the
      ; "delimiter" (the Program Files path) in the install path and returns
      ; anything that appears before that. If nothing appears before that,
      ; then the install is under Program Files.
      ${WordFind} $R9 $PROGRAMFILES32 "+1{" $0
      ${If} $0 == ""
        StrCpy $R9 "false"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  StrCpy $PreviousInstallDir ""
  StrCpy $PreviousInstallArch ""
  ${If} "$R9" != "false"
    ; Don't override the default install path with an existing installation
    ; of a different architecture.
    System::Call "*(i)p.r0"
    StrCpy $1 "$R9\${FileMainEXE}"
    System::Call "Kernel32::GetBinaryTypeW(w r1, p r0)i"
    System::Call "*$0(i.r2)"
    System::Free $0

    ${If} $2 == "6" ; 6 == SCS_64BIT_BINARY
    ${AndIf} ${RunningX64}
      StrCpy $PreviousInstallDir "$R9"
      StrCpy $PreviousInstallArch "64"
      StrCpy $INSTDIR "$PreviousInstallDir"
    ${ElseIf} $2 == "0" ; 0 == SCS_32BIT_BINARY
    ${AndIfNot} ${RunningX64}
      StrCpy $PreviousInstallDir "$R9"
      StrCpy $PreviousInstallArch "32"
      StrCpy $INSTDIR "$PreviousInstallDir"
    ${EndIf}
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
  ${Else}
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    StrCpy $CanSetAsDefault "true"
  ${EndIf}
  StrCpy $CheckboxSetAsDefault "0"

  ; The interval in MS used for the progress bars set as marquee.
  StrCpy $ProgressbarMarqueeIntervalMS "10"

  ; Initialize the majority of variables except those that need to be reset
  ; when a page is displayed.
  StrCpy $ExitCode "${ERR_DOWNLOAD_CANCEL}"
  StrCpy $IntroPhaseSeconds "0"
  StrCpy $OptionsPhaseSeconds "0"
  StrCpy $EndPreInstallPhaseTickCount "0"
  StrCpy $EndInstallPhaseTickCount "0"
  StrCpy $StartDownloadPhaseTickCount "0"
  StrCpy $EndDownloadPhaseTickCount "0"
  StrCpy $InitialInstallRequirementsCode ""
  StrCpy $IsDownloadFinished ""
  StrCpy $FirefoxLaunchCode "0"
  StrCpy $CheckboxShortcuts "1"
  StrCpy $CheckboxSendPing "1"
  StrCpy $CheckboxCleanupProfile "0"
!ifdef MOZ_MAINTENANCE_SERVICE
  ; We can only install the maintenance service if the user is an admin.
  Call IsUserAdmin
  Pop $0
  ${If} "$0" == "true"
    StrCpy $CheckboxInstallMaintSvc "1"
  ${Else}
    StrCpy $CheckboxInstallMaintSvc "0"
  ${EndIf}
!else
  StrCpy $CheckboxInstallMaintSvc "0"
!endif

  StrCpy $0 ""
!ifdef FONT_FILE1
  ${If} ${FileExists} "$FONTS\${FONT_FILE1}"
    StrCpy $0 "${FONT_NAME1}"
  ${EndIf}
!endif

!ifdef FONT_FILE2
  ${If} $0 == ""
  ${AndIf} ${FileExists} "$FONTS\${FONT_FILE2}"
    StrCpy $0 "${FONT_NAME2}"
  ${EndIf}
!endif

  ${If} $0 == ""
    StrCpy $0 "$(^Font)"
  ${EndIf}

  CreateFont $FontInstalling "$0" "28" "400"
  CreateFont $FontBlurb      "$0" "15" "400"
  CreateFont $FontFooter     "$0" "13" "400"
  CreateFont $FontCheckbox   "$0" "10" "400"

  InitPluginsDir
  File /oname=$PLUGINSDIR\bgstub.jpg "bgstub.jpg"
  File /oname=$PLUGINSDIR\bgstub_2x.jpg "bgstub_2x.jpg"

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

  Call CanWrite
  ${If} "$CanWriteToInstallDir" == "false"
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_WRITE_ACCESS_QUIT)\n\n$INSTDIR"
    Quit
  ${EndIf}

  Call CheckSpace
  ${If} "$HasRequiredSpaceAvailable" == "false"
    MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_DISK_SPACE_QUIT)"
    Quit
  ${EndIf}
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
  ${NSD_KillTimer} DisplayDownloadError
  ${NSD_KillTimer} NextBlurb
  ${NSD_KillTimer} ClearBlurb

  ${If} "$IsDownloadFinished" != ""
    Call DisplayDownloadError
  ${Else}
    Call SendPing
  ${EndIf}

  ; Aborting the abort will allow SendPing which is called by
  ; DisplayDownloadError to hide the installer window and close the installer
  ; after it sends the metrics ping.
  Abort
FunctionEnd

Function DrawBackgroundImage
  ${NSD_CreateBitmap} 0 0 100% 100% ""
  Pop $HwndBgBitmapControl

  ; If the scaling factor is 100%, use the 1x image; the 2x images scaled down
  ; by that much don't always look good because some of them use thinner lines
  ; and a darker background (over which aliasing is more visible).
  System::Call 'user32::GetWindowDC(i $HWNDPARENT) i .r0'
  System::Call 'gdi32::GetDeviceCaps(i $0, i 88) i .r1' ; 88 = LOGPIXELSX
  System::Call 'user32::ReleaseDC(i $HWNDPARENT, i $0)'
  ${If} $1 <= 96
    ${SetStretchedImageOLE} $HwndBgBitmapControl $PLUGINSDIR\bgstub.jpg $BgBitmapImage
  ${Else}
    ${SetStretchedImageOLE} $HwndBgBitmapControl $PLUGINSDIR\bgstub_2x.jpg $BgBitmapImage
  ${EndIf}

  ; transparent bg on control prevents flicker on redraw
  SetCtlColors $HwndBgBitmapControl ${INSTALL_BLURB_TEXT_COLOR} transparent
FunctionEnd

Function createProfileCleanup
  Call ShouldPromptForProfileCleanup
  ${Select} $ProfileCleanupPromptType
  ${Case} 0
    StrCpy $CheckboxCleanupProfile 0
    Abort ; Skip this page
  ${Case} 1
    StrCpy $ProfileCleanupHeaderString $(STUB_CLEANUP_REINSTALL_HEADER)
    StrCpy $ProfileCleanupButtonString $(STUB_CLEANUP_REINSTALL_BUTTON)
  ${Case} 2
    StrCpy $ProfileCleanupHeaderString $(STUB_CLEANUP_PAVEOVER_HEADER)
    StrCpy $ProfileCleanupButtonString $(STUB_CLEANUP_PAVEOVER_BUTTON)
  ${EndSelect}

  nsDialogs::Create /NOUNLOAD 1018
  Pop $Dialog

  SetCtlColors $HWNDPARENT ${FOOTER_CONTROL_TEXT_COLOR_NORMAL} ${FOOTER_BKGRD_COLOR}

  ; Since the text color for controls is set in this Dialog the foreground and
  ; background colors of the Dialog must also be hardcoded.
  SetCtlColors $Dialog ${COMMON_TEXT_COLOR_NORMAL} ${COMMON_BKGRD_COLOR}

  FindWindow $7 "#32770" "" $HWNDPARENT
  ${GetDlgItemWidthHeight} $HWNDPARENT $8 $9

  ; Resize the Dialog to fill the entire window
  System::Call 'user32::MoveWindow(i$Dialog,i0,i0,i $8,i $9,i0)'

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  ShowWindow $0 ${SW_HIDE}
  EnableWindow $0 0

  GetDlgItem $0 $HWNDPARENT 3 ; Back button
  ShowWindow $0 ${SW_HIDE}
  EnableWindow $0 0

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  ; Hide the Cancel button, but don't disable it (or else it won't be possible
  ; to close the window)
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 10 ; Default browser checkbox
  ; Hiding and then disabling allows Esc to still exit the installer
  ShowWindow $0 ${SW_HIDE}
  EnableWindow $0 0

  GetDlgItem $0 $HWNDPARENT 11 ; Footer text
  ShowWindow $0 ${SW_HIDE}
  EnableWindow $0 0

  ${GetDlgItemWidthHeight} $HWNDPARENT $R1 $R2
  ${GetTextWidthHeight} $ProfileCleanupHeaderString $FontInstalling $R1 $R1 $R2
  ${NSD_CreateLabelCenter} 0 ${PROFILE_CLEANUP_LABEL_TOP_DU} 100% $R2 \
    $ProfileCleanupHeaderString
  Pop $0
  SendMessage $0 ${WM_SETFONT} $FontInstalling 0
  SetCtlColors $0 ${INSTALL_BLURB_TEXT_COLOR} transparent

  ${GetDlgItemBottomDU} $Dialog $0 $1
  IntOp $1 $1 + 10 ; add a bit of padding between the header and the button
  ${GetTextExtent} $ProfileCleanupButtonString $FontFooter $R1 $R2
  ; Add some padding to both dimensions of the button.
  IntOp $R1 $R1 + 100
  IntOp $R2 $R2 + 10
  ; Now that we know the size and the Y coordinate for the button, we can find
  ; the correct X coordinate to get it properly centered.
  ${GetDlgItemWidthHeight} $HWNDPARENT $R3 $R4
  IntOp $R5 $R1 / 2
  IntOp $R3 $R3 / 2
  IntOp $R3 $R3 - $R5
  ; We need a custom button because the default ones get drawn underneath the
  ; background image we're about to insert.
  ${NSD_CreateButton} $R3 $1 $R1 $R2 $ProfileCleanupButtonString
  Pop $0
  SendMessage $0 ${WM_SETFONT} $FontFooter 0
  ${NSD_OnClick} $0 gotoInstallPage
  ${NSD_SetFocus} $0

  ; For the checkbox, first we need to know the width of the checkbox itself,
  ; since it can vary with the display scaling and the theme.
  System::Call 'User32::GetSystemMetrics(i 71) i .r1' ; 71 == SM_CXMENUCHECK
  ; Now get the width of the label test, if it were all on one line.
  ${GetTextExtent} $(STUB_CLEANUP_CHECKBOX_LABEL) $FontCheckbox $R1 $R2
  ${GetDlgItemWidthHeight} $HWNDPARENT $R3 $R4
  ; Add the checkbox width to the text width, then figure out how many lines
  ; we're going to need in order to display that text in our dialog.
  IntOp $R1 $R1 + $1
  IntOp $R1 $R1 + 5
  StrCpy $R5 $R1
  StrCpy $R6 $R2
  IntOp $R3 $R3 - 150 ; leave some padding on the sides of the dialog
  ${While} $R1 > $R3
    StrCpy $R5 $R3
    IntOp $R2 $R2 + $R6
    IntOp $R1 $R1 - $R3
  ${EndWhile}
  ${GetDlgItemBottomDU} $Dialog $0 $1
  ; Now that we know the size for the checkbox, center it in the dialog.
  ${GetDlgItemWidthHeight} $HWNDPARENT $R3 $R4
  IntOp $R6 $R5 / 2
  IntOp $R3 $R3 / 2
  IntOp $R3 $R3 - $R6
  IntOp $1 $1 + 20 ; add a bit of padding between the button and the checkbox
  ${NSD_CreateCheckbox} $R3 $1 $R5 $R2 $(STUB_CLEANUP_CHECKBOX_LABEL)
  Pop $CheckboxCleanupProfile
  SendMessage $CheckboxCleanupProfile ${WM_SETFONT} $FontCheckbox 0
  ; The uxtheme must be disabled on checkboxes in order to override the system
  ; colors and have a transparent background.
  System::Call 'uxtheme::SetWindowTheme(i $CheckboxCleanupProfile, w " ", w " ")'
  SetCtlColors $CheckboxCleanupProfile ${INSTALL_BLURB_TEXT_COLOR} transparent
  ; Setting the background color to transparent isn't enough to actually make a
  ; checkbox background transparent, you also have to set the right style.
  ${NSD_AddExStyle} $CheckboxCleanupProfile ${WS_EX_TRANSPARENT}
  ; For some reason, clicking on the checkbox causes its text to be redrawn
  ; one pixel to the side of where it was, but without clearing away the
  ; existing text first, so it looks like the weight increases when you click.
  ; Hack around this by manually hiding and then re-showing the textbox when
  ; it gets clicked on.
  ${NSD_OnClick} $CheckboxCleanupProfile RedrawWindow
  ${NSD_Check} $CheckboxCleanupProfile

  ${GetTextWidthHeight} "$(STUB_BLURB_FOOTER2)" $FontFooter \
    ${INSTALL_FOOTER_WIDTH_DU} $R1 $R2
  !ifdef ${AB_CD}_rtl
    nsDialogs::CreateControl STATIC ${DEFAULT_STYLES}|${SS_NOTIFY} \
      ${WS_EX_TRANSPARENT} 30u ${INSTALL_FOOTER_TOP_DU} ${INSTALL_FOOTER_WIDTH_DU} \
       "$R2u" "$(STUB_BLURB_FOOTER2)"
  !else
    nsDialogs::CreateControl STATIC ${DEFAULT_STYLES}|${SS_NOTIFY}|${SS_RIGHT} \
      ${WS_EX_TRANSPARENT} 175u ${INSTALL_FOOTER_TOP_DU} ${INSTALL_FOOTER_WIDTH_DU} \
      "$R2u" "$(STUB_BLURB_FOOTER2)"
  !endif
  Pop $0
  SendMessage $0 ${WM_SETFONT} $FontFooter 0
  SetCtlColors $0 ${INSTALL_BLURB_TEXT_COLOR} transparent

  Call DrawBackgroundImage

  LockWindow off
  nsDialogs::Show

  ${NSD_FreeImage} $BgBitmapImage
FunctionEnd

Function RedrawWindow
  Pop $0
  ShowWindow $0 ${SW_HIDE}
  ShowWindow $0 ${SW_SHOW}
FunctionEnd

Function gotoInstallPage
  ; Eat the parameter that NSD_OnClick always passes but that we don't need.
  Pop $0

  ; Save the state of the checkbox before it's destroyed.
  ${NSD_GetState} $CheckboxCleanupProfile $CheckboxCleanupProfile

  StrCpy $R9 1
  Call RelativeGotoPage
FunctionEnd

Function createInstall
  ; Begin setting up the download/install window

  nsDialogs::Create /NOUNLOAD 1018
  Pop $Dialog

  SetCtlColors $HWNDPARENT ${FOOTER_CONTROL_TEXT_COLOR_NORMAL} ${FOOTER_BKGRD_COLOR}

  ; Since the text color for controls is set in this Dialog the foreground and
  ; background colors of the Dialog must also be hardcoded.
  SetCtlColors $Dialog ${COMMON_TEXT_COLOR_NORMAL} ${COMMON_BKGRD_COLOR}

  FindWindow $7 "#32770" "" $HWNDPARENT
  ${GetDlgItemWidthHeight} $HWNDPARENT $8 $9

  ; Resize the Dialog to fill the entire window
  System::Call 'user32::MoveWindow(i$Dialog,i0,i0,i $8,i $9,i0)'

  ; The header string may need more than half the width of the window, but it's
  ; currently not close to needing multiple lines in any localization.
  ${NSD_CreateLabelCenter} 0% ${NOW_INSTALLING_TOP_DU} 100% 47u "$(STUB_INSTALLING_LABEL2)"
  Pop $0
  SendMessage $0 ${WM_SETFONT} $FontInstalling 0
  SetCtlColors $0 ${INSTALL_BLURB_TEXT_COLOR} transparent

  ${NSD_CreateLabelCenter} 0% ${INSTALL_BLURB_TOP_DU} 100% 60u "$(STUB_BLURB_FIRST1)"
  Pop $LabelBlurb
  SendMessage $LabelBlurb ${WM_SETFONT} $FontBlurb 0
  SetCtlColors $LabelBlurb ${INSTALL_BLURB_TEXT_COLOR} transparent

  StrCpy $CurrentBlurbIdx "0"

  ${GetTextWidthHeight} "$(STUB_BLURB_FOOTER2)" $FontFooter \
    ${INSTALL_FOOTER_WIDTH_DU} $R1 $R2
  !ifdef ${AB_CD}_rtl
    nsDialogs::CreateControl STATIC ${DEFAULT_STYLES}|${SS_NOTIFY} \
      ${WS_EX_TRANSPARENT} 30u ${INSTALL_FOOTER_TOP_DU} ${INSTALL_FOOTER_WIDTH_DU} "$R2u" \
      "$(STUB_BLURB_FOOTER2)"
  !else
    nsDialogs::CreateControl STATIC ${DEFAULT_STYLES}|${SS_NOTIFY}|${SS_RIGHT} \
      ${WS_EX_TRANSPARENT} 175u ${INSTALL_FOOTER_TOP_DU} ${INSTALL_FOOTER_WIDTH_DU} "$R2u" \
      "$(STUB_BLURB_FOOTER2)"
  !endif
  Pop $0
  SendMessage $0 ${WM_SETFONT} $FontFooter 0
  SetCtlColors $0 ${INSTALL_BLURB_TEXT_COLOR} transparent

  ${NSD_CreateProgressBar} 20% ${PROGRESS_BAR_TOP_DU} 60% 12u ""
  Pop $Progressbar
  ${NSD_AddStyle} $Progressbar ${PBS_MARQUEE}
  SendMessage $Progressbar ${PBM_SETMARQUEE} 1 \
              $ProgressbarMarqueeIntervalMS ; start=1|stop=0 interval(ms)=+N

  Call DrawBackgroundImage

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 3 ; Back button
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  ; Focus the Cancel button otherwise it isn't possible to tab to it since it is
  ; the only control that can be tabbed to.
  ${NSD_SetFocus} $0
  ; Kill the Cancel button's focus so pressing enter won't cancel the install.
  SendMessage $0 ${WM_KILLFOCUS} 0 0
  ; Hide the Cancel button, but don't disable it (or else it won't be possible
  ; to close the window)
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 10 ; Default browser checkbox
  ; Hiding and then disabling allows Esc to still exit the installer
  ShowWindow $0 ${SW_HIDE}
  EnableWindow $0 0

  GetDlgItem $0 $HWNDPARENT 11 ; Footer text
  ShowWindow $0 ${SW_HIDE}
  EnableWindow $0 0

  ; Set $DownloadReset to true so the first download tick count is measured.
  StrCpy $DownloadReset "true"
  StrCpy $IsDownloadFinished "false"
  StrCpy $DownloadRetryCount "0"
  StrCpy $DownloadedBytes "0"
  StrCpy $StartLastDownloadTickCount ""
  StrCpy $DownloadFirstTransferSeconds ""
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

  ${ITBL3Create}
  ${ITBL3SetProgressState} "${TBPF_INDETERMINATE}"

  ; Make sure the file we're about to try to download to doesn't already exist,
  ; so we don't end up trying to "resume" on top of the wrong file.
  Delete "$PLUGINSDIR\download.exe"
  ${NSD_CreateTimer} StartDownload ${DownloadIntervalMS}

  ${NSD_CreateTimer} ClearBlurb ${BlurbDisplayMS}

  LockWindow off
  nsDialogs::Show

  ${NSD_FreeImage} $BgBitmapImage
FunctionEnd

Function StartDownload
  ${NSD_KillTimer} StartDownload
  ${If} $DroplistArch == "$(VERSION_64BIT)"
    InetBgDL::Get "${URLStubDownload64}${URLStubDownloadAppend}" \
                  "$PLUGINSDIR\download.exe" \
                  /CONNECTTIMEOUT 120 /RECEIVETIMEOUT 120 /END
  ${Else}
    InetBgDL::Get "${URLStubDownload32}${URLStubDownloadAppend}" \
                  "$PLUGINSDIR\download.exe" \
                  /CONNECTTIMEOUT 120 /RECEIVETIMEOUT 120 /END
  ${EndIf}
  StrCpy $4 ""
  ${NSD_CreateTimer} OnDownload ${DownloadIntervalMS}
  ${If} ${FileExists} "$INSTDIR\${TO_BE_DELETED}"
    RmDir /r "$INSTDIR\${TO_BE_DELETED}"
  ${EndIf}
FunctionEnd

Function SetProgressBars
  SendMessage $Progressbar ${PBM_SETPOS} $ProgressCompleted 0
  ${ITBL3SetProgressValue} "$ProgressCompleted" "${PROGRESS_BAR_TOTAL_STEPS}"
FunctionEnd

Function NextBlurb
  ${NSD_KillTimer} NextBlurb

  IntOp $CurrentBlurbIdx $CurrentBlurbIdx + 1
  IntOp $CurrentBlurbIdx $CurrentBlurbIdx % 3

  ${If} $CurrentBlurbIdx == "0"
    StrCpy $0 "$(STUB_BLURB_FIRST1)"
  ${ElseIf} $CurrentBlurbIdx == "1"
    StrCpy $0 "$(STUB_BLURB_SECOND1)"
  ${ElseIf} $CurrentBlurbIdx == "2"
    StrCpy $0 "$(STUB_BLURB_THIRD1)"
  ${EndIf}

  SendMessage $LabelBlurb ${WM_SETTEXT} 0 "STR:$0"

  ${NSD_CreateTimer} ClearBlurb ${BlurbDisplayMS}
FunctionEnd

Function ClearBlurb
  ${NSD_KillTimer} ClearBlurb

  SendMessage $LabelBlurb ${WM_SETTEXT} 0 "STR:"

  ; force the background to repaint to clear the transparent label
  System::Call "*(i,i,i,i) p .r0"
  System::Call "user32::GetWindowRect(p $LabelBlurb, p r0)"
  System::Call "user32::MapWindowPoints(p 0, p $HwndBgBitmapControl, p r0, i 2)"
  System::Call "user32::InvalidateRect(p $HwndBgBitmapControl, p r0, i 0)"
  System::Free $0

  ${NSD_CreateTimer} NextBlurb ${BlurbBlankMS}
FunctionEnd

Function OnDownload
  InetBgDL::GetStats
  # $0 = HTTP status code, 0=Completed
  # $1 = Completed files
  # $2 = Remaining files
  # $3 = Number of downloaded bytes for the current file
  # $4 = Size of current file (Empty string if the size is unknown)
  # /RESET must be used if status $0 > 299 (e.g. failure), even if resuming
  # When status is $0 =< 299 it is handled by InetBgDL
  StrCpy $DownloadServerIP "$5"
  ${If} $0 > 299
    ${NSD_KillTimer} OnDownload
    IntOp $DownloadRetryCount $DownloadRetryCount + 1
    ${If} $DownloadRetryCount >= ${DownloadMaxRetries}
      StrCpy $ExitCode "${ERR_DOWNLOAD_TOO_MANY_RETRIES}"
      ; Use a timer so the UI has a chance to update
      ${NSD_CreateTimer} DisplayDownloadError ${InstallIntervalMS}
      Return
    ${EndIf}

    ; 1000 is a special code meaning InetBgDL lost the connection before it got
    ; all the bytes it was expecting. We'll try to resume the transfer in that
    ; case (assuming we aren't out of retries), so don't treat it as a reset
    ; or clear the progress bar.
    ${If} $0 != 1000
      ${If} "$DownloadReset" != "true"
        StrCpy $DownloadedBytes "0"
        ${NSD_AddStyle} $Progressbar ${PBS_MARQUEE}
        SendMessage $Progressbar ${PBM_SETMARQUEE} 1 \
                    $ProgressbarMarqueeIntervalMS ; start=1|stop=0 interval(ms)=+N
        ${ITBL3SetProgressState} "${TBPF_INDETERMINATE}"
      ${EndIf}
      StrCpy $DownloadSizeBytes ""
      StrCpy $DownloadReset "true"
      Delete "$PLUGINSDIR\download.exe"
    ${EndIf}

    InetBgDL::Get /RESET /END
    ${NSD_CreateTimer} StartDownload ${DownloadRetryIntervalMS}
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
    SendMessage $Progressbar ${PBM_SETMARQUEE} 0 0 ; start=1|stop=0 interval(ms)=+N
    ${RemoveStyle} $Progressbar ${PBS_MARQUEE}
    StrCpy $ProgressCompleted 0
    SendMessage $Progressbar ${PBM_SETRANGE32} $ProgressCompleted ${PROGRESS_BAR_TOTAL_STEPS}
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

      ; Update the progress bars first in the UI change so they take affect
      ; before other UI changes.
      StrCpy $ProgressCompleted "${PROGRESS_BAR_DOWNLOAD_END_STEP}"
      Call SetProgressBars

      ; Disable the Cancel button during the install
      GetDlgItem $5 $HWNDPARENT 2
      EnableWindow $5 0

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

      ; Always create a start menu shortcut, so the user always has some way
      ; to access the application.
      WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "StartMenuShortcuts" "true"

      ; Either avoid or force adding a taskbar pin and desktop shortcut
      ; based on the checkbox value.
      ${If} $CheckboxShortcuts == 0
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "TaskbarShortcut" "false"
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "DesktopShortcut" "false"
      ${Else}
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "TaskbarShortcut" "true"
        WriteINIStr "$PLUGINSDIR\${CONFIG_INI}" "Install" "DesktopShortcut" "true"
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

      ; Delete the taskbar shortcut history to ensure we do the right thing based on
      ; the config file above.
      ${GetShortcutsLogPath} $0
      Delete "$0"

      ${RemovePrecompleteEntries} "false"

      ; Delete the install.log and let the full installer create it. When the
      ; installer closes it we can detect that it has completed.
      Delete "$INSTDIR\install.log"

      ; Delete firefox.exe.moz-upgrade and firefox.exe.moz-delete if it exists
      ; since it being present will require an OS restart for the full
      ; installer.
      Delete "$INSTDIR\${FileMainEXE}.moz-upgrade"
      Delete "$INSTDIR\${FileMainEXE}.moz-delete"

      System::Call "kernel32::GetTickCount()l .s"
      Pop $EndPreInstallPhaseTickCount

      Exec "$\"$PLUGINSDIR\download.exe$\" /INI=$PLUGINSDIR\${CONFIG_INI}"
      ${NSD_CreateTimer} CheckInstall ${InstallIntervalMS}
    ${Else}
      StrCpy $DownloadedBytes "$3"
      System::Int64Op $DownloadedBytes * ${PROGRESS_BAR_DOWNLOAD_END_STEP}
      Pop $ProgressCompleted
      System::Int64Op $ProgressCompleted / $DownloadSizeBytes
      Pop $ProgressCompleted
      Call SetProgressBars
    ${EndIf}
  ${EndIf}
FunctionEnd

Function SendPing
  ${NSD_KillTimer} NextBlurb
  ${NSD_KillTimer} ClearBlurb
  HideWindow

  ${If} $CheckboxSendPing == 1
    ; Get the tick count for the completion of all phases.
    System::Call "kernel32::GetTickCount()l .s"
    Pop $EndFinishPhaseTickCount

    ; When the value of $IsDownloadFinished is false the download was started
    ; but didn't finish. In this case the tick count stored in
    ; $EndFinishPhaseTickCount is used to determine how long the download was
    ; in progress.
    ${If} "$IsDownloadFinished" == "false"
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

    ${If} $DroplistArch == "$(VERSION_64BIT)"
      StrCpy $R0 "1"
    ${Else}
      StrCpy $R0 "0"
    ${EndIf}

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
                      $\nDownload Server IP = $DownloadServerIP \
                      $\nPost-Signing Data = $PostSigningData \
                      $\nProfile cleanup prompt shown = $ProfileCleanupPromptType \
                      $\nDid profile cleanup = $CheckboxCleanupProfile"
    ; The following will exit the installer
    SetAutoClose true
    StrCpy $R9 "2"
    Call RelativeGotoPage
!else
    ${NSD_CreateTimer} OnPing ${DownloadIntervalMS}
    InetBgDL::Get "${BaseURLStubPing}/${StubURLVersion}${StubURLVersionAppend}/${Channel}/${UpdateChannel}/${AB_CD}/$R0/$R1/$5/$6/$7/$8/$9/$ExitCode/$FirefoxLaunchCode/$DownloadRetryCount/$DownloadedBytes/$DownloadSizeBytes/$IntroPhaseSeconds/$OptionsPhaseSeconds/$0/$1/$DownloadFirstTransferSeconds/$2/$3/$4/$InitialInstallRequirementsCode/$OpenedDownloadPage/$ExistingProfile/$ExistingVersion/$ExistingBuildID/$R5/$R6/$R7/$R8/$R2/$R3/$DownloadServerIP/$PostSigningData/$ProfileCleanupPromptType/$CheckboxCleanupProfile" \
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

  ${If} $ProgressCompleted < ${PROGRESS_BAR_INSTALL_END_STEP}
    IntOp $0 ${PROGRESS_BAR_INSTALL_END_STEP} - ${PROGRESS_BAR_DOWNLOAD_END_STEP}
    IntOp $0 $InstallCounterStep * $0
    IntOp $0 $0 / $InstallTotalSteps
    IntOp $ProgressCompleted ${PROGRESS_BAR_DOWNLOAD_END_STEP} + $0
    Call SetProgressBars
  ${EndIf}

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
      Call FinishInstall
    ${EndUnless}
  ${EndIf}
FunctionEnd

Function FinishInstall
  StrCpy $ProgressCompleted "${PROGRESS_BAR_INSTALL_END_STEP}"
  Call SetProgressBars

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}.moz-upgrade"
    Delete "$INSTDIR\${FileMainEXE}"
    Rename "$INSTDIR\${FileMainEXE}.moz-upgrade" "$INSTDIR\${FileMainEXE}"
  ${EndIf}

  StrCpy $ExitCode "${ERR_SUCCESS}"

  Call CopyPostSigningData
  Call LaunchApp
FunctionEnd

Function RelativeGotoPage
  IntCmp $R9 0 0 Move Move
  StrCmp $R9 "X" 0 Move
  StrCpy $R9 "120"

  Move:
  SendMessage $HWNDPARENT "0x408" "$R9" ""
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

Function LaunchApp
!ifndef DEV_EDITION
  FindWindow $0 "${WindowClass}"
  ${If} $0 <> 0 ; integer comparison
    StrCpy $FirefoxLaunchCode "1"

    StrCpy $ProgressCompleted ${PROGRESS_BAR_TOTAL_STEPS}
    Call SetProgressBars

    MessageBox MB_OK|MB_ICONQUESTION "$(WARN_MANUALLY_CLOSE_APP_LAUNCH)"
    Call SendPing
    Return
  ${EndIf}
!endif

  StrCpy $FirefoxLaunchCode "2"

  ; Set the current working directory to the installation directory
  SetOutPath "$INSTDIR"
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $1
  ${If} ${Errors}
    ${If} $CheckboxCleanupProfile == 1
      Exec "$\"$INSTDIR\${FileMainEXE}$\" -reset-profile -migration"
    ${Else}
      Exec "$\"$INSTDIR\${FileMainEXE}$\""
    ${EndIf}
  ${Else}
    StrCpy $R1 $CheckboxCleanupProfile
    GetFunctionAddress $0 LaunchAppFromElevatedProcess
    UAC::ExecCodeSegment $0
  ${EndIf}

  StrCpy $AppLaunchWaitTickCount 0
  ${NSD_CreateTimer} WaitForAppLaunch ${AppLaunchWaitIntervalMS}
FunctionEnd

Function LaunchAppFromElevatedProcess
  ; Set the current working directory to the installation directory
  SetOutPath "$INSTDIR"
  ${If} $R1 == 1
    Exec "$\"$INSTDIR\${FileMainEXE}$\" -reset-profile -migration"
  ${Else}
    Exec "$\"$INSTDIR\${FileMainEXE}$\""
  ${EndIf}
FunctionEnd

Function WaitForAppLaunch
  FindWindow $0 "${MainWindowClass}"
  FindWindow $1 "${DialogWindowClass}"
  ${If} $0 <> 0
  ${OrIf} $1 <> 0
    ${NSD_KillTimer} WaitForAppLaunch
    StrCpy $ProgressCompleted "${PROGRESS_BAR_APP_LAUNCH_END_STEP}"
    Call SetProgressBars
    Call SendPing
    Return
  ${EndIf}

  IntOp $AppLaunchWaitTickCount $AppLaunchWaitTickCount + 1
  IntOp $0 $AppLaunchWaitTickCount * ${AppLaunchWaitIntervalMS}
  ${If} $0 >= ${AppLaunchWaitTimeoutMS}
    ; We've waited an unreasonably long time, so just exit.
    ${NSD_KillTimer} WaitForAppLaunch
    Call SendPing
    Return
  ${EndIf}

  ${If} $ProgressCompleted < ${PROGRESS_BAR_APP_LAUNCH_END_STEP}
    IntOp $ProgressCompleted $ProgressCompleted + 1
    Call SetProgressBars
  ${EndIf}
FunctionEnd

Function CopyPostSigningData
  ${LineRead} "$EXEDIR\postSigningData" "1" $PostSigningData
  ${If} ${Errors}
    ClearErrors
    StrCpy $PostSigningData "0"
  ${Else}
    CreateDirectory "$LOCALAPPDATA\Mozilla\Firefox"
    CopyFiles /SILENT "$EXEDIR\postSigningData" "$LOCALAPPDATA\Mozilla\Firefox"
  ${Endif}
FunctionEnd

Function DisplayDownloadError
  ${NSD_KillTimer} DisplayDownloadError
  ${NSD_KillTimer} NextBlurb
  ${NSD_KillTimer} ClearBlurb
  ; To better display the error state on the taskbar set the progress completed
  ; value to the total value.
  ${ITBL3SetProgressValue} "100" "100"
  ${ITBL3SetProgressState} "${TBPF_ERROR}"
  MessageBox MB_OKCANCEL|MB_ICONSTOP "$(ERROR_DOWNLOAD_CONT)" IDCANCEL +2 IDOK +1
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

Function ShouldPromptForProfileCleanup
  Call GetLatestReleasedVersion

  ; This will be our return value.
  StrCpy $ProfileCleanupPromptType 0

  ; Only consider installations of the same architecture we're installing.
  ${If} $DroplistArch == "$(VERSION_64BIT)"
    SetRegView 64
  ${Else}
    SetRegView 32
  ${EndIf}

  ; Make sure $APPDATA is the user's AppData and not ProgramData.
  ; We'll set this back to all at the end of the function.
  SetShellVarContext current

  ; Check each Profile section in profiles.ini until we find the default profile.
  StrCpy $R0 ""
  ${If} ${FileExists} "$APPDATA\Mozilla\Firefox\profiles.ini"
    StrCpy $0 0
    ${Do}
      ClearErrors
      ; Check if the section exists by reading a value that must be present.
      ReadINIStr $1 "$APPDATA\Mozilla\Firefox\profiles.ini" "Profile$0" "Path"
      ${If} ${Errors}
        ; We've run out of profile sections.
        ${Break}
      ${EndIf}

      ClearErrors
      ReadINIStr $1 "$APPDATA\Mozilla\Firefox\profiles.ini" "Profile$0" "Default"
      ${IfNot} ${Errors}
      ${AndIf} $1 == "1"
        ; We've found the default profile
        ReadINIStr $1 "$APPDATA\Mozilla\Firefox\profiles.ini" "Profile$0" "Path"
        ReadINIStr $2 "$APPDATA\Mozilla\Firefox\profiles.ini" "Profile$0" "IsRelative"
        ${If} $2 == "1"
          StrCpy $R0 "$APPDATA\Mozilla\Firefox\$1"
        ${Else}
          StrCpy $R0 "$1"
        ${EndIf}
        GetFullPathName $R0 $R0
        ${Break}
      ${EndIf}

      IntOp $0 $0 + 1
    ${Loop}
  ${EndIf}

  ${If} $R0 == ""
    ; No profile to clean up, so don't show the cleanup prompt.
    GoTo end
  ${EndIf}

  ; We have at least one profile present. If we don't have any installations,
  ; then we need to show the re-install prompt. We'll say there's an
  ; installation present if HKCR\FirefoxURL* exists and points to a real path.
  StrCpy $0 0
  StrCpy $R9 ""
  ${Do}
    ClearErrors
    EnumRegKey $1 HKCR "" $0
    ${If} ${Errors}
    ${OrIf} $1 == ""
      ${Break}
    ${EndIf}
    ${WordFind} "$1" "-" "+1{" $2
    ${If} $2 == "FirefoxURL"
      ClearErrors
      ReadRegStr $2 HKCR "$1\DefaultIcon" ""
      ${IfNot} ${Errors}
        ${GetPathFromString} $2 $1
        ${If} ${FileExists} $1
          StrCpy $R9 $1
          ${Break}
        ${EndIf}
      ${EndIf}
    ${EndIf}
    IntOp $0 $0 + 1
  ${Loop}
  ${If} $R9 == ""
    StrCpy $ProfileCleanupPromptType 1
    GoTo end
  ${EndIf}

  ; Okay, there's at least one install, let's see if it's for this channel.
  SetShellVarContext all
  ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $0
  ${If} $0 == "false"
    SetShellVarContext current
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $0
    ${If} $0 == "false"
      ; Existing installs are not for this channel. Don't show any prompt.
      GoTo end
    ${EndIf}
  ${EndIf}

  ; Find out what version the default profile was last used on.
  ${If} ${FileExists} "$R0\compatibility.ini"
    ClearErrors
    ReadINIStr $0 "$R0\compatibility.ini" "Compatibility" "LastVersion"
    ${If} ${Errors}
      GoTo end
    ${EndIf}
    ${WordFind} $0 "." "+1{" $0

    ; We don't know what version we're about to install because we haven't
    ; downloaded it yet. Find out what the latest version released on this
    ; channel is and assume we'll be installing that one.
    Call GetLatestReleasedVersion
    ${If} ${Errors}
      ; Use this stub installer's version as a fallback when we can't get the
      ; real current version; this may be behind, but it's better than nothing.
      StrCpy $1 ${AppVersion}
    ${EndIf}

    ${WordFind} $1 "." "+1{" $1
    IntOp $1 $1 - 2

    ${If} $1 > $0
      ; Default profile was last used more than two versions ago, so we need
      ; to show the paveover version of the profile cleanup prompt.
      StrCpy $ProfileCleanupPromptType 2
    ${EndIf}
  ${EndIf}

  end:
  SetRegView lastused
  SetShellVarContext all
FunctionEnd

Function GetLatestReleasedVersion
  ClearErrors
  nsJSON::Set /tree requestConfig /value \
    `{"Url": "https://product-details.mozilla.org/1.0/firefox_versions.json", "Async": false}`
  IfErrors end
  nsJSON::Set /http requestConfig
  IfErrors end
  ${Select} ${Channel}
  ${Case} "unofficial"
    StrCpy $1 "FIREFOX_NIGHTLY"
  ${Case} "nightly"
    StrCpy $1 "FIREFOX_NIGHTLY"
  ${Case} "aurora"
    StrCpy $1 "FIREFOX_AURORA"
  ${Case} "beta"
    StrCpy $1 "LATEST_FIREFOX_RELEASED_DEVEL_VERSION"
  ${Case} "release"
    StrCpy $1 "LATEST_FIREFOX_VERSION"
  ${EndSelect}
  nsJSON::Get "Output" $1 /end
  IfErrors end
  Pop $1

  end:
FunctionEnd

Section
SectionEnd
