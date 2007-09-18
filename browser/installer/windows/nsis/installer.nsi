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
# ShellLink    http://nsis.sourceforge.net/ShellLink_plug-in

; Set verbosity to 3 (e.g. no script) to lessen the noise in the build logs
!verbose 3

; 7-Zip provides better compression than the lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
SetDatablockOptimize on
SetCompress off
CRCCheck on

!addplugindir ./

; empty files - except for the comment line - for generating custom pages.
!system 'echo ; > options.ini'
!system 'echo ; > components.ini'
!system 'echo ; > shortcuts.ini'

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
!include TextFunc.nsh
!include WinMessages.nsh
!include WordFunc.nsh
!include MUI.nsh

; WinVer.nsh was added in the same release that RequestExecutionLevel so check
; if ___WINVER__NSH___ is defined to determine if RequestExecutionLevel is
; available.
!include /NONFATAL WinVer.nsh
!ifdef ___WINVER__NSH___
  RequestExecutionLevel admin
!else
  !warning "Installer will be created without Vista compatibility.$\n            \
            Upgrade your NSIS installation to at least version 2.22 to resolve."
!endif

!insertmacro StrFilter
!insertmacro WordFind
!insertmacro WordReplace
!insertmacro GetSize

; NSIS provided macros that we have overridden
!include overrides.nsh
!insertmacro LocateNoDetails
!insertmacro TextCompareNoDetails

; The following includes are custom.
!include branding.nsi
!include defines.nsi
!include common.nsh
!include locales.nsi
!include version.nsh

VIAddVersionKey "FileDescription" "${BrandShortName} Installer"

; Must be inserted before other macros that use logging
!insertmacro _LoggingCommon

!insertmacro AddHandlerValues
!insertmacro CloseApp
!insertmacro CreateRegKey
!insertmacro RegCleanMain
!insertmacro RegCleanUninstall
!insertmacro WriteRegStr2
!insertmacro WriteRegDWORD2

!include shared.nsh

; Helper macros for ui callbacks. Insert these after shared.nsh
!insertmacro CheckCustomCommon
!insertmacro InstallEndCleanupCommon
!insertmacro InstallOnInitCommon
!insertmacro InstallStartCleanupCommon
!insertmacro LeaveDirectoryCommon
!insertmacro PreDirectoryCommon

Name "${BrandFullName}"
OutFile "setup.exe"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})" "InstallLocation"
InstallDir "$PROGRAMFILES\${BrandFullName}\"
ShowInstDetails nevershow

ReserveFile options.ini
ReserveFile components.ini
ReserveFile shortcuts.ini

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
!insertmacro MUI_PAGE_WELCOME

; License Page
!define MUI_LICENSEPAGE_RADIOBUTTONS
!insertmacro MUI_PAGE_LICENSE license.rtf

; Custom Options Page
Page custom preOptions leaveOptions

; Custom Components Page
Page custom preComponents leaveComponents

; Select Install Directory Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE preDirectory
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE leaveDirectory
!define MUI_DIRECTORYPAGE_VERIFYONLEAVE
!insertmacro MUI_PAGE_DIRECTORY

; Custom Shortcuts Page
Page custom preShortcuts leaveShortcuts

; Start Menu Folder Page Configuration
!define MUI_PAGE_CUSTOMFUNCTION_PRE preStartMenu
!define MUI_STARTMENUPAGE_NODISABLE
!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Main"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!insertmacro MUI_PAGE_STARTMENU Application $StartMenuDir

; Install Files Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE preInstFiles
!insertmacro MUI_PAGE_INSTFILES

; Finish Page
!define MUI_FINISHPAGE_NOREBOOTSUPPORT
!define MUI_FINISHPAGE_TITLE_3LINES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_FUNCTION LaunchApp
!define MUI_FINISHPAGE_RUN_TEXT $(LAUNCH_TEXT)
!define MUI_PAGE_CUSTOMFUNCTION_PRE preFinish
!insertmacro MUI_PAGE_FINISH

################################################################################
# Install Sections

