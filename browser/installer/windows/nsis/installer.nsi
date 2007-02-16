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

# Also requires:
# ShellLink plugin http://nsis.sourceforge.net/ShellLink_plug-in

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
Var fhInstallLog
Var fhUninstallLog

; Other included files may depend upon these includes!
; The following includes are provided by NSIS.
!include FileFunc.nsh
!include LogicLib.nsh
!include TextFunc.nsh
!include WinMessages.nsh
!include WordFunc.nsh
!include MUI.nsh

!insertmacro FileJoin
!insertmacro GetTime
!insertmacro LineFind
!insertmacro StrFilter
!insertmacro TrimNewLines
!insertmacro WordFind
!insertmacro WordReplace
!insertmacro GetSize
!insertmacro GetParameters
!insertmacro GetOptions
!insertmacro GetRoot
!insertmacro DriveSpace

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

!insertmacro RegCleanMain
!insertmacro RegCleanUninstall
!insertmacro CloseApp
!insertmacro WriteRegStr2
!insertmacro WriteRegDWORD2
!insertmacro CreateRegKey
!insertmacro CanWriteToInstallDir
!insertmacro CheckDiskSpace
!insertmacro AddHandlerValues
!insertmacro DisplayCopyErrMsg

!include shared.nsh

Name "${BrandFullName}"
OutFile "setup.exe"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})" "InstallLocation"
InstallDir "$PROGRAMFILES\${BrandFullName}"
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
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE leaveInstFiles
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

