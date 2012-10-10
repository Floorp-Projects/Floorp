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
SetCompress force
SetCompressor /FINAL /SOLID lzma
CRCCheck on

RequestExecutionLevel user

!addplugindir ./

Var Dialog
Var ProgressbarDownload
Var ProgressbarInstall
Var LabelDownloadingDown
Var LabelDownloadingInProgress
Var LabelInstallingInProgress
Var LabelInstallingToBeDone
Var LabelFreeSpace
Var CheckboxSetAsDefault
Var CheckboxShortcutOnBar ; Used for Quicklaunch or Taskbar as appropriate
Var CheckboxShortcutInStartMenu
Var CheckboxShortcutOnDesktop
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
Var Initialized
Var DownloadSize
Var HalfOfDownload
Var DownloadReset
Var ExistingTopDir
Var SpaceAvailableBytes
Var InitialInstallDir
Var HandleDownload
Var CanSetAsDefault
Var TmpVal

Var HEIGHT_PX
Var CTL_RIGHT_PX

; On Vista and above attempt to elevate Standard Users in addition to users that
; are a member of the Administrators group.
!define NONADMIN_ELEVATE

!define CONFIG_INI "config.ini"

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

; Workaround to support different urls for Official and Beta since they share
; the same branding.
!ifdef Official
!ifdef BETA_UPDATE_CHANNEL
!undef URLStubDownload
!define URLStubDownload "http://download.mozilla.org/?product=firefox-beta-latest&os=win&lang=${AB_CD}"
!undef URLManualDownload
!define URLManualDownload "http://download.mozilla.org/?product=firefox-beta-latest&os=win&lang=${AB_CD}"
!endif
!endif

!include "common.nsh"

!insertmacro ElevateUAC
!insertmacro GetLongPath
!insertmacro GetPathFromString
!insertmacro GetSingleInstallPath
!insertmacro GetTextWidthHeight
!insertmacro IsUserAdmin
!insertmacro OnStubInstallUninstall
!insertmacro SetBrandNameVars
!insertmacro UnloadUAC

VIAddVersionKey "FileDescription" "${BrandShortName} Stub Installer"
VIAddVersionKey "OriginalFilename" "stub.exe"

Name "$BrandFullName"
OutFile "stub.exe"
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
  !define INDENT "     "
  !define INTRO_BLURB "Thanks for choosing $BrandFullName. We’re not just designed to be different, we’re different by design. Click to install."
  !define INSTALL_BLURB1 "You're about to enjoy the very latest in speed, flexibility and security so you're always in control."
  !define INSTALL_BLURB2 "And you're joining a global community of users, contributors and developers working to make the best browser in the world."
  !define INSTALL_BLURB3 "You even get a haiku:$\n${INDENT}Proudly non-profit$\n${INDENT}Free to innovate for you$\n${INDENT}And a better Web"
  !undef INDENT
!else
  !define INTRO_BLURB "$(INTRO_BLURB)"
  !define INSTALL_BLURB1 "$(INSTALL_BLURB1)"
  !define INSTALL_BLURB2 "$(INSTALL_BLURB2)"
  !define INSTALL_BLURB3 "$(INSTALL_BLURB3)"
!endif

Caption "$(WIN_CAPTION)"

Page custom createDummy ; Needed to enable the Intro page's back button
Page custom createIntro leaveIntro ; Introduction page
Page custom createOptions leaveOptions ; Options page
Page custom createInstall leaveInstall ; Download / Installation page

Function .onInit
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
    ; 3.5.1), to 4 (Windows NT 4) or 5 (Windows 2000 and Windows XP).
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

  SetShellVarContext all      ; Set SHCTX to HKLM
  ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9

  ${If} "$R9" == "false"
    SetShellVarContext current  ; Set SHCTX to HKCU
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9
  ${EndIf}

  ${If} "$R9" != "false"
    StrCpy $INSTDIR "$R9"
  ${EndIf}

  StrCpy $InitialInstallDir "$INSTDIR"

  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" \
                   "Write Test"

  ${If} ${Errors}
  ${OrIf} ${AtLeastWin8}
    StrCpy $CanSetAsDefault "false"
    StrCpy $CheckboxSetAsDefault "0"
  ${Else}
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    StrCpy $CanSetAsDefault "true"
  ${EndIf}

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
  ${UnloadUAC}