; Cleanup operations to perform at the start of the installation.
Section "-InstallStartCleanup"
  SetDetailsPrint both
  DetailPrint $(STATUS_CLEANUP)
  SetDetailsPrint none

  SetOutPath $INSTDIR
  ${StartInstallLog} "${BrandFullName}" "${AB_CD}" "${AppVersion}" "${GREVersion}"

  ; Try to delete the app's main executable and if we can't delete it try to
  ; close the app. This allows running an instance that is located in another
  ; directory and prevents the launching of the app during the installation.
  ; A copy of the executable is placed in a temporary directory so it can be
  ; copied back in the case where a specific file is checked / found to be in
  ; use that would prevent a successful install.

  ; Create a temporary backup directory.
  GetTempFileName $TmpVal "$TEMP"
  ${DeleteFile} $TmpVal
  SetOutPath $TmpVal

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ClearErrors
    CopyFiles /SILENT "$INSTDIR\${FileMainEXE}" "$TmpVal\${FileMainEXE}"
    Delete "$INSTDIR\${FileMainEXE}"
    ${If} ${Errors}
      ClearErrors
      ${CloseApp} "true" $(WARN_APP_RUNNING_INSTALL)
      ; Try to delete it again to prevent launching the app while we are
      ; installing.
      ClearErrors
      CopyFiles /SILENT "$INSTDIR\${FileMainEXE}" "$TmpVal\${FileMainEXE}"
      Delete "$INSTDIR\${FileMainEXE}"
      ${If} ${Errors}
        ClearErrors
        ; Try closing the app a second time
        ${CloseApp} "true" $(WARN_APP_RUNNING_INSTALL)
        StrCpy $R1 "${FileMainEXE}"
        Call CheckInUse
      ${EndIf}
    ${EndIf}
  ${EndIf}

  StrCpy $R1 "freebl3.dll"
  Call CheckInUse

  StrCpy $R1 "nssckbi.dll"
  Call CheckInUse

  StrCpy $R1 "nspr4.dll"
  Call CheckInUse

  StrCpy $R1 "xpicleanup.exe"
  Call CheckInUse

  SetOutPath $INSTDIR
  RmDir /r "$TmpVal"
  ClearErrors

  ${If} $InstallType == ${INSTALLTYPE_CUSTOM}
    ; Custom installs.
    ; If DOMi is installed and this install includes DOMi remove it from
    ; the installation directory. This will remove it if the user deselected
    ; DOMi on the components page.
    ${If} ${FileExists} "$INSTDIR\extensions\inspector@mozilla.org"
    ${AndIf} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
      RmDir /r "$INSTDIR\extensions\inspector@mozilla.org"
    ${EndIf}
  ${EndIf}

  ${InstallStartCleanupCommon}
SectionEnd

Section "-Application" APP_IDX
  ${StartUninstallLog}

  SetDetailsPrint both
  DetailPrint $(STATUS_INSTALL_APP)
  SetDetailsPrint none

  ${LogHeader} "Installing Main Files"
  StrCpy $R0 "$EXEDIR\nonlocalized"
  StrCpy $R1 "$INSTDIR"
  Call DoCopyFiles

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
  StrCpy $R0 "$EXEDIR\localized"
  StrCpy $R1 "$INSTDIR"
  Call DoCopyFiles

  ${LogHeader} "Adding Additional Files"
  ; Check if QuickTime is installed and copy the nsIQTScriptablePlugin.xpt from
  ; its plugins directory into the app's components directory.
  ClearErrors
  ReadRegStr $R0 HKLM "Software\Apple Computer, Inc.\QuickTime" "InstallDir"
  ${Unless} ${Errors}
    Push $R0
    ${GetPathFromRegStr}
    Pop $R0
    ${Unless} ${Errors}
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

  ; Remove registry entries for non-existent apps and for apps that point to our
  ; install location in the Software\Mozilla key and uninstall registry entries
  ; that point to our install location for both HKCU and HKLM.
  SetShellVarContext current  ; Set SHCTX to HKCU
  ${RegCleanMain} "Software\Mozilla"
  ${RegCleanUninstall}

  SetShellVarContext all  ; Set SHCTX to HKLM
  ${RegCleanMain} "Software\Mozilla"
  ${RegCleanUninstall}

  ${LogHeader} "Adding Registry Entries"
  ClearErrors
  WriteRegStr HKLM "Software\Mozilla\InstallerTest" "InstallerTest" "Test"
  ${If} ${Errors}
    SetShellVarContext current  ; Set SHCTX to HKCU
    StrCpy $TmpVal "HKCU" ; used primarily for logging
  ${Else}
    SetShellVarContext all  ; Set SHCTX to HKLM
    DeleteRegKey HKLM "Software\Mozilla\InstallerTest"
    StrCpy $TmpVal "HKLM" ; used primarily for logging
  ${EndIf}

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

  ; The following keys should only be set if we can write to HKLM
  ${If} $TmpVal == "HKLM"
    ; Uninstall keys can only exist under HKLM on some versions of windows.
    ${SetUninstallKeys}

    ; Set the Start Menu Internet and Vista Registered App HKLM registry keys.
    ${SetStartMenuInternet}

    ; If we are writing to HKLM and create the quick launch and the desktop
    ; shortcuts set IconsVisible to 1 otherwise to 0.
    ${If} $AddQuickLaunchSC == 1
    ${OrIf} $AddDesktopSC == 1
      ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
      StrCpy $0 "Software\Clients\StartMenuInternet\$R9\InstallInfo"
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