Section "-Application" Section1
  SectionIn 1 RO
  SetDetailsPrint textonly
  DetailPrint $(STATUS_CLEANUP)
  SetDetailsPrint none
  SetOutPath $INSTDIR

  ; Try to delete the app's main executable and if we can't delete it try to
  ; close the app. This allows running an instance that is located in another
  ; directory and prevents the launching of the app during the installation.
  ClearErrors
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${EndIf}
  ${If} ${Errors}
    ClearErrors
    ${CloseApp} "true" $(WARN_APP_RUNNING_INSTALL)
    ; Try to delete it again to prevent launching the app while we are
    ; installing.
    ClearErrors
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
    ${If} ${Errors}
      ClearErrors
      ; Try closing the app a second time
      ${CloseApp} "true" $(WARN_APP_RUNNING_INSTALL)
      retry:
      ClearErrors
      ${DeleteFile} "$INSTDIR\${FileMainEXE}"
      ${If} ${Errors}
        ; Fallback to the FileError_NoIgnore error with retry/cancel options
        ${DisplayCopyErrMsg} "${FileMainEXE}"
        GoTo retry
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ; During an install Vista checks if a new entry is added under the uninstall
  ; registry key (e.g. ARP). When the same version of the app is installed on
  ; top of an existing install the key is deleted / added and the Program
  ; Compatibility Assistant doesn't see this as a new entry and displays an
  ; error to the user. See Bug 354000.
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})"
  DeleteRegKey HKLM "$0"

  ; For a "Standard" upgrade without talkback installed add the InstallDisabled
  ; file to the talkback source files so it will be disabled by the extension
  ; manager. This is done at the start of the installation since we check for
  ; the existence of a directory to determine if this is an upgrade.
  ${If} $InstallType == 1
  ${AndIf} ${FileExists} "$INSTDIR\greprefs"
  ${AndIf} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
    ${Unless} ${FileExists} "$INSTDIR\extensions\talkback@mozilla.org"
      ${Unless} ${FileExists} "$INSTDIR\extensions"
        CreateDirectory "$INSTDIR\extensions"
      ${EndUnless}
      CreateDirectory "$INSTDIR\extensions\talkback@mozilla.org"
      FileOpen $2 "$EXEDIR\optional\extensions\talkback@mozilla.org\InstallDisabled" w
      FileWrite $2 "$\r$\n"
      FileClose $2
    ${EndUnless}
  ${Else}
    ; Custom installs.
    ; If DOMi is installed and this install includes DOMi remove it from
    ; the installation directory. This will remove it if the user deselected
    ; DOMi on the components page.
    ${If} ${FileExists} "$INSTDIR\extensions\inspector@mozilla.org"
    ${AndIf} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
      RmDir /r "$INSTDIR\extensions\inspector@mozilla.org"
    ${EndIf}
    ; If TalkBack is installed and this install includes TalkBack remove it from
    ; the installation directory. This will remove it if the user deselected
    ; TalkBack on the components page.
    ${If} ${FileExists} "$INSTDIR\extensions\talkback@mozilla.org"
    ${AndIf} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
      RmDir /r "$INSTDIR\extensions\talkback@mozilla.org"
    ${EndIf}
  ${EndIf}

  Call CleanupOldLogs

  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.log"
    ; Diff cleanup.log with uninstall.bak
    ${LogHeader} "Updating Uninstall Log With XPInstall Wizard Logs"
    StrCpy $R0 "$INSTDIR\uninstall\uninstall.log"
    StrCpy $R1 "$INSTDIR\uninstall\cleanup.log"
    GetTempFileName $R2
    FileOpen $R3 $R2 w
    ${TextCompareNoDetails} "$R1" "$R0" "SlowDiff" "GetDiff"
    FileClose $R3

    ${Unless} ${Errors}
      ${FileJoin} "$INSTDIR\uninstall\uninstall.log" "$R2" "$INSTDIR\uninstall\uninstall.log"
    ${EndUnless}
    ${DeleteFile} "$INSTDIR\uninstall\cleanup.log"
    ${DeleteFile} "$R2"
    ${DeleteFile} "$INSTDIR\uninstall\uninstall.bak"
    Rename "$INSTDIR\uninstall\uninstall.log" "$INSTDIR\uninstall\uninstall.bak"
  ${EndIf}

  ${Unless} ${FileExists} "$INSTDIR\uninstall"
    CreateDirectory "$INSTDIR\uninstall"
  ${EndUnless}

  FileOpen $fhUninstallLog "$INSTDIR\uninstall\uninstall.log" w
  FileOpen $fhInstallLog "$INSTDIR\install.log" w

  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  FileWrite $fhInstallLog "${BrandFullName} Installation Started: $2-$1-$0 $4:$5:$6"
  Call WriteLogSeparator

  ${LogHeader} "Installation Details"
  ${LogMsg} "Install Dir: $INSTDIR"
  ${LogMsg} "Locale     : ${AB_CD}"
  ${LogMsg} "App Version: ${AppVersion}"
  ${LogMsg} "GRE Version: ${GREVersion}"

  ${If} ${FileExists} "$EXEDIR\removed-files.log"
    ${LogHeader} "Removing Obsolete Files and Directories"
    ${LineFind} "$EXEDIR\removed-files.log" "/NUL" "1:-1" "onInstallDeleteFile"
    ${LineFind} "$EXEDIR\removed-files.log" "/NUL" "1:-1" "onInstallRemoveDir"
  ${EndIf}

  ${DeleteFile} "$INSTDIR\install_wizard.log"
  ${DeleteFile} "$INSTDIR\install_status.log"

  RmDir /r "$INSTDIR\updates"
  ${DeleteFile} "$INSTDIR\updates.xml"
  ${DeleteFile} "$INSTDIR\active-update.xml"

  SetDetailsPrint textonly
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

  SetDetailsPrint textonly
  DetailPrint $(STATUS_INSTALL_LANG)
  SetDetailsPrint none
  ${LogHeader} "Installing Localized Files"
  StrCpy $R0 "$EXEDIR\localized"
  StrCpy $R1 "$INSTDIR"
  Call DoCopyFiles

  ${If} $InstallType != 4
    Call installTalkback
    ${If} ${FileExists} "$INSTDIR\extensions\inspector@mozilla.org"
      Call installInspector
    ${EndIf}
  ${EndIf}

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
          ${LogUninstall} "File: $R3"
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
  ; install location in the Software\Mozilla key.
  SetShellVarContext current  ; Set SHCTX to HKCU
  ${RegCleanMain} "Software\Mozilla"
  SetShellVarContext all  ; Set SHCTX to HKLM
  ${RegCleanMain} "Software\Mozilla"

  ; Remove uninstall entries that point to our install location
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
    CreateDirectory "$SMPROGRAMS\$StartMenuDir"
    CreateShortCut "$SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal}.lnk"
    CreateShortCut "$SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal} ($(SAFE_MODE)).lnk" "$INSTDIR\${FileMainEXE}" "-safe-mode" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $SMPROGRAMS\$StartMenuDir\${BrandFullNameInternal} ($(SAFE_MODE)).lnk"
  ${EndIf}

  ; perhaps use the uninstall keys
  ${If} $AddQuickLaunchSC == 1
    CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $QUICKLAUNCH\${BrandFullName}.lnk"
  ${EndIf}

  ${LogHeader} "Updating Quick Launch Shortcuts"
  ${If} $AddDesktopSC == 1
    CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${LogUninstall} "File: $DESKTOP\${BrandFullName}.lnk"
  ${EndIf}

  !insertmacro MUI_STARTMENU_WRITE_END

  ; Refresh desktop icons
  System::Call "shell32::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)"