FunctionEnd

Function .onUserAbort
  ${NSD_KillTimer} StartDownload
  ${NSD_KillTimer} OnDownload
  ${NSD_KillTimer} StartInstall

  Delete "$PLUGINSDIR\download.exe"
  Delete "$PLUGINSDIR\${CONFIG_INI}"
FunctionEnd

Function createDummy
FunctionEnd

Function createIntro
  ; If Back is clicked on the options page reset variables
  StrCpy $INSTDIR "$InitialInstallDir"
  StrCpy $CheckboxShortcutOnBar 1
  StrCpy $CheckboxShortcutInStartMenu 1
  StrCpy $CheckboxShortcutOnDesktop 1
!ifdef MOZ_MAINTENANCE_SERVICE
  StrCpy $CheckboxInstallMaintSvc 1
!else
  StrCpy $CheckboxInstallMaintSvc 0
!endif

  nsDialogs::Create /NOUNLOAD 1018
  Pop $Dialog

  StrCpy $WasOptionsButtonClicked ""

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

  ${Unless} $Initialized == "true"
    SetCtlColors $HWNDPARENT ${FOOTER_CONTROL_TEXT_COLOR_NORMAL} ${FOOTER_BKGRD_COLOR}
    GetDlgItem $0 $HWNDPARENT 10 ; Default browser checkbox
    ; Set as default is not supported in the installer for Win8 and above so
    ; only display it on Windows 7 and below
    ${If} $CanSetAsDefault == "true"
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
  ${EndUnless}

  ${NSD_CreateBitmap} ${APPNAME_BMP_EDGE_DU} ${APPNAME_BMP_TOP_DU} \
                      ${APPNAME_BMP_WIDTH_DU} ${APPNAME_BMP_HEIGHT_DU} ""
  Pop $2
  ${SetStretchedTransparentImage} $2 $PLUGINSDIR\appname.bmp $0

  ${NSD_CreateBitmap} 0 0 100% 100% ""
  Pop $2
  ${NSD_SetStretchedImage} $2 $PLUGINSDIR\bgintro.bmp $1

  GetDlgItem $0 $HWNDPARENT 1 ; Install button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(INSTALL_BUTTON)"

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(CANCEL_BUTTON)"
  ; Focus the Cancel button so tab stops will start from there.
  ${NSD_SetFocus} $0
  ; Kill the Cancel button's focus so pressing enter won't cancel the install.
  SendMessage $0 ${WM_KILLFOCUS} 0 0

  GetDlgItem $0 $HWNDPARENT 3 ; Back and Options button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(OPTIONS_BUTTON)"

  LockWindow off
  nsDialogs::Show

  ${NSD_FreeImage} $0
  ${NSD_FreeImage} $1

  StrCpy $Initialized "true"
FunctionEnd