Section /o "Developer Tools" DOMI_IDX
  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    SetDetailsPrint both
    DetailPrint $(STATUS_INSTALL_OPTIONAL)
    SetDetailsPrint none

    ${RemoveDir} "$INSTDIR\extensions\inspector@mozilla.org"
    ClearErrors
    ${LogHeader} "Installing Developer Tools"
    StrCpy $R0 "$EXEDIR\optional\extensions\inspector@mozilla.org"
    StrCpy $R1 "$INSTDIR\extensions\inspector@mozilla.org"
    Call DoCopyFiles
  ${EndIf}
SectionEnd

; Cleanup operations to perform at the end of the installation.
Section "-InstallEndCleanup"
  SetDetailsPrint both
  DetailPrint "$(STATUS_CLEANUP)"
  SetDetailsPrint none
  ${LogHeader} "Updating Uninstall Log With Previous Uninstall Log"

  ${InstallEndCleanupCommon}

  ; Refresh desktop icons
  System::Call "shell32::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)"
SectionEnd

################################################################################
# Helper Functions

; Copies a file to a temporary backup directory and then checks if it is in use
; by attempting to delete the file. If the file is in use an error is displayed
; and the user is given the options to either retry or cancel. If cancel is
; selected then the files are restored.
Function CheckInUse
  ${If} ${FileExists} "$INSTDIR\$R1"
    retry:
    ClearErrors
    CopyFiles /SILENT "$INSTDIR\$R1" "$TmpVal\$R1"
    ${Unless} ${Errors}
      Delete "$INSTDIR\$R1"
    ${EndUnless}
    ${If} ${Errors}
      StrCpy $0 "$INSTDIR\$R1"
      ${WordReplace} "$(^FileError_NoIgnore)" "\r\n" "$\r$\n" "+*" $0
      MessageBox MB_RETRYCANCEL|MB_ICONQUESTION "$0" IDRETRY retry
      Delete "$TmpVal\$R1"
      CopyFiles /SILENT "$TmpVal\*" "$INSTDIR\"
      SetOutPath $INSTDIR
      RmDir /r "$TmpVal"
      Quit
    ${EndIf}
  ${EndIf}
FunctionEnd

Function DoCopyFiles
  StrLen $R2 $R0
  ${LocateNoDetails} "$R0" "/L=FD" "CopyFile"
FunctionEnd

