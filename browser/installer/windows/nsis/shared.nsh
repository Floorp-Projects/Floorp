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

!macro PostUpdate
  ${CreateShortcutsLog}

  ; Remove registry entries for non-existent apps and for apps that point to our
  ; install location in the Software\Mozilla key and uninstall registry entries
  ; that point to our install location for both HKCU and HKLM.
  SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
  ${RegCleanMain} "Software\Mozilla"
  ${RegCleanUninstall}
  ${UpdateProtocolHandlers}

  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    StrCpy $TmpVal "HKCU" ; used primarily for logging
  ${Else}
    SetShellVarContext all    ; Set SHCTX to all users (e.g. HKLM)
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    StrCpy $TmpVal "HKLM" ; used primarily for logging
    ${RegCleanMain} "Software\Mozilla"
    ${RegCleanUninstall}
    ${UpdateProtocolHandlers}
    ${FixShellIconHandler}
    ${SetAppLSPCategories} ${LSP_CATEGORIES}

    ; Only update the Clients\StartMenuInternet registry key values if they
    ; don't exist or this installation is the same as the one set in those keys.
    ${StrFilter} "${FileMainEXE}" "+" "" "" $1
    ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$1\DefaultIcon" ""
    ${GetPathFromString} "$0" $0
    ${GetParent} "$0" $0
    ${If} ${FileExists} "$0"
      ${GetLongPath} "$0" $0
    ${EndIf}
    ${If} "$0" == "$INSTDIR"
      ${SetStartMenuInternet}
    ${EndIf}

    ReadRegStr $0 HKLM "Software\mozilla.org\Mozilla" "CurrentVersion"
    ${If} "$0" != "${GREVersion}"
      WriteRegStr HKLM "Software\mozilla.org\Mozilla" "CurrentVersion" "${GREVersion}"
    ${EndIf}
  ${EndIf}

  ${RemoveDeprecatedKeys}

  ${SetAppKeys}
  ${FixClassKeys}
  ${SetUninstallKeys}

  ; Win7 taskbar and start menu link maintenance
  ${UpdateShortcutAppModelIDs} "$INSTDIR\${FileMainEXE}" "${AppUserModelID}" $0

  ; Remove files that may be left behind by the application in the
  ; VirtualStore directory.
  ${CleanVirtualStore}

  ; Remove talkback if it is present (remove after bug 386760 is fixed)
  ${If} ${FileExists} "$INSTDIR\extensions\talkback@mozilla.org\"
    RmDir /r "$INSTDIR\extensions\talkback@mozilla.org\"
  ${EndIf}
!macroend
!define PostUpdate "!insertmacro PostUpdate"

!macro SetAsDefaultAppGlobal
  ${RemoveDeprecatedKeys}

  SetShellVarContext all      ; Set SHCTX to all users (e.g. HKLM)
  ${SetHandlers}
  ${SetStartMenuInternet}
  ${FixShellIconHandler}
  ${ShowShortcuts}
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  WriteRegStr HKLM "Software\Clients\StartMenuInternet" "" "$R9"
!macroend
!define SetAsDefaultAppGlobal "!insertmacro SetAsDefaultAppGlobal"