Function leaveIntro
  LockWindow on
  SetShellVarContext all      ; Set SHCTX to All Users
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
  ; Skip the options page unless the Options button was clicked
  ${If} "$WasOptionsButtonClicked" != "true"
    ${If} "$CanWriteToInstallDir" == "true"
    ${AndIf} "$HasRequiredSpaceAvailable" == "true"
      Abort
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
    StrCpy $HEIGHT_PX $1
  ${Else}
    StrCpy $HEIGHT_PX $3
  ${EndIf}

  IntOp $0 $0 + 8 ; Add padding to the control's width
  ; Make both controls the same width as the widest control
  ${NSD_CreateLabelCenter} ${OPTIONS_SUBITEM_EDGE_DU} 134u $0 $HEIGHT_PX "$(SPACE_REQUIRED)"
  Pop $5
  SetCtlColors $5 ${OPTIONS_TEXT_COLOR_FADED} ${OPTIONS_BKGRD_COLOR}
  SendMessage $5 ${WM_SETFONT} $FontItalic 0

  IntOp $2 $2 + 8 ; Add padding to the control's width
  ${NSD_CreateLabelCenter} ${OPTIONS_SUBITEM_EDGE_DU} 145u $2 $HEIGHT_PX "$(SPACE_AVAILABLE)"
  Pop $6
  SetCtlColors $6 ${OPTIONS_TEXT_COLOR_FADED} ${OPTIONS_BKGRD_COLOR}
  SendMessage $6 ${WM_SETFONT} $FontItalic 0

  ; Use the widest label for aligning the labels next to them
  ${If} $0 > $2
    StrCpy $6 $5
  ${EndIf}
  FindWindow $1 "#32770" "" $HWNDPARENT
  ${GetDlgItemEndPX} $6 $CTL_RIGHT_PX

  IntOp $CTL_RIGHT_PX $CTL_RIGHT_PX + 6

  ${NSD_CreateLabel} $CTL_RIGHT_PX 134u 100% $HEIGHT_PX \
                     "${APPROXIMATE_REQUIRED_SPACE_MB} $(MEGA)$(BYTE)"
  Pop $7
  SetCtlColors $7 ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $7 ${WM_SETFONT} $FontNormal 0

  ; Create the free space label with an empty string and update it by calling
  ; UpdateFreeSpaceLabel
  ${NSD_CreateLabel} $CTL_RIGHT_PX 145u 100% $HEIGHT_PX " "
  Pop $LabelFreeSpace
  SetCtlColors $LabelFreeSpace ${OPTIONS_TEXT_COLOR_NORMAL} ${OPTIONS_BKGRD_COLOR}
  SendMessage $LabelFreeSpace ${WM_SETFONT} $FontNormal 0

  Call UpdateFreeSpaceLabel

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
      ${NSD_CreateCheckbox} ${OPTIONS_ITEM_EDGE_DU} 175u ${OPTIONS_ITEM_WIDTH_DU} \
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
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(INSTALL_BUTTON)"

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(CANCEL_BUTTON)"
  ; If the first control in the dialog is focused it won't display with a focus
  ; ring on dialog creation when the Options button is clicked so just focus the
  ; Cancel button so tab stops will start from there.
  ${NSD_SetFocus} $0
  ; Kill the Cancel button's focus so pressing enter won't cancel the install.
  SendMessage $0 ${WM_KILLFOCUS} 0 0

  GetDlgItem $0 $HWNDPARENT 3 ; Back and Options button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(BACK_BUTTON)"

  ${If} "$WasOptionsButtonClicked" != "true"
    ${If} "$CanWriteToInstallDir" == "false"
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_WRITE_ACCESS)"
    ${ElseIf} "$HasRequiredSpaceAvailable" == "false"
      MessageBox MB_OK|MB_ICONEXCLAMATION "$(WARN_DISK_SPACE)"
    ${EndIf}
  ${EndIf}

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

  ${NSD_GetState} $CheckboxShortcutOnBar $CheckboxShortcutOnBar
  ${NSD_GetState} $CheckboxShortcutInStartMenu $CheckboxShortcutInStartMenu
  ${NSD_GetState} $CheckboxShortcutOnDesktop $CheckboxShortcutOnDesktop
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

  nsDialogs::CreateControl /NOUNLOAD STATIC ${DEFAULT_STYLES}|${SS_SUNKEN} 0 260u 166u 1u 30u ""
  Pop $0

  ${NSD_CreateLabelCenter} 103u 180u 157u 20u "$(DOWNLOADING_IN_PROGRESS)"
  Pop $LabelDownloadingInProgress
  SendMessage $LabelDownloadingInProgress ${WM_SETFONT} $FontNormal 0
  SetCtlColors $LabelDownloadingInProgress ${INSTALL_PROGRESS_TEXT_COLOR_NORMAL} transparent

  ${NSD_CreateLabelCenter} 103u 180u 157u 20u "$(DOWNLOADING_DONE)"
  Pop $LabelDownloadingDown
  SendMessage $LabelDownloadingDown ${WM_SETFONT} $FontItalic 0
  SetCtlColors $LabelDownloadingDown ${INSTALL_PROGRESS_TEXT_COLOR_FADED} transparent
  ShowWindow $LabelDownloadingDown ${SW_HIDE}

  ${NSD_CreateLabelCenter} 260uu 180u 84u 20u "$(INSTALLING_TO_BE_DONE)"
  Pop $LabelInstallingToBeDone
  SendMessage $LabelInstallingToBeDone ${WM_SETFONT} $FontItalic 0
  SetCtlColors $LabelInstallingToBeDone ${INSTALL_PROGRESS_TEXT_COLOR_FADED} transparent

  ${NSD_CreateLabelCenter} 260uu 180u 84u 20u "$(INSTALLING_IN_PROGRESS)"
  Pop $LabelInstallingInProgress
  SendMessage $LabelInstallingInProgress ${WM_SETFONT} $FontNormal 0
  SetCtlColors $LabelInstallingInProgress ${INSTALL_PROGRESS_TEXT_COLOR_NORMAL} transparent
  ShowWindow $LabelInstallingInProgress ${SW_HIDE}

  ${NSD_CreateProgressBar} 103u 166u 157u 9u ""
  Pop $ProgressbarDownload
  ${NSD_AddStyle} $ProgressbarDownload ${PBS_MARQUEE}
  SendMessage $ProgressbarDownload ${PBM_SETMARQUEE} 1 10 ; start=1|stop=0 interval(ms)=+N

  ${NSD_CreateProgressBar} 260u 166u 84u 9u ""
  Pop $ProgressbarInstall
  ${NSD_AddStyle} $ProgressbarInstall ${PBS_MARQUEE}
  SendMessage $ProgressbarInstall ${PBM_SETMARQUEE} 0 10 ; start=1|stop=0 interval(ms)=+N

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

  GetDlgItem $0 $HWNDPARENT 3 ; Back and Options button
  EnableWindow $0 0
  ShowWindow $0 ${SW_HIDE}

  GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(CANCEL_BUTTON)"
  ; Focus the Cancel button otherwise it isn't possible to tab to it since it is
  ; the only control that can be tabbed to.
  ${NSD_SetFocus} $0
  ; Kill the Cancel button's focus so pressing enter won't cancel the install.
  SendMessage $0 ${WM_KILLFOCUS} 0 0

  ; Set as default is not supported in the installer for Win8 and above
  ${If} $CanSetAsDefault == "true"
    GetDlgItem $0 $HWNDPARENT 10 ; Default browser checkbox
    SendMessage $0 ${BM_GETCHECK} 0 0 $CheckboxSetAsDefault
    EnableWindow $0 0
    ShowWindow $0 ${SW_HIDE}
  ${EndIf}

  GetDlgItem $0 $HWNDPARENT 11
  SendMessage $0 ${WM_SETTEXT} 0 "STR:$(ONE_MOMENT)"
  SendMessage $0 ${WM_SETFONT} $FontNormal 0
  SetCtlColors $0 ${FOOTER_CONTROL_TEXT_COLOR_FADED} ${FOOTER_BKGRD_COLOR}
  ShowWindow $0 ${SW_SHOW}

  StrCpy $DownloadReset "false"
  ${NSD_CreateTimer} StartDownload 500

  LockWindow off
  nsDialogs::Show

  ${NSD_FreeImage} $0
  ${NSD_FreeImage} $1
  ${NSD_FreeImage} $HwndBitmapBlurb1
  ${NSD_FreeImage} $HwndBitmapBlurb2
  ${NSD_FreeImage} $HWndBitmapBlurb3