SectionEnd

Section /o "Developer Tools" Section2
  Call installInspector
SectionEnd

Section /o "Quality Feedback Agent" Section3
  Call installTalkback
SectionEnd

################################################################################
# Helper Functions

Function installInspector
  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    SetDetailsPrint textonly
    DetailPrint $(STATUS_INSTALL_OPTIONAL)
    SetDetailsPrint none
    ${RemoveDir} "$INSTDIR\extensions\inspector@mozilla.org"
    ClearErrors
    ${LogHeader} "Installing Developer Tools"
    StrCpy $R0 "$EXEDIR\optional\extensions\inspector@mozilla.org"
    StrCpy $R1 "$INSTDIR\extensions\inspector@mozilla.org"
    Call DoCopyFiles
  ${EndIf}
FunctionEnd

Function installTalkback
  StrCpy $R0 "$EXEDIR\optional\extensions\talkback@mozilla.org"
  ${If} ${FileExists} "$R0"
    SetDetailsPrint textonly
    DetailPrint $(STATUS_INSTALL_OPTIONAL)
    SetDetailsPrint none
    StrCpy $R1 "$INSTDIR\extensions\talkback@mozilla.org"
    ${If} ${FileExists} "$R1"
      ; If there is an existing InstallDisabled file copy it to the source dir.
      ; This will add it during install to the uninstall.log and retains the
      ; original disabled state from the installation.
      ${If} ${FileExists} "$R1\InstallDisabled"
        CopyFiles "$R1\InstallDisabled" "$R0"
      ${EndIf}
      ; Remove the existing install of talkback
      RmDir /r "$R1"
    ${ElseIf} $InstallType == 1
      ; For standard installations only enable talkback for the x percent as
      ; defined by the application. We use QueryPerformanceCounter for the seed
      ; since it returns a 64bit integer which should improve the accuracy.
      System::Call "kernel32::QueryPerformanceCounter(*l.r1)"
      System::Int64Op $1 % 100
      Pop $0
      ; The percentage provided by the application refers to the percentage to
      ; include so all numbers equal or greater than should be disabled.
      ${If} $0 >= ${RandomPercent}
        FileOpen $2 "$R0\InstallDisabled" w
        FileWrite $2 "$\r$\n"
        FileClose $2
      ${EndIf}
    ${EndIf}
    ClearErrors
    ${LogHeader} "Installing Quality Feedback Agent"
    Call DoCopyFiles
  ${EndIf}
FunctionEnd

; Adds a section divider to the human readable log.
Function WriteLogSeparator
  FileWrite $fhInstallLog "$\r$\n-------------------------------------------------------------------------------$\r$\n"
FunctionEnd

