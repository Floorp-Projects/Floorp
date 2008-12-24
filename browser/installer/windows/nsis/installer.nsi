# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Mozilla Installer code.
#
# The Initial Developer of the Original Code is Mozilla Foundation
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Robert Strong <robert.bugzilla@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Required Plugins:
# AppAssocReg http://nsis.sourceforge.net/Application_Association_Registration_plug-in
# ShellLink   http://nsis.sourceforge.net/ShellLink_plug-in
# UAC         http://nsis.sourceforge.net/UAC_plug-in

; Set verbosity to 3 (e.g. no script) to lessen the noise in the build logs
!verbose 3

; 7-Zip provides better compression than the lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
SetDatablockOptimize on
SetCompress off
CRCCheck on

RequestExecutionLevel user

!addplugindir ./

; empty files - except for the comment line - for generating custom pages.
!system 'echo ; > options.ini'
!system 'echo ; > shortcuts.ini'
!system 'echo ; > summary.ini'

; USE_UAC_PLUGIN is temporary until all applications have been updated to use
; the UAC plugin
!define USE_UAC_PLUGIN

Var TmpVal
Var StartMenuDir
Var InstallType
Var AddStartMenuSC
Var AddQuickLaunchSC
Var AddDesktopSC

; Other included files may depend upon these includes!
; The following includes are provided by NSIS.
!include FileFunc.nsh
!include LogicLib.nsh
!include MUI.nsh
!include WinMessages.nsh
!include WinVer.nsh
!include WordFunc.nsh

!insertmacro GetOptions
!insertmacro GetParameters
!insertmacro GetSize
!insertmacro StrFilter
!insertmacro WordReplace

; The following includes are custom.
!include branding.nsi
!include defines.nsi
!include common.nsh
!include locales.nsi
!include version.nsh

VIAddVersionKey "FileDescription" "${BrandShortName} Installer"
VIAddVersionKey "OriginalFilename" "setup.exe"

; Must be inserted before other macros that use logging
!insertmacro _LoggingCommon

!insertmacro AddDDEHandlerValues
!insertmacro ChangeMUIHeaderImage
!insertmacro CheckForFilesInUse
!insertmacro CleanUpdatesDir
!insertmacro CopyFilesFromDir
!insertmacro CreateRegKey
!insertmacro GetPathFromString
!insertmacro GetParent
!insertmacro IsHandlerForInstallDir
!insertmacro ManualCloseAppPrompt
!insertmacro RegCleanAppHandler
!insertmacro RegCleanMain
!insertmacro RegCleanUninstall
!insertmacro SetBrandNameVars
!insertmacro UnloadUAC
!insertmacro WriteRegStr2
!insertmacro WriteRegDWORD2

!include shared.nsh

; Helper macros for ui callbacks. Insert these after shared.nsh
!insertmacro CheckCustomCommon
!insertmacro InstallEndCleanupCommon
!insertmacro InstallOnInitCommon
!insertmacro InstallStartCleanupCommon
!insertmacro LeaveDirectoryCommon
!insertmacro OnEndCommon
!insertmacro PreDirectoryCommon

Name "${BrandFullName}"
OutFile "setup.exe"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})" "InstallLocation"
InstallDir "$PROGRAMFILES\${BrandFullName}\"
ShowInstDetails nevershow

ReserveFile options.ini
ReserveFile shortcuts.ini
ReserveFile summary.ini

################################################################################
# Modern User Interface - MUI

!define MUI_ABORTWARNING
!define MUI_ICON setup.ico
!define MUI_UNICON setup.ico
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_WELCOMEFINISHPAGE_BITMAP wizWatermark.bmp

; Use a right to left header image when the language is right to left
!ifdef ${AB_CD}_rtl
!define MUI_HEADERIMAGE_BITMAP_RTL wizHeaderRTL.bmp
!else
!define MUI_HEADERIMAGE_BITMAP wizHeader.bmp
!endif

/**
 * Installation Pages
 */
; Welcome Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE preWelcome
!insertmacro MUI_PAGE_WELCOME

; Custom Options Page
Page custom preOptions leaveOptions

; Select Install Directory Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE preDirectory
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE leaveDirectory
!define MUI_DIRECTORYPAGE_VERIFYONLEAVE
!insertmacro MUI_PAGE_DIRECTORY