FunctionEnd

Function leaveInstall
# Need a ping?
FunctionEnd

Function StartDownload
  ${NSD_KillTimer} StartDownload
  InetBgDL::Get "${URLStubDownload}" "$PLUGINSDIR\download.exe" /END
  StrCpy $4 ""
  ${NSD_CreateTimer} OnDownload 500
  ${If} ${FileExists} "$INSTDIR\${TO_BE_DELETED}"
    RmDir /r "$INSTDIR\${TO_BE_DELETED}"
  ${EndIf}
FunctionEnd

!define FILE_SHARE_READ 1
!define GENERIC_READ 0x80000000
!define OPEN_EXISTING 3
!define FILE_BEGIN 0
!define FILE_END 2
!define INVALID_HANDLE_VALUE -1
!define INVALID_FILE_SIZE 0xffffffff

Function OnDownload
  InetBgDL::GetStats
  # $0 = HTTP status code, 0=Completed
  # $1 = Completed files
  # $2 = Remaining files
  # $3 = Number of downloaded bytes for the current file
  # $4 = Size of current file (Empty string if the size is unknown)
  # /RESET must be used if status $0 > 299 (e.g. failure)
  # When status is $0 =< 299 it is handled by InetBgDL
  ${If} $0 > 299
    ${NSD_KillTimer} OnDownload
    ${If} "$DownloadReset" != "true"
      ${NSD_AddStyle} $ProgressbarDownload ${PBS_MARQUEE}
      SendMessage $ProgressbarDownload ${PBM_SETMARQUEE} 1 10 ; start=1|stop=0 interval(ms)=+N
    ${EndIf}
    InetBgDL::Get /RESET /END
    ${NSD_CreateTimer} StartDownload 500
    StrCpy $DownloadReset "true"
    StrCpy $DownloadSize ""
    Return
  ${EndIf}

  ${If} "$DownloadReset" == "true"
    StrCpy $DownloadReset "false"
  ${EndIf}

  ${If} $DownloadSize == ""
  ${AndIf} $4 != ""
    StrCpy $DownloadSize $4
    System::Int64Op $4 / 2
    Pop $HalfOfDownload
    SendMessage $ProgressbarDownload ${PBM_SETMARQUEE} 0 50 ; start=1|stop=0 interval(ms)=+N
    ${RemoveStyle} $ProgressbarDownload ${PBS_MARQUEE}
    SendMessage $ProgressbarDownload ${PBM_SETRANGE32} 0 $DownloadSize
  ${EndIf}

  ; Don't update the status until after the download starts
  ${If} $2 != 0
  ${AndIf} $4 == ""
    Return
  ${EndIf}

  ${If} $IsDownloadFinished != "true"
    ${If} $2 == 0
      ${NSD_KillTimer} OnDownload
      StrCpy $IsDownloadFinished "true"
      LockWindow on
      ; Update the progress bars first in the UI change so they take affect
      ; before other UI changes.
      SendMessage $ProgressbarDownload ${PBM_SETPOS} $DownloadSize 0
      ${NSD_AddStyle} $ProgressbarDownload ${PBS_MARQUEE}
      SendMessage $ProgressbarInstall ${PBM_SETMARQUEE} 1 10 ; start=1|stop=0 interval(ms)=+N
      ShowWindow $LabelDownloadingInProgress ${SW_HIDE}
      ShowWindow $LabelInstallingToBeDone ${SW_HIDE}
      ShowWindow $LabelInstallingInProgress ${SW_SHOW}
      ShowWindow $LabelDownloadingDown ${SW_SHOW}
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
        StrCpy $0 "0"
        StrCpy $1 "0"
      ${Else}
        CertCheck::VerifyCertTrust "$PLUGINSDIR\download.exe"
        Pop $0
        CertCheck::VerifyCertNameIssuer "$PLUGINSDIR\download.exe" \
                                        "${CertNameDownload}" "${CertIssuerDownload}"
        Pop $1
      ${EndIf}

      ${If} $0 == 0
      ${OrIf} $1 == 0
        MessageBox MB_OKCANCEL|MB_ICONSTOP "$(ERROR_DOWNLOAD)" IDCANCEL +2 IDOK 0
        ExecShell "open" "${URLManualDownload}"
        ; The following will exit the installer
        SetAutoClose true
        StrCpy $R9 2
        Call RelativeGotoPage
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

      ${OnStubInstallUninstall}

      Exec "$\"$PLUGINSDIR\download.exe$\" /INI=$PLUGINSDIR\${CONFIG_INI}"
      ; Close the handle that prevents modification of the full installer
      System::Call 'kernel32::CloseHandle(i $HandleDownload)'
      ${NSD_CreateTimer} StartInstall 1000
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
      SendMessage $ProgressbarDownload ${PBM_SETPOS} $3 0
    ${EndIf}
  ${EndIf}