Function CopyFile
  StrCpy $R3 $R8 "" $R2
  retry:
  ClearErrors
  ${If} $R6 ==  ""
    ${Unless} ${FileExists} "$R1$R3\$R7"
      ClearErrors
      CreateDirectory "$R1$R3\$R7"
      ${If} ${Errors}
        ${LogMsg}  "** ERROR Creating Directory: $R1$R3\$R7 **"
        StrCpy $0 "$R1$R3\$R7"
        ${WordReplace} "$(^FileError_NoIgnore)" "\r\n" "$\r$\n" "+*" $0
        MessageBox MB_RETRYCANCEL|MB_ICONQUESTION "$0" IDRETRY retry
        Quit
      ${Else}
        ${LogMsg}  "Created Directory: $R1$R3\$R7"
      ${EndIf}
    ${EndUnless}
  ${Else}
    ${Unless} ${FileExists} "$R1$R3"
      ClearErrors
      CreateDirectory "$R1$R3"
      ${If} ${Errors}
        ${LogMsg}  "** ERROR Creating Directory: $R1$R3 **"
        StrCpy $0 "$R1$R3"
        ${WordReplace} "$(^FileError_NoIgnore)" "\r\n" "$\r$\n" "+*" $0
        MessageBox MB_RETRYCANCEL|MB_ICONQUESTION "$0" IDRETRY retry
        Quit
      ${Else}
        ${LogMsg}  "Created Directory: $R1$R3"
      ${EndIf}
    ${EndUnless}
    ${If} ${FileExists} "$R1$R3\$R7"
      ClearErrors
      Delete "$R1$R3\$R7"
      ${If} ${Errors}
        ${LogMsg} "** ERROR Deleting File: $R1$R3\$R7 **"
        StrCpy $0 "$R1$R3\$R7"
        ${WordReplace} "$(^FileError_NoIgnore)" "\r\n" "$\r$\n" "+*" $0
        MessageBox MB_RETRYCANCEL|MB_ICONQUESTION "$0" IDRETRY retry
        Quit
      ${EndIf}
    ${EndIf}
    ClearErrors

    CopyFiles /SILENT $R9 "$R1$R3"

    ${If} ${Errors}
      ${LogMsg} "** ERROR Installing File: $R1$R3\$R7 **"
      StrCpy $0 "$R1$R3\$R7"
      ${WordReplace} "$(^FileError_NoIgnore)" "\r\n" "$\r$\n" "+*" $0
      MessageBox MB_RETRYCANCEL|MB_ICONQUESTION "$0" IDRETRY retry
      Quit
    ${Else}
      ${LogMsg} "Installed File: $R1$R3\$R7"
    ${EndIf}
    ; If the file is installed into the installation directory remove the
    ; installation directory's path from the file path when writing to the
    ; uninstall.log so it will be a relative path. This allows the same
    ; helper.exe to be used with zip builds if we supply an uninstall.log.
    ${WordReplace} "$R1$R3\$R7" "$INSTDIR" "" "+" $R3
    ${LogUninstall} "File: $R3"
  ${EndIf}
  Push 0
FunctionEnd

Function LaunchApp
  ${CloseApp} "true" $(WARN_APP_RUNNING_INSTALL)
  Exec "$INSTDIR\${FileMainEXE}"
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
# Page pre and leave functions

Function preOptions
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
FunctionEnd

Function preComponents
  ${CheckCustomCommon}
  ; If DOMi isn't available skip the components page
  ${Unless} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    Abort
  ${EndUnless}
  !insertmacro MUI_HEADER_TEXT "$(OPTIONAL_COMPONENTS_TITLE)" "$(OPTIONAL_COMPONENTS_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "components.ini"
FunctionEnd

Function leaveComponents
  ${MUI_INSTALLOPTIONS_READ} $R0 "components.ini" "Field 2" "State"
  ; State will be 1 for checked and 0 for unchecked so we can use that to set
  ; the section flags for installation.
  SectionSetFlags ${DOMI_IDX} $R0
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

Function preInstFiles
  ${If} $InstallType != ${INSTALLTYPE_CUSTOM}
    ; Set DOMi to be installed
    SectionSetFlags ${DOMI_IDX} 1
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
  ${InstallOnInitCommon} "$(WARN_UNSUPPORTED_MSG)"

  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "options.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "components.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "shortcuts.ini"
  !insertmacro createBasicCustomOptionsINI
  !insertmacro createComponentsINI
  !insertmacro createShortcutsINI

  StrCpy $LANGUAGE 0

  ; There must always be nonlocalized and localized directories.
  ${GetSize} "$EXEDIR\nonlocalized\" "/S=0K" $R5 $R7 $R8
  ${GetSize} "$EXEDIR\localized\" "/S=0K" $R6 $R7 $R8
  IntOp $R8 $R5 + $R6
  SectionSetSize ${APP_IDX} $R8

  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    ; Set the section size for DOMi.
    ${GetSize} "$EXEDIR\optional\extensions\inspector@mozilla.org" "/S=0K" $0 $8 $9
    SectionSetSize ${DOMI_IDX} $0
  ${Else}
    ; Hide DOMi in the components page if it isn't available.
    SectionSetText ${DOMI_IDX} ""
  ${EndIf}
FunctionEnd