; Custom Shortcuts Page
Page custom preShortcuts leaveShortcuts

; Start Menu Folder Page Configuration
!define MUI_PAGE_CUSTOMFUNCTION_PRE preStartMenu
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE leaveStartMenu
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Main"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuDir

; Custom Summary Page
Page custom preSummary leaveSummary

; Install Files Page
!insertmacro MUI_PAGE_INSTFILES

; Finish Page
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchApp
!define MUI_FINISHPAGE_RUN_TEXT $(LAUNCH_TEXT)
!define MUI_PAGE_CUSTOMFUNCTION_PRE preFinish
!insertmacro MUI_PAGE_FINISH

; Use the default dialog for IDD_VERIFY for a simple Banner
ChangeUI IDD_VERIFY "${NSISDIR}\Contrib\UIs\default.exe"

################################################################################
# Install Sections

; Cleanup operations to perform at the start of the installation.
Section "-InstallStartCleanup"
  SetDetailsPrint both
  DetailPrint $(STATUS_CLEANUP)
  SetDetailsPrint none

  SetOutPath "$INSTDIR"
  ${StartInstallLog} "${BrandFullName}" "${AB_CD}" "${AppVersion}" "${GREVersion}"

  ; Delete the app exe to prevent launching the app while we are installing.
  ClearErrors
  ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${If} ${Errors}
    ; If the user closed the application it can take several seconds for it to
    ; shut down completely. If the application is being used by another user we
    ; can rename the file and then delete is when the system is restarted.
    Sleep 5000
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
    ClearErrors
  ${EndIf}

  ; Remove the updates directory for Vista and above
  ${CleanUpdatesDir} "Mozilla\Firefox"

  ${InstallStartCleanupCommon}
SectionEnd