FunctionEnd

Function StartInstall
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
  ${AndIf} ${FileExists} "$INSTDIR\uninstall\uninstall.log"
    Delete "$INSTDIR\uninstall\uninstall.tmp"
    CopyFiles /SILENT "$INSTDIR\uninstall\uninstall.log" "$INSTDIR\uninstall\uninstall.tmp"
    ClearErrors
    Delete "$INSTDIR\uninstall\uninstall.log"
    ${Unless} ${Errors}
      CopyFiles /SILENT "$INSTDIR\uninstall\uninstall.tmp" "$INSTDIR\uninstall\uninstall.log"
      ${NSD_KillTimer} StartInstall
      Delete "$INSTDIR\uninstall\uninstall.tmp"
      Delete "$PLUGINSDIR\download.exe"
      Delete "$PLUGINSDIR\${CONFIG_INI}"

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

      Call LaunchApp

      ; The following will exit the installer
      SetAutoClose true
      StrCpy $R9 2
      Call RelativeGotoPage
    ${EndUnless}
  ${EndIf}
FunctionEnd

Function OnBack
  StrCpy $WasOptionsButtonClicked "true"
  StrCpy $R9 1 ; Goto the next page
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
  StrCpy $INSTDIR $0
  Call UpdateFreeSpaceLabel