; Removes shortcuts for this installation. This should also remove the
; application from Open With for the file types the application handles
; (bug 370480).
!macro HideShortcuts
  ${StrFilter} "${FileMainEXE}" "+" "" "" $0
  StrCpy $R1 "Software\Clients\StartMenuInternet\$0\InstallInfo"
  WriteRegDWORD HKLM "$R1" "IconsVisible" 0
  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
  ${EndUnless}

  ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$DESKTOP\${BrandFullName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$DESKTOP\${BrandFullName}.lnk"
      Pop $0
      ; Needs to handle short paths
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$DESKTOP\${BrandFullName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$QUICKLAUNCH\${BrandFullName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$QUICKLAUNCH\${BrandFullName}.lnk"
      Pop $0
      ; Needs to handle short paths
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$QUICKLAUNCH\${BrandFullName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define HideShortcuts "!insertmacro HideShortcuts"

; Adds shortcuts for this installation. This should also add the application
; to Open With for the file types the application handles (bug 370480).
!macro ShowShortcuts
  ${StrFilter} "${FileMainEXE}" "+" "" "" $0
  StrCpy $R1 "Software\Clients\StartMenuInternet\$0\InstallInfo"
  WriteRegDWORD HKLM "$R1" "IconsVisible" 1
  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ApplicationID::Set "$DESKTOP\${BrandFullName}.lnk" "${AppUserModelID}"
    ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR"
    ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
      ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
        CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
        ApplicationID::Set "$DESKTOP\${BrandFullName}.lnk" "${AppUserModelID}"
        ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR"
      ${EndUnless}
    ${EndUnless}
  ${EndUnless}
  ${Unless} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ApplicationID::Set "$QUICKLAUNCH\${BrandFullName}.lnk" "${AppUserModelID}"
    ShellLink::SetShortCutWorkingDirectory "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR"
  ${EndUnless}
!macroend
!define ShowShortcuts "!insertmacro ShowShortcuts"

; Adds the protocol and file handler registry entries for making Firefox the
; default handler (uses SHCTX).
!macro SetHandlers
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8

  StrCpy $0 "SOFTWARE\Classes"
  StrCpy $2 "$\"$8$\" -requestPending -osint -url $\"%1$\""

  ; Associate the file handlers with FirefoxHTML
  ReadRegStr $6 SHCTX "$0\.htm" ""
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.htm"   "" "FirefoxHTML"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.html" ""
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.html"  "" "FirefoxHTML"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.shtml" ""
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.shtml" "" "FirefoxHTML"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.xht" ""
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.xht"   "" "FirefoxHTML"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.xhtml" ""
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.xhtml" "" "FirefoxHTML"
  ${EndIf}

  StrCpy $3 "$\"%1$\",,0,0,,,,"

  ; An empty string is used for the 5th param because FirefoxHTML is not a
  ; protocol handler
  ${AddDDEHandlerValues} "FirefoxHTML" "$2" "$8,1" "${AppRegName} Document" "" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"

  ${AddDDEHandlerValues} "FirefoxURL" "$2" "$8,1" "${AppRegName} URL" "true" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"

  ; An empty string is used for the 4th & 5th params because the following
  ; protocol handlers already have a display name and the additional keys
  ; required for a protocol handler.
  ${AddDDEHandlerValues} "ftp" "$2" "$8,1" "" "" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"
  ${AddDDEHandlerValues} "http" "$2" "$8,1" "" "" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"
  ${AddDDEHandlerValues} "https" "$2" "$8,1" "" "" \
                         "${DDEApplication}" "$3" "WWW_OpenURL"
!macroend
!define SetHandlers "!insertmacro SetHandlers"

; Adds the HKLM\Software\Clients\StartMenuInternet\FIREFOX.EXE registry
; entries (does not use SHCTX).
;
; The values for StartMenuInternet are only valid under HKLM and there can only
; be one installation registerred under StartMenuInternet per application since
; the key name is derived from the main application executable.
; http://support.microsoft.com/kb/297878
;
; Note: we might be able to get away with using the full path to the
; application executable for the key name in order to support multiple
; installations.
!macro SetStartMenuInternet
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  ${GetLongPath} "$INSTDIR\uninstall\helper.exe" $7

  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9

  StrCpy $0 "Software\Clients\StartMenuInternet\$R9"

  WriteRegStr HKLM "$0" "" "${BrandFullName}"

  WriteRegStr HKLM "$0\DefaultIcon" "" "$8,0"

  ; The Reinstall Command is defined at
  ; http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_adv/registeringapps.asp
  WriteRegStr HKLM "$0\InstallInfo" "HideIconsCommand" "$\"$7$\" /HideShortcuts"
  WriteRegStr HKLM "$0\InstallInfo" "ShowIconsCommand" "$\"$7$\" /ShowShortcuts"
  WriteRegStr HKLM "$0\InstallInfo" "ReinstallCommand" "$\"$7$\" /SetAsDefaultAppGlobal"

  ClearErrors
  ReadRegDWORD $1 HKLM "$0\InstallInfo" "IconsVisible"
  ; If the IconsVisible name value pair doesn't exist add it otherwise the
  ; application won't be displayed in Set Program Access and Defaults.
  ${If} ${Errors}
    ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
      WriteRegDWORD HKLM "$0\InstallInfo" "IconsVisible" 1
    ${Else}
      WriteRegDWORD HKLM "$0\InstallInfo" "IconsVisible" 0
    ${EndIf}
  ${EndIf}

  WriteRegStr HKLM "$0\shell\open\command" "" "$8"

  WriteRegStr HKLM "$0\shell\properties" "" "$(CONTEXT_OPTIONS)"
  WriteRegStr HKLM "$0\shell\properties\command" "" "$\"$8$\" -preferences"

  WriteRegStr HKLM "$0\shell\safemode" "" "$(CONTEXT_SAFE_MODE)"
  WriteRegStr HKLM "$0\shell\safemode\command" "" "$\"$8$\" -safe-mode"

  ; Vista Capabilities registry keys
  WriteRegStr HKLM "$0\Capabilities" "ApplicationDescription" "$(REG_APP_DESC)"
  WriteRegStr HKLM "$0\Capabilities" "ApplicationIcon" "$8,0"
  WriteRegStr HKLM "$0\Capabilities" "ApplicationName" "${BrandShortName}"

  WriteRegStr HKLM "$0\Capabilities\FileAssociations" ".htm"   "FirefoxHTML"
  WriteRegStr HKLM "$0\Capabilities\FileAssociations" ".html"  "FirefoxHTML"
  WriteRegStr HKLM "$0\Capabilities\FileAssociations" ".shtml" "FirefoxHTML"
  WriteRegStr HKLM "$0\Capabilities\FileAssociations" ".xht"   "FirefoxHTML"
  WriteRegStr HKLM "$0\Capabilities\FileAssociations" ".xhtml" "FirefoxHTML"

  WriteRegStr HKLM "$0\Capabilities\StartMenu" "StartMenuInternet" "$R9"

  WriteRegStr HKLM "$0\Capabilities\URLAssociations" "ftp"    "FirefoxURL"
  WriteRegStr HKLM "$0\Capabilities\URLAssociations" "http"   "FirefoxURL"
  WriteRegStr HKLM "$0\Capabilities\URLAssociations" "https"  "FirefoxURL"

  ; Vista Registered Application
  WriteRegStr HKLM "Software\RegisteredApplications" "${AppRegName}" "$0\Capabilities"
!macroend
!define SetStartMenuInternet "!insertmacro SetStartMenuInternet"

; The IconHandler reference for FirefoxHTML can end up in an inconsistent state
; due to changes not being detected by the IconHandler for side by side
; installs (see bug 268512). The symptoms can be either an incorrect icon or no
; icon being displayed for files associated with Firefox (does not use SHCTX).
!macro FixShellIconHandler
  ClearErrors
  ReadRegStr $1 HKLM "Software\Classes\FirefoxHTML\ShellEx\IconHandler" ""
  ${Unless} ${Errors}
    ReadRegStr $1 HKLM "Software\Classes\FirefoxHTML\" ""
    ${GetLongPath} "$INSTDIR\${FileMainEXE}" $2
    ${If} "$1" != "$2,1"
      WriteRegStr HKLM "Software\Classes\FirefoxHTML\DefaultIcon" "" "$2,1"
    ${EndIf}
  ${EndUnless}
!macroend
!define FixShellIconHandler "!insertmacro FixShellIconHandler"

; Add Software\Mozilla\ registry entries (uses SHCTX).
!macro SetAppKeys
  ${GetLongPath} "$INSTDIR" $8
  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Main"
  ${WriteRegStr2} $TmpVal "$0" "Install Directory" "$8" 0
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$8\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Uninstall"
  ${WriteRegStr2} $TmpVal "$0" "Description" "${BrandFullNameInternal} ${AppVersion} (${ARCH} ${AB_CD})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})"
  ${WriteRegStr2} $TmpVal  "$0" "" "${AppVersion} (${AB_CD})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}\bin"
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$8\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}\extensions"
  ${WriteRegStr2} $TmpVal "$0" "Components" "$8\components" 0
  ${WriteRegStr2} $TmpVal "$0" "Plugins" "$8\plugins" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}"
  ${WriteRegStr2} $TmpVal "$0" "GeckoVer" "${GREVersion}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}"
  ${WriteRegStr2} $TmpVal "$0" "" "${GREVersion}" 0
  ${WriteRegStr2} $TmpVal "$0" "CurrentVersion" "${AppVersion} (${AB_CD})" 0
!macroend
!define SetAppKeys "!insertmacro SetAppKeys"

; Add uninstall registry entries. This macro tests for write access to determine
; if the uninstall keys should be added to HKLM or HKCU.
!macro SetUninstallKeys
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} ${AppVersion} (${ARCH} ${AB_CD})"

  WriteRegStr HKLM "$0" "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    StrCpy $1 "HKCU"
    SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
  ${Else}
    StrCpy $1 "HKLM"
    SetShellVarContext all     ; Set SHCTX to all users (e.g. HKLM)
    DeleteRegValue HKLM "$0" "${BrandShortName}InstallerTest"
  ${EndIf}

  ${GetLongPath} "$INSTDIR" $8

  ; Write the uninstall registry keys
  ${WriteRegStr2} $1 "$0" "Comments" "${BrandFullNameInternal} ${AppVersion} (${ARCH} ${AB_CD})" 0
  ${WriteRegStr2} $1 "$0" "DisplayIcon" "$8\${FileMainEXE},0" 0
  ${WriteRegStr2} $1 "$0" "DisplayName" "${BrandFullNameInternal} ${AppVersion} (${ARCH} ${AB_CD})" 0
  ${WriteRegStr2} $1 "$0" "DisplayVersion" "${AppVersion}" 0
  ${WriteRegStr2} $1 "$0" "InstallLocation" "$8" 0
  ${WriteRegStr2} $1 "$0" "Publisher" "Mozilla" 0
  ${WriteRegStr2} $1 "$0" "UninstallString" "$8\uninstall\helper.exe" 0
  ${WriteRegStr2} $1 "$0" "URLInfoAbout" "${URLInfoAbout}" 0
  ${WriteRegStr2} $1 "$0" "URLUpdateInfo" "${URLUpdateInfo}" 0
  ${WriteRegDWORD2} $1 "$0" "NoModify" 1 0
  ${WriteRegDWORD2} $1 "$0" "NoRepair" 1 0

  ${GetSize} "$8" "/S=0K" $R2 $R3 $R4
  ${WriteRegDWORD2} $1 "$0" "EstimatedSize" $R2 0

  ${If} "$TmpVal" == "HKLM"
    SetShellVarContext all     ; Set SHCTX to all users (e.g. HKLM)
  ${Else}
    SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
  ${EndIf}
!macroend
!define SetUninstallKeys "!insertmacro SetUninstallKeys"

; Add app specific handler registry entries under Software\Classes if they
; don't exist (does not use SHCTX).
!macro FixClassKeys
  StrCpy $1 "SOFTWARE\Classes"

  ; File handler keys and name value pairs that may need to be created during
  ; install or upgrade.
  ReadRegStr $0 HKCR ".shtml" "Content Type"
  ${If} "$0" == ""
    StrCpy $0 "$1\.shtml"
    ${WriteRegStr2} $TmpVal "$1\.shtml" "" "shtmlfile" 0
    ${WriteRegStr2} $TmpVal "$1\.shtml" "Content Type" "text/html" 0
    ${WriteRegStr2} $TmpVal "$1\.shtml" "PerceivedType" "text" 0
  ${EndIf}

  ReadRegStr $0 HKCR ".xht" "Content Type"
  ${If} "$0" == ""
    ${WriteRegStr2} $TmpVal "$1\.xht" "" "xhtfile" 0
    ${WriteRegStr2} $TmpVal "$1\.xht" "Content Type" "application/xhtml+xml" 0
  ${EndIf}

  ReadRegStr $0 HKCR ".xhtml" "Content Type"
  ${If} "$0" == ""
    ${WriteRegStr2} $TmpVal "$1\.xhtml" "" "xhtmlfile" 0
    ${WriteRegStr2} $TmpVal "$1\.xhtml" "Content Type" "application/xhtml+xml" 0
  ${EndIf}
!macroend
!define FixClassKeys "!insertmacro FixClassKeys"

; Updates protocol handlers if their registry open command value is for this
; install location (uses SHCTX).
!macro UpdateProtocolHandlers
  ; Store the command to open the app with an url in a register for easy access.
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  StrCpy $2 "$\"$8$\" -requestPending -osint -url $\"%1$\""
  StrCpy $3 "$\"%1$\",,0,0,,,,"

  ; Only set the file and protocol handlers if the existing one under HKCR is
  ; for this install location.

  ${IsHandlerForInstallDir} "FirefoxHTML" $R9
  ${If} "$R9" == "true"
    ; An empty string is used for the 5th param because FirefoxHTML is not a
    ; protocol handler.
    ${AddDDEHandlerValues} "FirefoxHTML" "$2" "$8,1" "${AppRegName} Document" "" \
                           "${DDEApplication}" "$3" "WWW_OpenURL"
  ${EndIf}

  ${IsHandlerForInstallDir} "FirefoxURL" $R9
  ${If} "$R9" == "true"
    ${AddDDEHandlerValues} "FirefoxURL" "$2" "$8,1" "${AppRegName} URL" "true" \
                           "${DDEApplication}" "$3" "WWW_OpenURL"
  ${EndIf}

  ${IsHandlerForInstallDir} "ftp" $R9
  ${If} "$R9" == "true"
    ${AddDDEHandlerValues} "ftp" "$2" "$8,1" "" "" \
                           "${DDEApplication}" "$3" "WWW_OpenURL"
  ${EndIf}

  ${IsHandlerForInstallDir} "http" $R9
  ${If} "$R9" == "true"
    ${AddDDEHandlerValues} "http" "$2" "$8,1" "" "" \
                           "${DDEApplication}" "$3" "WWW_OpenURL"
  ${EndIf}

  ${IsHandlerForInstallDir} "https" $R9
  ${If} "$R9" == "true"
    ${AddDDEHandlerValues} "https" "$2" "$8,1" "" "" \
                           "${DDEApplication}" "$3" "WWW_OpenURL"
  ${EndIf}
!macroend
!define UpdateProtocolHandlers "!insertmacro UpdateProtocolHandlers"

; Removes various registry entries for reasons noted below (does not use SHCTX).
!macro RemoveDeprecatedKeys
  StrCpy $0 "SOFTWARE\Classes"
  ; Remove support for launching gopher urls from the shell during install or
  ; update if the DefaultIcon is from firefox.exe.
  ${RegCleanAppHandler} "gopher"

  ; Remove support for launching chrome urls from the shell during install or
  ; update if the DefaultIcon is from firefox.exe (Bug 301073).
  ${RegCleanAppHandler} "chrome"

  ; Remove protocol handler registry keys added by the MS shim
  DeleteRegKey HKLM "Software\Classes\Firefox.URL"
  DeleteRegKey HKCU "Software\Classes\Firefox.URL"

  ; Remove the app compatibility registry key
  StrCpy $0 "Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers"
  DeleteRegValue HKLM "$0" "$INSTDIR\${FileMainEXE}"
  DeleteRegValue HKCU "$0" "$INSTDIR\${FileMainEXE}"

  ; Delete gopher from Capabilities\URLAssociations if it is present.
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  StrCpy $0 "Software\Clients\StartMenuInternet\$R9"
  ClearErrors
  ReadRegStr $2 HKLM "$0\Capabilities\URLAssociations" "gopher"
  ${Unless} ${Errors}
    DeleteRegValue HKLM "$0\Capabilities\URLAssociations" "gopher"
  ${EndUnless}

  ; Delete gopher from the user's UrlAssociations if it points to FirefoxURL.
  StrCpy $0 "Software\Microsoft\Windows\Shell\Associations\UrlAssociations\gopher"
  ReadRegStr $2 HKCU "$0\UserChoice" "Progid"
  ${If} "$2" == "FirefoxURL"
    DeleteRegKey HKCU "$0"
  ${EndIf}
!macroend
!define RemoveDeprecatedKeys "!insertmacro RemoveDeprecatedKeys"

; Creates the shortcuts log ini file with the appropriate entries if it doesn't
; already exist.
!macro CreateShortcutsLog
  ${GetShortcutsLogPath} $0
  ${Unless} ${FileExists} "$0"
    ; Default to ${BrandFullName} for the Start Menu Folder
    StrCpy $TmpVal "${BrandFullName}"
    ; Prior to Firefox 3.1 the Start Menu directory was written to the registry and
    ; this value can be used to set the Start Menu directory.
    ClearErrors
    ReadRegStr $0 SHCTX "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Main" "Start Menu Folder"
    ${If} ${Errors}
      ${FindSMProgramsDir} $0
      ${If} "$0" != ""
        StrCpy $TmpVal "$0"
      ${EndIf}
    ${Else}
      StrCpy $TmpVal "$0"
    ${EndUnless}

    ${LogSMProgramsDirRelPath} "$TmpVal"
    ${LogSMProgramsShortcut} "${BrandFullName}.lnk"
    ${LogSMProgramsShortcut} "${BrandFullName} ($(SAFE_MODE)).lnk"
    ${LogQuickLaunchShortcut} "${BrandFullName}.lnk"
    ${LogDesktopShortcut} "${BrandFullName}.lnk"
  ${EndUnless}
!macroend
!define CreateShortcutsLog "!insertmacro CreateShortcutsLog"

; The files to check if they are in use during (un)install so the restart is
; required message is displayed. All files must be located in the $INSTDIR
; directory.
!macro PushFilesToCheck
  ; The first string to be pushed onto the stack MUST be "end" to indicate
  ; that there are no more files to check in $INSTDIR and the last string
  ; should be ${FileMainEXE} so if it is in use the CheckForFilesInUse macro
  ; returns after the first check.
  Push "end"
  Push "AccessibleMarshal.dll"
  Push "freebl3.dll"
  Push "nssckbi.dll"
  Push "nspr4.dll"
  Push "nssdbm3.dll"
  Push "mozsqlite3.dll"
  Push "xpcom.dll"
  Push "crashreporter.exe"
  Push "updater.exe"
  Push "${FileMainEXE}"
!macroend
!define PushFilesToCheck "!insertmacro PushFilesToCheck"


; Sets this installation as the default browser by setting the registry keys
; under HKEY_CURRENT_USER via registry calls and using the AppAssocReg NSIS
; plugin for Vista and above. This is a function instead of a macro so it is
; easily called from an elevated instance of the binary. Since this can be
; called by an elevated instance logging is not performed in this function.
Function SetAsDefaultAppUserHKCU
  ; Only set as the user's StartMenuInternet browser if the StartMenuInternet
  ; registry keys are for this install.
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  ClearErrors
  ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${Unless} ${Errors}
    ${GetPathFromString} "$0" $0
    ${GetParent} "$0" $0
    ${If} ${FileExists} "$0"
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR"
        WriteRegStr HKCU "Software\Clients\StartMenuInternet" "" "$R9"
      ${EndIf}
    ${EndIf}
  ${EndUnless}

  SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
  ${SetHandlers}

  ${If} ${AtLeastWinVista}
    ; Only register as the handler on Vista and above if the app registry name
    ; exists under the RegisteredApplications registry key. The protocol and
    ; file handlers set previously at the user level will associate this install
    ; as the default browser.
    ClearErrors
    ReadRegStr $0 HKLM "Software\RegisteredApplications" "${AppRegName}"
    ${Unless} ${Errors}
      AppAssocReg::SetAppAsDefaultAll "${AppRegName}"
    ${EndUnless}
  ${EndIf}
  ${RemoveDeprecatedKeys}
FunctionEnd

; The !ifdef NO_LOG prevents warnings when compiling the installer.nsi due to
; this function only being used by the uninstaller.nsi.
!ifdef NO_LOG

Function SetAsDefaultAppUser
  ; It is only possible to set this installation of the application as the
  ; StartMenuInternet handler if it was added to the HKLM StartMenuInternet
  ; registry keys.
  ; http://support.microsoft.com/kb/297878

  ; Check if this install location registered as the StartMenuInternet client
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  ClearErrors
  ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${Unless} ${Errors}
    ${GetPathFromString} "$0" $0
    ${GetParent} "$0" $0
    ${If} ${FileExists} "$0"
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR"
        ; Check if this is running in an elevated process
        ClearErrors
        ${GetParameters} $0
        ${GetOptions} "$0" "/UAC:" $0
        ${If} ${Errors} ; Not elevated
          Call SetAsDefaultAppUserHKCU
        ${Else} ; Elevated - execute the function in the unelevated process
          GetFunctionAddress $0 SetAsDefaultAppUserHKCU
          UAC::ExecCodeSegment $0
        ${EndIf}
        Return ; Nothing more needs to be done
      ${EndIf}
    ${EndIf}
  ${EndUnless}

  ; The code after ElevateUAC won't be executed on Vista and above when the
  ; user:
  ; a) is a member of the administrators group (e.g. elevation is required)
  ; b) is not a member of the administrators group and chooses to elevate
  ${ElevateUAC}

  ${SetStartMenuInternet}

  SetShellVarContext all  ; Set SHCTX to all users (e.g. HKLM)
  ${FixShellIconHandler}
  ${RemoveDeprecatedKeys}

  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $0
  ${If} ${Errors}
    Call SetAsDefaultAppUserHKCU
  ${Else}
    GetFunctionAddress $0 SetAsDefaultAppUserHKCU
    UAC::ExecCodeSegment $0
  ${EndIf}
FunctionEnd
!define SetAsDefaultAppUser "Call SetAsDefaultAppUser"

!endif