Section "-Application" APP_IDX
  ${StartUninstallLog}

  SetDetailsPrint both
  DetailPrint $(STATUS_INSTALL_APP)
  SetDetailsPrint none

  ${LogHeader} "Installing Main Files"
  ${CopyFilesFromDir} "$EXEDIR\nonlocalized" "$INSTDIR" \
                      "$(ERROR_CREATE_DIRECTORY_PREFIX)" \
                      "$(ERROR_CREATE_DIRECTORY_SUFFIX)"

  ; Register DLLs
  ; XXXrstrong - AccessibleMarshal.dll can be used by multiple applications but
  ; is only registered for the last application installed. When the last
  ; application installed is uninstalled AccessibleMarshal.dll will no longer be
  ; registered. bug 338878
  ${LogHeader} "DLL Registration"
  ClearErrors
  RegDLL "$INSTDIR\AccessibleMarshal.dll"
  ${If} ${Errors}
    ${LogMsg} "** ERROR Registering: $INSTDIR\AccessibleMarshal.dll **"
  ${Else}
    ${LogUninstall} "DLLReg: \AccessibleMarshal.dll"
    ${LogMsg} "Registered: $INSTDIR\AccessibleMarshal.dll"
  ${EndIf}

  ; Write extra files created by the application to the uninstall.log so they
  ; will be removed when the application is uninstalled. To remove an empty
  ; directory write a bogus filename to the deepest directory and all empty
  ; parent directories will be removed.
  ${LogUninstall} "File: \components\compreg.dat"
  ${LogUninstall} "File: \components\xpti.dat"
  ${LogUninstall} "File: \.autoreg"
  ${LogUninstall} "File: \active-update.xml"
  ${LogUninstall} "File: \install.log"
  ${LogUninstall} "File: \install_status.log"
  ${LogUninstall} "File: \install_wizard.log"
  ${LogUninstall} "File: \updates.xml"

  SetDetailsPrint both
  DetailPrint $(STATUS_INSTALL_LANG)
  SetDetailsPrint none

  ${LogHeader} "Installing Localized Files"
  ${CopyFilesFromDir} "$EXEDIR\localized" "$INSTDIR" \
                      "$(ERROR_CREATE_DIRECTORY_PREFIX)" \
                      "$(ERROR_CREATE_DIRECTORY_SUFFIX)"

  ${LogHeader} "Adding Additional Files"
  ; Check if QuickTime is installed and copy the nsIQTScriptablePlugin.xpt from
  ; its plugins directory into the app's components directory.
  ClearErrors
  ReadRegStr $R0 HKLM "Software\Apple Computer, Inc.\QuickTime" "InstallDir"
  ${Unless} ${Errors}
    ${GetLongPath} $R0 "$R0"
    ${Unless} $R0 == ""
      ClearErrors
      GetFullPathName $R0 "$R0\Plugins\nsIQTScriptablePlugin.xpt"
      ${Unless} ${Errors}
        ${LogHeader} "Copying QuickTime Scriptable Component"
        CopyFiles /SILENT "$R0" "$INSTDIR\components"
        ${If} ${Errors}
          ${LogMsg} "** ERROR Installing File: $INSTDIR\components\nsIQTScriptablePlugin.xpt **"
        ${Else}
          ${LogMsg} "Installed File: $INSTDIR\components\nsIQTScriptablePlugin.xpt"
          ${LogUninstall} "File: \components\nsIQTScriptablePlugin.xpt"
        ${EndIf}
      ${EndUnless}
    ${EndUnless}
  ${EndUnless}
  ClearErrors

  ; Default for creating Start Menu folder and shortcuts
  ; (1 = create, 0 = don't create)
  ${If} $AddStartMenuSC == ""
    StrCpy $AddStartMenuSC "1"
  ${EndIf}

  ; Default for creating Quick Launch shortcut (1 = create, 0 = don't create)
  ${If} $AddQuickLaunchSC == ""
    StrCpy $AddQuickLaunchSC "1"
  ${EndIf}

  ; Default for creating Desktop shortcut (1 = create, 0 = don't create)
  ${If} $AddDesktopSC == ""
    StrCpy $AddDesktopSC "1"
  ${EndIf}

  ${LogHeader} "Adding Registry Entries"
  SetShellVarContext current  ; Set SHCTX to HKCU
  ${RegCleanMain} "Software\Mozilla"
  ${RegCleanUninstall}
  ${UpdateProtocolHandlers}

  ClearErrors
  WriteRegStr HKLM "Software\Mozilla\InstallerTest" "InstallerTest" "Test"
  ${If} ${Errors}
    StrCpy $TmpVal "HKCU" ; used primarily for logging
  ${Else}
    SetShellVarContext all  ; Set SHCTX to HKLM
    DeleteRegKey HKLM "Software\Mozilla\InstallerTest"
    StrCpy $TmpVal "HKLM" ; used primarily for logging
    ${RegCleanMain} "Software\Mozilla"
    ${RegCleanUninstall}
    ${UpdateProtocolHandlers}

    ReadRegStr $0 HKLM "Software\mozilla.org\Mozilla" "CurrentVersion"
    ${If} "$0" != "${GREVersion}"
      WriteRegStr HKLM "Software\mozilla.org\Mozilla" "CurrentVersion" "${GREVersion}"
    ${EndIf}
  ${EndIf}

  ${RemoveDeprecatedKeys}

  ; The previous installer adds several regsitry values to both HKLM and HKCU.
  ; We now try to add to HKLM and if that fails to HKCU

  ; The order that reg keys and values are added is important if you use the
  ; uninstall log to remove them on uninstall. When using the uninstall log you
  ; MUST add children first so they will be removed first on uninstall so they
  ; will be empty when the key is deleted. This allows the uninstaller to
  ; specify that only empty keys will be deleted.
  ${SetAppKeys}

  ; XXXrstrong - this should be set in shared.nsh along with "Create Quick
  ; Launch Shortcut" and Create Desktop Shortcut.
  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Uninstall"
  ${WriteRegDWORD2} $TmpVal "$0" "Create Start Menu Shortcut" $AddStartMenuSC 0

  ${FixClassKeys}

  ; On install always add the FirefoxHTML and FirefoxURL keys.
  ; An empty string is used for the 5th param because FirefoxHTML and FirefoxURL
  ; are not protocol handlers.
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  StrCpy $2 "$\"$8$\" -requestPending -osint -url $\"%1$\""
  StrCpy $3 "$\"%1$\",,0,0,,,,"

  ${AddDDEHandlerValues} "FirefoxHTML" "$2" "$8,1" "${AppRegName} Document" "" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"

  ${AddDDEHandlerValues} "FirefoxURL" "$2" "$8,1" "${AppRegName} URL" "true" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"

  ${FixShellIconHandler}

  ; The following keys should only be set if we can write to HKLM
  ${If} $TmpVal == "HKLM"
    ; Uninstall keys can only exist under HKLM on some versions of windows.
    ${SetUninstallKeys}

    ; Set the Start Menu Internet and Vista Registered App HKLM registry keys.
    ${SetStartMenuInternet}

    ; If we are writing to HKLM and create the quick launch and the desktop
    ; shortcuts set IconsVisible to 1 otherwise to 0.
    ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
    StrCpy $0 "Software\Clients\StartMenuInternet\$R9\InstallInfo"
    ${If} $AddQuickLaunchSC == 1
    ${OrIf} $AddDesktopSC == 1
      WriteRegDWORD HKLM "$0" "IconsVisible" 1
    ${Else}
      WriteRegDWORD HKLM "$0" "IconsVisible" 0
    ${EndIf}
  ${EndIf}

  ; These need special handling on uninstall since they may be overwritten by
  ; an install into a different location.
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\App Paths\${FileMainEXE}"
  ${WriteRegStr2} $TmpVal "$0" "" "$INSTDIR\${FileMainEXE}" 0
  ${WriteRegStr2} $TmpVal "$0" "Path" "$INSTDIR" 0

  StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\$R9"
  ${CreateRegKey} "$TmpVal" "$0" 0

  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

  ; Create Start Menu shortcuts
  ${LogHeader} "Adding Shortcuts"
  ${If} $AddStartMenuSC == 1
    ${Unless} ${FileExists} "$SMPROGRAMS\$StartMenuDir"
      CreateDirectory "$SMPROGRAMS\$StartMenuDir"
      ${LogMsg} "Added Start Menu Directory: $SMPROGRAMS\$StartMenuDir"
    ${EndUnless}
    CreateShortCut "$SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal}.lnk"
    ${LogMsg} "Added Shortcut: $SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal}.lnk"
    CreateShortCut "$SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal} ($(SAFE_MODE)).lnk" "$INSTDIR\${FileMainEXE}" "-safe-mode" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal} ($(SAFE_MODE)).lnk"
    ${LogMsg} "Added Shortcut: $SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal} ($(SAFE_MODE)).lnk"
  ${EndIf}

  ; perhaps use the uninstall keys
  ${If} $AddQuickLaunchSC == 1
    CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $QUICKLAUNCH\${BrandFullName}.lnk"
    ${LogMsg} "Added Shortcut: $QUICKLAUNCH\${BrandFullName}.lnk"
  ${EndIf}

  ${If} $AddDesktopSC == 1
    CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $DESKTOP\${BrandFullName}.lnk"
    ${LogMsg} "Added Shortcut: $DESKTOP\${BrandFullName}.lnk"
  ${EndIf}

  !insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

; Cleanup operations to perform at the end of the installation.
Section "-InstallEndCleanup"
  SetDetailsPrint both
  DetailPrint "$(STATUS_CLEANUP)"
  SetDetailsPrint none

  ${Unless} ${Silent}
    ${MUI_INSTALLOPTIONS_READ} $0 "options.ini" "Field 6" "State"
    ${If} "$0" == "1"
      ${LogHeader} "Setting as the default browser"
      ${SetAsDefaultAppUser}
    ${EndIf}
  ${EndUnless}

  ${LogHeader} "Updating Uninstall Log With Previous Uninstall Log"

  ; Refresh desktop icons
  System::Call "shell32::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)"

  ${InstallEndCleanupCommon}

  ; If we have to reboot give SHChangeNotify time to finish the refreshing
  ; the icons so the OS doesn't display the icons from helper.exe
  ${If} ${RebootFlag}
    Sleep 10000
    ${LogHeader} "Reboot Required To Finish Installation"
    ; ${FileMainEXE}.moz-upgrade should never exist but just in case...
    ${Unless} ${FileExists} "$INSTDIR\${FileMainEXE}.moz-upgrade"
      Rename "$INSTDIR\${FileMainEXE}" "$INSTDIR\${FileMainEXE}.moz-upgrade"
    ${EndUnless}

    ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
      ClearErrors
      Rename "$INSTDIR\${FileMainEXE}" "$INSTDIR\${FileMainEXE}.moz-delete"
      ${Unless} ${Errors}
        Delete /REBOOTOK "$INSTDIR\${FileMainEXE}.moz-delete"
      ${EndUnless}
    ${EndUnless}

    ${Unless} ${FileExists} "$INSTDIR\${FileMainEXE}"
      CopyFiles /SILENT "$INSTDIR\uninstall\helper.exe" "$INSTDIR"
      FileOpen $0 "$INSTDIR\${FileMainEXE}" w
      FileWrite $0 "Will be deleted on restart"
      Rename /REBOOTOK "$INSTDIR\${FileMainEXE}.moz-upgrade" "$INSTDIR\${FileMainEXE}"
      FileClose $0
      Delete "$INSTDIR\${FileMainEXE}"
      Rename "$INSTDIR\helper.exe" "$INSTDIR\${FileMainEXE}"
    ${EndUnless}
  ${EndIf}
SectionEnd

################################################################################
# Helper Functions

Function CheckExistingInstall
  ; If there is a pending file copy from a previous uninstall don't allow
  ; installing until after the system has rebooted.
  IfFileExists "$INSTDIR\${FileMainEXE}.moz-upgrade" +1 +4
  MessageBox MB_YESNO "$(WARN_RESTART_REQUIRED_UPGRADE)" IDNO +2
  Reboot
  Quit

  ; If there is a pending file deletion from a previous uninstall don't allow
  ; installing until after the system has rebooted.
  IfFileExists "$INSTDIR\${FileMainEXE}.moz-delete" +1 +4
  MessageBox MB_YESNO "$(WARN_RESTART_REQUIRED_UNINSTALL)" IDNO +2
  Reboot
  Quit

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    Banner::show /NOUNLOAD "$(BANNER_CHECK_EXISTING)"

    ${If} "$TmpVal" == "FoundMessageWindow"
      Sleep 5000
    ${EndIf}

    ${PushFilesToCheck}

    ; Store the return value in $TmpVal so it is less likely to be accidentally
    ; overwritten elsewhere.
    ${CheckForFilesInUse} $TmpVal

    Banner::destroy

    ${If} "$TmpVal" == "true"
      StrCpy $TmpVal "FoundMessageWindow"
      ${ManualCloseAppPrompt} "${WindowClass}" "$(WARN_MANUALLY_CLOSE_APP_INSTALL)"
      StrCpy $TmpVal "true"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function LaunchApp
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $1
  ${If} ${Errors}
    ${ManualCloseAppPrompt} "${WindowClass}" "$(WARN_MANUALLY_CLOSE_APP_LAUNCH)"
    Exec "$INSTDIR\${FileMainEXE}"
  ${Else}
    GetFunctionAddress $0 LaunchAppFromElevatedProcess
    UAC::ExecCodeSegment $0
  ${EndIf}
FunctionEnd

Function LaunchAppFromElevatedProcess
  ${ManualCloseAppPrompt} "${WindowClass}" "$(WARN_MANUALLY_CLOSE_APP_LAUNCH)"

  ; Find the installation directory when launching using GetFunctionAddress
  ; from an elevated installer since $INSTDIR will not be set in this installer
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${GetPathFromString} "$0" $0
  ${GetParent} "$0" $1
  ; Set our current working directory to the application's install directory
  ; otherwise the 7-Zip temp directory will be in use and won't be deleted.
  SetOutPath "$1"
  Exec "$0"
FunctionEnd

################################################################################
# Language

!insertmacro MOZ_MUI_LANGUAGE 'baseLocale'
!verbose push
!verbose 3
!include "overrideLocale.nsh"
!include "customLocale.nsh"
!verbose pop

; Set this after the locale files to override it if it is in the locale
; using " " for BrandingText will hide the "Nullsoft Install System..." branding
BrandingText " "

################################################################################
# Page pre, show, and leave functions

Function preWelcome
  ${If} ${FileExists} "$EXEDIR\localized\distribution\modern-wizard.bmp"
    Delete "$PLUGINSDIR\modern-wizard.bmp"
    CopyFiles /SILENT "$EXEDIR\localized\distribution\modern-wizard.bmp" "$PLUGINSDIR\modern-wizard.bmp"
  ${EndIf}
FunctionEnd

Function preOptions
  ${If} ${FileExists} "$EXEDIR\localized\distribution\modern-header.bmp"
  ${AndIf} $hHeaderBitmap == ""
    Delete "$PLUGINSDIR\modern-header.bmp"
    CopyFiles /SILENT "$EXEDIR\localized\distribution\modern-header.bmp" "$PLUGINSDIR\modern-header.bmp"
    ${ChangeMUIHeaderImage} "$PLUGINSDIR\modern-header.bmp"
  ${EndIf}
  !insertmacro MUI_HEADER_TEXT "$(OPTIONS_PAGE_TITLE)" "$(OPTIONS_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "options.ini"
FunctionEnd

Function leaveOptions
  ${MUI_INSTALLOPTIONS_READ} $0 "options.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  ${MUI_INSTALLOPTIONS_READ} $R0 "options.ini" "Field 2" "State"
  StrCmp $R0 "1" +1 +2
  StrCpy $InstallType ${INSTALLTYPE_BASIC}
  ${MUI_INSTALLOPTIONS_READ} $R0 "options.ini" "Field 3" "State"
  StrCmp $R0 "1" +1 +2
  StrCpy $InstallType ${INSTALLTYPE_CUSTOM}

  ${If} $InstallType != ${INSTALLTYPE_CUSTOM}
!ifndef NO_INSTDIR_FROM_REG
    SetShellVarContext all      ; Set SHCTX to HKLM
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9

    StrCmp "$R9" "false" +1 fix_install_dir

    SetShellVarContext current  ; Set SHCTX to HKCU
    ${GetSingleInstallPath} "Software\Mozilla\${BrandFullNameInternal}" $R9

    fix_install_dir:
    StrCmp "$R9" "false" +2 +1
    StrCpy $INSTDIR "$R9"
!endif

    Call CheckExistingInstall
  ${EndIf}
FunctionEnd

Function preDirectory
  ${PreDirectoryCommon}
FunctionEnd

Function leaveDirectory
  ${LeaveDirectoryCommon} "$(WARN_DISK_SPACE)" "$(WARN_WRITE_ACCESS)"
FunctionEnd

Function preShortcuts
  ${CheckCustomCommon}
  !insertmacro MUI_HEADER_TEXT "$(SHORTCUTS_PAGE_TITLE)" "$(SHORTCUTS_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "shortcuts.ini"
FunctionEnd

Function leaveShortcuts
  ${MUI_INSTALLOPTIONS_READ} $0 "shortcuts.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  ${MUI_INSTALLOPTIONS_READ} $AddDesktopSC "shortcuts.ini" "Field 2" "State"
  ${MUI_INSTALLOPTIONS_READ} $AddStartMenuSC "shortcuts.ini" "Field 3" "State"
  ${MUI_INSTALLOPTIONS_READ} $AddQuickLaunchSC "shortcuts.ini" "Field 4" "State"
FunctionEnd

Function preStartMenu
  ${CheckCustomCommon}
  ${If} $AddStartMenuSC != 1
    Abort
  ${EndIf}
FunctionEnd

Function leaveStartMenu
  ${If} $InstallType == ${INSTALLTYPE_CUSTOM}
    Call CheckExistingInstall
  ${EndIf}
FunctionEnd

Function preSummary
  ; Setup the summary.ini file for the Custom Summary Page
  WriteINIStr "$PLUGINSDIR\summary.ini" "Settings" NumFields "3"

  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 1" Text   "$(SUMMARY_INSTALLED_TO)"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 1" Bottom "15"

  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" Type   "text"
  ; The contents of this control must be set as follows in the pre function
  ; ${MUI_INSTALLOPTIONS_READ} $1 "summary.ini" "Field 2" "HWND"
  ; SendMessage $1 ${WM_SETTEXT} 0 "STR:$INSTDIR"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" state  ""
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" Left   "0"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" Top    "17"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" Bottom "30"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 2" flags  "READONLY"

  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Type   "label"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Text   "$(SUMMARY_CLICK)"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Left   "0"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Top    "130"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Bottom "150"

  ${If} "$TmpVal" == "true"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Type   "label"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Text   "$(SUMMARY_REBOOT_REQUIRED_INSTALL)"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Left   "0"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Right  "-1"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Top    "35"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Bottom "45"

    WriteINIStr "$PLUGINSDIR\summary.ini" "Settings" NumFields "4"
  ${EndIf}

  ReadINIStr $0 "$PLUGINSDIR\options.ini" "Field 6" "State"
  ${If} "$0" == "1"
    ${If} "$TmpVal" == "true"
      ; To insert this control reset Top / Bottom for controls below this one
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Top    "50"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Bottom "60"
      StrCpy $0 "5"
    ${Else}
      StrCpy $0 "4"
    ${EndIf}

    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Type   "label"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Text   "$(SUMMARY_MAKE_DEFAULT)"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Left   "0"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Right  "-1"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Top    "35"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Bottom "45"

    WriteINIStr "$PLUGINSDIR\summary.ini" "Settings" NumFields "$0"
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "$(SUMMARY_PAGE_TITLE)" "$(SUMMARY_PAGE_SUBTITLE)"

  ; The Summary custom page has a textbox that will automatically receive
  ; focus. This sets the focus to the Install button instead.
  !insertmacro MUI_INSTALLOPTIONS_INITDIALOG "summary.ini"
  GetDlgItem $0 $HWNDPARENT 1
  System::Call "user32::SetFocus(i r0, i 0x0007, i,i)i"
  ${MUI_INSTALLOPTIONS_READ} $1 "summary.ini" "Field 2" "HWND"
  SendMessage $1 ${WM_SETTEXT} 0 "STR:$INSTDIR"
  !insertmacro MUI_INSTALLOPTIONS_SHOW
FunctionEnd

Function leaveSummary
  ; Try to delete the app executable and if we can't delete it try to find the
  ; app's message window and prompt the user to close the app. This allows
  ; running an instance that is located in another directory. If for whatever
  ; reason there is no message window we will just rename the app's files and
  ; then remove them on restart.
  ClearErrors
  ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${If} ${Errors}
    ${ManualCloseAppPrompt} "${WindowClass}" "$(WARN_MANUALLY_CLOSE_APP_INSTALL)"
  ${EndIf}
FunctionEnd

; When we add an optional action to the finish page the cancel button is
; enabled. This disables it and leaves the finish button as the only choice.
Function preFinish
  ${EndInstallLog} "${BrandFullName}"
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"
FunctionEnd

################################################################################
# Initialization Functions

Function .onInit
  StrCpy $LANGUAGE 0
  ${SetBrandNameVars} "$EXEDIR\localized\distribution\setup.ini"

  ${InstallOnInitCommon} "$(WARN_UNSUPPORTED_MSG)"

  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "options.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "shortcuts.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "summary.ini"

  ; Setup the options.ini file for the Custom Options Page
  WriteINIStr "$PLUGINSDIR\options.ini" "Settings" NumFields "6"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Text   "$(OPTIONS_SUMMARY)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Top    "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Bottom "10"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Text   "$(OPTION_STANDARD_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Top    "25"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Bottom "35"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Text   "$(OPTION_CUSTOM_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Top    "55"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Bottom "65"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" State  "0"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Text   "$(OPTION_STANDARD_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Top    "37"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Bottom "57"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Text   "$(OPTION_CUSTOM_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Left   "30"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Top    "67"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Bottom "87"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Text   "$(OPTIONS_MAKE_DEFAULT)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Top    "124"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" Bottom "145"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 6" State  "1"

  ; Setup the shortcuts.ini file for the Custom Shortcuts Page
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Settings" NumFields "4"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Text   "$(CREATE_ICONS_DESC)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Bottom "15"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Text   "$(ICONS_DESKTOP)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Left   "15"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Top    "20"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Bottom "30"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Text   "$(ICONS_STARTMENU)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Left   "15"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Top    "40"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Bottom "50"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" State  "1"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Text   "$(ICONS_QUICKLAUNCH)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Left   "15"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Top    "60"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Bottom "70"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" State  "1"

  ; There must always be nonlocalized and localized directories.
  ${GetSize} "$EXEDIR\nonlocalized\" "/S=0K" $R5 $R7 $R8
  ${GetSize} "$EXEDIR\localized\" "/S=0K" $R6 $R7 $R8
  IntOp $R8 $R5 + $R6
  SectionSetSize ${APP_IDX} $R8

  ; Initialize $hHeaderBitmap to prevent redundant changing of the bitmap if
  ; the user clicks the back button
  StrCpy $hHeaderBitmap ""
FunctionEnd

Function .onGUIEnd
  ${OnEndCommon}
FunctionEnd