FunctionEnd

Function OnClick_ButtonBrowse
  ; The call to GetLongPath returns a long path without a trailing
  ; back-slash. Append a \ to the path to prevent the directory
  ; name from being appended when using the NSIS create new folder.
  ; http://www.nullsoft.com/free/nsis/makensis.htm#InstallDir
  StrCpy $0 "$INSTDIR" "" -1 ; the last character
  ${If} "$0" == "\"
    StrCpy $0 "$INSTDIR"
  ${Else}
    StrCpy $0 "$INSTDIR"
  ${EndIf}

  nsDialogs::SelectFolderDialog /NOUNLOAD "$(SELECT_FOLDER_TEXT)" $0
  Pop $0
  ${If} $0 == "error" ; returns 'error' if 'cancel' was pressed?
    Return
  ${EndIf}

  ${If} $0 != ""
    StrCpy $INSTDIR "$0"
    system::Call 'user32::SetWindowTextW(i $DirRequest, w "$INSTDIR")'
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

  ; GetDiskFreeSpaceExW can require a backslash
  StrCpy $0 "$ExistingTopDir" "" -1 ; the last character
  ${If} "$0" != "\"
    ; A backslash is required for 
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
    MessageBox MB_OK|MB_ICONQUESTION "$(WARN_MANUALLY_CLOSE_APP_LAUNCH)"
    Return
  ${EndIf}

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
  Exec "$\"$0$\""
FunctionEnd

Section
SectionEnd