; Check whether to display the current page (e.g. if we aren't performing a
; custom install don't display the custom pages).
Function CheckCustom
  ${If} $InstallType != 4
    Abort
  ${EndIf}
FunctionEnd

Function onInstallDeleteFile
  ${TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 5
  ${If} $R1 == "File:"
    StrCpy $R9 "$R9" "" 6
    ${If} ${FileExists} "$INSTDIR$R9"
      ClearErrors
      Delete "$INSTDIR$R9"
      ${If} ${Errors}
        ${LogMsg} "** ERROR Deleting File: $INSTDIR$R9 **"
      ${Else}
        ${LogMsg} "Deleted File: $INSTDIR$R9"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

; The previous installer removed directories even when they aren't empty so this
; function does as well.
Function onInstallRemoveDir
  ${TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 4
  ${If} $R1 == "Dir:"
    StrCpy $R9 "$R9" "" 5
    StrCpy $R1 "$R9" "" -1
    ${If} $R1 == "\"
      StrCpy $R9 "$R9" -1
    ${EndIf}
    ${If} ${FileExists} "$INSTDIR$R9"
      ClearErrors
      RmDir /r "$INSTDIR$R9"
      ${If} ${Errors}
        ${LogMsg} "** ERROR Removing Directory: $INSTDIR$R9 **"
      ${Else}
        ${LogMsg} "Removed Directory: $INSTDIR$R9"
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

Function GetDiff
  ${TrimNewLines} "$9" "$9"
  ${If} $9 != ""
    FileWrite $R3 "$9$\r$\n"
    ${LogMsg} "Added To Uninstall Log: $9"
  ${EndIf}
  Push 0
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
        ${DisplayCopyErrMsg} "$R7"
        GoTo retry
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
        ${DisplayCopyErrMsg} "$R3"
        GoTo retry
      ${Else}
        ${LogMsg}  "Created Directory: $R1$R3"
      ${EndIf}
    ${EndUnless}
    ${If} ${FileExists} "$R1$R3\$R7"
      Delete "$R1$R3\$R7"
      ${If} ${Errors}
        ${DisplayCopyErrMsg} "$R7"
        GoTo retry
      ${EndIf}
    ${EndIf}
    ClearErrors
    CopyFiles /SILENT $R9 "$R1$R3"
    ${If} ${Errors}
      ${LogMsg} "** ERROR Installing File: $R1$R3\$R7 **"
      ${DisplayCopyErrMsg} "$R7"
      GoTo retry
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

; Clean up the old log files. We only diff the first two found since it is
; possible for there to be several MB and comparing that many would take a very
; long time to diff.
Function CleanupOldLogs
  FindFirst $0 $TmpVal "$INSTDIR\uninstall\*wizard*"
  StrCmp $TmpVal "" done
  StrCpy $TmpVal "$INSTDIR\uninstall\$TmpVal"

  FindNext $0 $1
  StrCmp $1 "" cleanup
  StrCpy $1 "$INSTDIR\uninstall\$1"
  Push $1
  Call DiffOldLogFiles
  FindClose $0
  ${DeleteFile} "$1"

  cleanup:
    StrCpy $2 "$INSTDIR\uninstall\cleanup.log"
    ${DeleteFile} "$2"
    FileOpen $R2 $2 w
    Push $TmpVal
    ${LineFind} "$INSTDIR\uninstall\$TmpVal" "/NUL" "1:-1" "CleanOldLogFilesCallback"
    ${DeleteFile} "$INSTDIR\uninstall\$TmpVal"
  done:
    FindClose $0
    FileClose $R2
    FileClose $R3
FunctionEnd

Function DiffOldLogFiles
  StrCpy $R1 "$1"
  GetTempFileName $R2
  FileOpen $R3 $R2 w
  ${TextCompareNoDetails} "$R1" "$TmpVal" "SlowDiff" "GetDiff"
  FileClose $R3
  ${FileJoin} "$TmpVal" "$R2" "$TmpVal"
  ${DeleteFile} "$R2"
FunctionEnd

Function CleanOldLogFilesCallback
  ${TrimNewLines} "$R9" $R9
  ${WordReplace} "$R9" "$INSTDIR" "" "+" $R3
  ${WordFind} "$R9" "	" "E+1}" $R0
  IfErrors updater 0

  ${WordFind} "$R0" "Installing: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "File: $R1$\r$\n"
    GoTo done
  ${EndUnless}

  ${WordFind} "$R0" "Replacing: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "File: $R1$\r$\n"
    GoTo done
  ${EndUnless}

  ${WordFind} "$R0" "Windows Shortcut: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "File: $R1.lnk$\r$\n"
    GoTo done
  ${EndUnless}

  ${WordFind} "$R0" "Create Folder: " "E+1}" $R1
  ${Unless} ${Errors}
    FileWrite $R2 "Dir: $R1$\r$\n"
    GoTo done
  ${EndUnless}

  updater:
    ${WordFind} "$R9" "installing: " "E+1}" $R0
    ${Unless} ${Errors}
      FileWrite $R2 "File: $R0$\r$\n"
    ${EndUnless}

  done:
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
  StrCpy $InstallType "1"
  ${MUI_INSTALLOPTIONS_READ} $R0 "options.ini" "Field 3" "State"
  StrCmp $R0 "1" +1 +2
  StrCpy $InstallType "4"
FunctionEnd

Function preComponents
  Call CheckCustom
  ; If DOMi isn't available skip the components page
  ${Unless} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    Abort
  ${EndUnless}
  !insertmacro MUI_HEADER_TEXT "$(OPTIONAL_COMPONENTS_TITLE)" "$(OPTIONAL_COMPONENTS_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "components.ini"
FunctionEnd

Function leaveComponents
  ; If DOMi exists then it will be Field 2.
  ; If DOMi doesn't exist and talkback exists then TalkBack will be Field 2 but
  ; if DOMi doesn't exist we won't display this page anyways.
  StrCpy $R1 2
  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    ${MUI_INSTALLOPTIONS_READ} $R0 "components.ini" "Field $R1" "State"
    ; State will be 1 for checked and 0 for unchecked so we can use that to set
    ; the section flags for installation.
    SectionSetFlags 1 $R0
    IntOp $R1 $R1 + 1
  ${Else}
    SectionSetFlags 1 0 ; Disable install for DOMi
  ${EndIf}

  ${If} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
    ${MUI_INSTALLOPTIONS_READ} $R0 "components.ini" "Field $R1" "State"
    ; State will be 1 for checked and 0 for unchecked so we can use that to set
    ; the section flags for installation.
    SectionSetFlags 2 $R0
  ${Else}
    SectionSetFlags 2 0 ; Disable install for TalkBack
  ${EndIf}
FunctionEnd

Function preDirectory
  ${If} $InstallType != 4
    ${CheckDiskSpace} $R9
    ${If} $R9 != "false"
      ${CanWriteToInstallDir} $R9
      ${If} $R9 != "false"
        Abort
      ${EndIf}
    ${EndIf}
  ${EndIf}
FunctionEnd

Function leaveDirectory
  ${CheckDiskSpace} $R9
  ${If} $R9 == "false"
    MessageBox MB_OK "$(WARN_DISK_SPACE)"
    Abort
  ${EndIf}

  ${CanWriteToInstallDir} $R9
  ${If} $R9 == "false"
    MessageBox MB_OK "$(WARN_WRITE_ACCESS)"
    Abort
  ${EndIf}
FunctionEnd

Function preShortcuts
  Call CheckCustom
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
  Call CheckCustom
  ${If} $AddStartMenuSC != 1
    Abort
  ${EndIf}
FunctionEnd

Function leaveInstFiles
  FileClose $fhUninstallLog
  ; Diff and add missing entries from the previous file log if it exists
  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.bak"
    SetDetailsPrint textonly
    DetailPrint $(STATUS_CLEANUP)
    SetDetailsPrint none
    ${LogHeader} "Updating Uninstall Log With Previous Uninstall Log"
    StrCpy $R0 "$INSTDIR\uninstall\uninstall.log"
    StrCpy $R1 "$INSTDIR\uninstall\uninstall.bak"
    GetTempFileName $R2
    FileOpen $R3 $R2 w
    ${TextCompareNoDetails} "$R1" "$R0" "SlowDiff" "GetDiff"
    FileClose $R3
    ${Unless} ${Errors}
      ${FileJoin} "$INSTDIR\uninstall\uninstall.log" "$R2" "$INSTDIR\uninstall\uninstall.log"
    ${EndUnless}
    ${DeleteFile} "$INSTDIR\uninstall\uninstall.bak"
    ${DeleteFile} "$R2"
  ${EndIf}

  Call WriteLogSeparator
  ${GetTime} "" "L" $0 $1 $2 $3 $4 $5 $6
  FileWrite $fhInstallLog "${BrandFullName} Installation Finished: $2-$1-$0 $4:$5:$6$\r$\n"
  FileClose $fhInstallLog
FunctionEnd

; When we add an optional action to the finish page the cancel button is
; enabled. This disables it and leaves the finish button as the only choice.
Function preFinish
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"
FunctionEnd

################################################################################
# Initialization Functions

Function .onInit
  ${GetParameters} $R0
  ${If} $R0 != ""
    ClearErrors
    ${GetOptions} "$R0" "-ms" $R1
    ${If} ${Errors}
      ; Default install type
      StrCpy $InstallType "1"
      ; Support for specifying an installation configuration file.
      ClearErrors
      ${GetOptions} "$R0" "/INI=" $R1
      ${Unless} ${Errors}
        ; The configuration file must also exist
        ${If} ${FileExists} "$R1"
          SetSilent silent
          ReadINIStr $0 $R1 "Install" "InstallDirectoryName"
          ${If} $0 != ""
            StrCpy $INSTDIR "$PROGRAMFILES\$0"
          ${Else}
            ReadINIStr $0 $R1 "Install" "InstallDirectoryPath"
            ${If} $$0 != ""
              StrCpy $INSTDIR "$0"
            ${EndIf}
          ${EndIf}

          ${If} $INSTDIR == ""
            ; Check if there is an existing uninstall registry entry for this
            ; version of the application and if present install into that location
            ReadRegStr $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})" "InstallLocation"
            ${If} $0 == ""
              StrCpy $INSTDIR "$PROGRAMFILES\${BrandFullName}"
            ${Else}
              GetFullPathName $INSTDIR "$0"
              ${Unless} ${FileExists} "$INSTDIR"
                StrCpy $INSTDIR "$PROGRAMFILES\${BrandFullName}"
              ${EndUnless}
            ${EndIf}
          ${EndIf}

          ; Quit if we are unable to create the installation directory or we are
          ; unable to write to a file in the installation directory.
          ClearErrors
          ${If} ${FileExists} "$INSTDIR"
            GetTempFileName $R2 "$INSTDIR"
            FileOpen $R3 $R2 w
            FileWrite $R3 "Write Access Test"
            FileClose $R3
            Delete $R2
            ${If} ${Errors}
              Quit
            ${EndIf}
          ${Else}
            CreateDirectory "$INSTDIR"
            ${If} ${Errors}
              Quit
            ${EndIf}
          ${EndIf}

          ReadINIStr $0 $R1 "Install" "CloseAppNoPrompt"
          ${If} $0 == "true"
            ; Try to close the app if the exe is in use.
            ClearErrors
            ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
              ${DeleteFile} "$INSTDIR\${FileMainEXE}"
            ${EndIf}
            ${If} ${Errors}
              ClearErrors
              ${CloseApp} "false" ""
              ClearErrors
              ${DeleteFile} "$INSTDIR\${FileMainEXE}"
              ; If unsuccessful try one more time and if it still fails Quit
              ${If} ${Errors}
                ClearErrors
                ${CloseApp} "false" ""
                ClearErrors
                ${DeleteFile} "$INSTDIR\${FileMainEXE}"
                ${If} ${Errors}
                  Quit
                ${EndIf}
              ${EndIf}
            ${EndIf}
          ${EndIf}

          ReadINIStr $0 $R1 "Install" "QuickLaunchShortcut"
          ${If} $0 == "false"
            StrCpy $AddQuickLaunchSC "0"
          ${Else}
            StrCpy $AddQuickLaunchSC "1"
          ${EndIf}

          ReadINIStr $0 $R1 "Install" "DesktopShortcut"
          ${If} $0 == "false"
            StrCpy $AddDesktopSC "0"
          ${Else}
            StrCpy $AddDesktopSC "1"
          ${EndIf}

          ReadINIStr $0 $R1 "Install" "StartMenuShortcuts"
          ${If} $0 == "false"
            StrCpy $AddStartMenuSC "0"
          ${Else}
            StrCpy $AddStartMenuSC "1"
          ${EndIf}

          ReadINIStr $0 $R1 "Install" "StartMenuDirectoryName"
          ${If} $0 != ""
            StrCpy $StartMenuDir "$0"
          ${EndIf}
        ${EndIf}
      ${EndUnless}
    ${Else}
      ; Support for the deprecated -ms command line argument. The new command
      ; line arguments are not supported when -ms is used.
      SetSilent silent
    ${EndIf}
  ${EndIf}
  ClearErrors

  StrCpy $LANGUAGE 0
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "options.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "components.ini"
  !insertmacro MUI_INSTALLOPTIONS_EXTRACT "shortcuts.ini"
  !insertmacro createBasicCustomOptionsINI
  !insertmacro createComponentsINI
  !insertmacro createShortcutsINI

  ; There must always be nonlocalized and localized directories.
  ${GetSize} "$EXEDIR\nonlocalized\" "/S=0K" $1 $8 $9
  ${GetSize} "$EXEDIR\localized\" "/S=0K" $2 $8 $9
  IntOp $0 $1 + $2
  SectionSetSize 0 $0

  ${If} ${FileExists} "$EXEDIR\optional\extensions\inspector@mozilla.org"
    ; Set the section size for DOMi.
    ${GetSize} "$EXEDIR\optional\extensions\inspector@mozilla.org" "/S=0K" $0 $8 $9
    SectionSetSize 1 $0
  ${Else}
    ; Hide DOMi in the components page if it isn't available.
    SectionSetText 1 ""
  ${EndIf}

  ; Set the section size for Talkback only if it exists.
  ${If} ${FileExists} "$EXEDIR\optional\extensions\talkback@mozilla.org"
    ${GetSize} "$EXEDIR\optional\extensions\talkback@mozilla.org" "/S=0K" $0 $8 $9
    SectionSetSize 2 $0
    ; Install Talkback by default.
    SectionSetFlags 2 1
  ${Else}
    ; Hide Talkback in the components page if it isn't available.
    SectionSetText 2 ""
  ${EndIf}
FunctionEnd
