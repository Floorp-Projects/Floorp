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
  SetShellVarContext all
  ${SetStartMenuInternet}

  ; Remove registry entries for non-existent apps and for apps that point to our
  ; install location in the Software\Mozilla key.
  SetShellVarContext current  ; Set SHCTX to HKCU
  ${RegCleanMain} "Software\Mozilla"
  SetShellVarContext all  ; Set SHCTX to HKLM
  ${RegCleanMain} "Software\Mozilla"

  ; Remove uninstall entries that point to our install location
  ${RegCleanUninstall}

  ; Add Software\Mozilla\ registry entries
  ${SetAppKeys}

  ${SetUninstallKeys}

  ${FixClassKeys}
!macroend
!define PostUpdate "!insertmacro PostUpdate"

!macro SetAsDefaultAppUser
  SetShellVarContext current
  ${SetHandlers}
!macroend
!define SetAsDefaultAppUser "!insertmacro SetAsDefaultAppUser"

!macro SetAsDefaultAppGlobal
  SetShellVarContext all
  ${SetHandlers}
  ${SetStartMenuInternet}
  WriteRegStr HKLM "Software\Clients\StartMenuInternet" "" "$R9"
  ${ShowShortcuts}
!macroend
!define SetAsDefaultAppGlobal "!insertmacro SetAsDefaultAppGlobal"

!macro HideShortcuts
  ${StrFilter} "${FileMainEXE}" "+" "" "" $0
  StrCpy $R1 "Software\Clients\StartMenuInternet\$0\InstallInfo"
  WriteRegDWORD HKLM $R1 "IconsVisible" 0
  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
  ${EndUnless}

  ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$DESKTOP\${BrandFullName}.lnk"
    Pop $0
    ${If} $0 == ""
      ShellLink::GetShortCutTarget "$DESKTOP\${BrandFullName}.lnk"
      Pop $0
      ; Needs to handle short paths
      ${If} $0 == "$INSTDIR\${FileMainEXE}"
        Delete "$DESKTOP\${BrandFullName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$QUICKLAUNCH\${BrandFullName}.lnk"
    Pop $0
    ${If} $0 == ""
      ShellLink::GetShortCutTarget "$QUICKLAUNCH\${BrandFullName}.lnk"
      Pop $0
      ; Needs to handle short paths
      ${If} $0 == "$INSTDIR\${FileMainEXE}"
        Delete "$QUICKLAUNCH\${BrandFullName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define HideShortcuts "!insertmacro HideShortcuts"

!macro ShowShortcuts
  ${StrFilter} "${FileMainEXE}" "+" "" "" $0
  StrCpy $R1 "Software\Clients\StartMenuInternet\$0\InstallInfo"
  WriteRegDWORD HKLM $R1 "IconsVisible" 1
  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR"
    ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
      ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
        CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
        ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR"
      ${EndUnless}
    ${EndUnless}
  ${EndUnless}
  ${Unless} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ShellLink::SetShortCutWorkingDirectory "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR"
  ${EndUnless}
!macroend
!define ShowShortcuts "!insertmacro ShowShortcuts"

!macro SetHandlers
  GetFullPathName $8 "$INSTDIR\${FileMainEXE}"

  StrCpy $0 "SOFTWARE\Classes"
  StrCpy $2 "$\"$8$\" -url $\"%1$\" -requestPending"

  ; Associate the file handlers with FirefoxHTML
  WriteRegStr SHCTX "$0\.htm"   "" "FirefoxHTML"
  WriteRegStr SHCTX "$0\.html"  "" "FirefoxHTML"
  WriteRegStr SHCTX "$0\.shtml" "" "FirefoxHTML"
  WriteRegStr SHCTX "$0\.xht"   "" "FirefoxHTML"
  WriteRegStr SHCTX "$0\.xhtml" "" "FirefoxHTML"

  ; An empty string is used for the 5th param because FirefoxHTML is not a
  ; protocol handler
  ${AddHandlerValues} "$0\FirefoxHTML" "$2" "$8,1" "${AppRegName} Document" "" "true"

  ${AddHandlerValues} "$0\FirefoxURL" "$2" "$8,0" "${AppRegName} URL" "true" "true"
  ${AddHandlerValues} "$0\gopher" "$2" "$8,0" "URL:Gopher Protocol" "true" "true"

  ; An empty string is used for the 4th & 5th params because the following
  ; protocol handlers already have a display name and additional keys required
  ; for a protocol handler.
  ${AddHandlerValues} "$0\ftp" "$2" "$8,0" "" "" "true"
  ${AddHandlerValues} "$0\http" "$2" "$8,0" "" "" "true"
  ${AddHandlerValues} "$0\https" "$2" "$8,0" "" "" "true"
!macroend
!define SetHandlers "!insertmacro SetHandlers"

; XXXrstrong - there are several values that will be overwritten by and
; overwrite other installs of the same application.
!macro SetStartMenuInternet
  GetFullPathName $8 "$INSTDIR\${FileMainEXE}"
  GetFullPathName $7 "$INSTDIR\uninstall\helper.exe"

  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9

  StrCpy $0 "Software\Clients\StartMenuInternet\$R9"
  ; Remove existing keys so we only have our settings
  DeleteRegKey HKLM "$0"
  WriteRegStr HKLM "$0" "" "${BrandFullName}"

  WriteRegStr HKLM "$0\DefaultIcon" "" "$8,0"

  ; The Reinstall Command is defined at
  ; http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_adv/registeringapps.asp
  WriteRegStr HKLM "$0\InstallInfo" "HideIconsCommand" "$\"$7$\" /HideShortcuts"
  WriteRegStr HKLM "$0\InstallInfo" "ShowIconsCommand" "$\"$7$\" /ShowShortcuts"
  WriteRegStr HKLM "$0\InstallInfo" "ReinstallCommand" "$\"$7$\" /SetAsDefaultAppGlobal"

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
  WriteRegStr HKLM "$0\Capabilities\URLAssociations" "gopher" "FirefoxURL"
  WriteRegStr HKLM "$0\Capabilities\URLAssociations" "http"   "FirefoxURL"
  WriteRegStr HKLM "$0\Capabilities\URLAssociations" "https"  "FirefoxURL"

  ; Vista Registered Application
  WriteRegStr HKLM "Software\RegisteredApplications" "${AppRegName}" "$0\Capabilities"
!macroend
!define SetStartMenuInternet "!insertmacro SetStartMenuInternet"

!macro SetAppKeys
  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Main"
  ${WriteRegStr2} $TmpVal "$0" "Install Directory" "$INSTDIR" 0
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$INSTDIR\${FileMainEXE}" 0
  ${WriteRegStr2} $TmpVal "$0" "Program Folder Path" "$SMPROGRAMS\$StartMenuDir" 0

  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
  ${EndUnless}

  ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$DESKTOP\${BrandFullName}.lnk"
    Pop $1
    ${If} $1 == ""
      ShellLink::GetShortCutTarget "$DESKTOP\${BrandFullName}.lnk"
      Pop $1
      ; Needs to handle short paths
      ${If} $1 == "$INSTDIR\${FileMainEXE}"
        ${WriteRegDWORD2} $TmpVal "$0" "Create Desktop Shortcut" 1 0
      ${Else}
        ${WriteRegDWORD2} $TmpVal "$0" "Create Desktop Shortcut" 0 0
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ; XXXrstrong - need a cleaner way to prevent unsetting SHCTX from HKLM when
  ; trying to find the desktop shortcut.
  ${If} $TmpVal == "HKCU"
    SetShellVarContext current
  ${Else}
    SetShellVarContext all
  ${EndIf}

  ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$QUICKLAUNCH\${BrandFullName}.lnk"
    Pop $1
    ${If} $1 == ""
      ShellLink::GetShortCutTarget "$QUICKLAUNCH\${BrandFullName}.lnk"
      Pop $1
      ; Needs to handle short paths
      ${If} $1 == "$INSTDIR\${FileMainEXE}"
        ${WriteRegDWORD2} $TmpVal "$0" "Create Quick Launch Shortcut" 1 0
      ${Else}
        ${WriteRegDWORD2} $TmpVal "$0" "Create Quick Launch Shortcut" 0 0
      ${EndIf}
    ${EndIf}
  ${EndIf}
  ; XXXrstrong - "Create Start Menu Shortcut" and "Start Menu Folder" are only
  ; set in the installer and should also be set here for software update.

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})\Uninstall"
  ${WriteRegStr2} $TmpVal "$0" "Uninstall Log Folder" "$INSTDIR\uninstall" 0
  ${WriteRegStr2} $TmpVal "$0" "Description" "${BrandFullNameInternal} (${AppVersion})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion} (${AB_CD})"
  ${WriteRegStr2} $TmpVal  "$0" "" "${AppVersion} (${AB_CD})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}\bin"
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$INSTDIR\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}\extensions"
  ${WriteRegStr2} $TmpVal "$0" "Components" "$INSTDIR\components" 0
  ${WriteRegStr2} $TmpVal "$0" "Plugins" "$INSTDIR\plugins" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}"
  ${WriteRegStr2} $TmpVal "$0" "GeckoVer" "${GREVersion}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}"
  ${WriteRegStr2} $TmpVal "$0" "" "${GREVersion}" 0
  ${WriteRegStr2} $TmpVal "$0" "CurrentVersion" "${AppVersion} (${AB_CD})" 0
!macroend
!define SetAppKeys "!insertmacro SetAppKeys"

!macro SetUninstallKeys
  ; Write the uninstall registry keys
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})"
  GetFullPathName $8 "$INSTDIR\${FileMainEXE}"
  GetFullPathName $7 "$INSTDIR\uninstall\helper.exe"

  ${WriteRegStr2} $TmpVal "$0" "Comments" "${BrandFullNameInternal}" 0
  ${WriteRegStr2} $TmpVal "$0" "DisplayIcon" "$8,0" 0
  ${WriteRegStr2} $TmpVal "$0" "DisplayName" "${BrandFullNameInternal} (${AppVersion})" 0
  ${WriteRegStr2} $TmpVal "$0" "DisplayVersion" "${AppVersion} (${AB_CD})" 0
  ${WriteRegStr2} $TmpVal "$0" "InstallLocation" "$INSTDIR" 0
  ${WriteRegStr2} $TmpVal "$0" "Publisher" "Mozilla" 0
  ${WriteRegStr2} $TmpVal "$0" "UninstallString" "$7" 0
  ${WriteRegStr2} $TmpVal "$0" "URLInfoAbout" "${URLInfoAbout}" 0
  ${WriteRegStr2} $TmpVal "$0" "URLUpdateInfo" "${URLUpdateInfo}" 0
  ${WriteRegDWORD2} $TmpVal "$0" "NoModify" 1 0
  ${WriteRegDWORD2} $TmpVal "$0" "NoRepair" 1 0
!macroend
!define SetUninstallKeys "!insertmacro SetUninstallKeys"

!macro FixClassKeys
  StrCpy $0 "SOFTWARE\Classes"

  ; File handler keys and name value pairs that may need to be created during
  ; install or upgrade.
  ReadRegStr $2 SHCTX "$0\.shtml" "Content Type"
  ${If} $2 == ""
    StrCpy $2 "$0\.shtml"
    ${WriteRegStr2} $TmpVal "$0\.shtml" "" "shtmlfile" 0
    ${WriteRegStr2} $TmpVal "$0\.shtml" "Content Type" "text/html" 0
    ${WriteRegStr2} $TmpVal "$0\.shtml" "PerceivedType" "text" 0
  ${EndIf}

  ReadRegStr $2 SHCTX "$0\.xht" "Content Type"
  ${If} $2 == ""
    ${WriteRegStr2} $TmpVal "$0\.xht" "" "xhtfile" 0
    ${WriteRegStr2} $TmpVal "$0\.xht" "Content Type" "application/xhtml+xml" 0
  ${EndIf}

  ReadRegStr $2 SHCTX "$0\.xhtml" "Content Type"
  ${If} $2 == ""
    ${WriteRegStr2} $TmpVal "$0\.xhtml" "" "xhtmlfile" 0
    ${WriteRegStr2} $TmpVal "$0\.xhtml" "Content Type" "application/xhtml+xml" 0
  ${EndIf}

  ; Protocol handler keys and name value pairs that may need to be updated during
  ; install or upgrade.

  ; Bug 301073 Comment #9 makes it so Firefox no longer supports launching
  ; chrome urls from the shell so remove it during install or update if the
  ; DefaultIcon is from firefox.exe.
  ReadRegStr $2 SHCTX "$0\chrome\DefaultIcon" ""

  ClearErrors
  ${WordFind} "$2" "${FileMainEXE}" "E+1{" $R1

  ${Unless} ${Errors}
    DeleteRegKey SHCTX "$0\chrome"
  ${EndUnless}

  ; Store the command to open the app with an url in a register for easy access.
  GetFullPathName $8 "$INSTDIR\${FileMainEXE}"
  StrCpy $1 "$\"$8$\" -url $\"%1$\" -requestPending"

  ; Always set the file and protocol handlers since they may specify a
  ; different path and the path is used by Vista when setting associations.
  ${AddHandlerValues} "$0\FirefoxURL" "$1" "$8,0" "${AppRegName} URL" "true" "true"

  ; An empty string is used for the 5th param because FirefoxHTML is not a
  ; protocol handler
  ${AddHandlerValues} "$0\FirefoxHTML" "$1" "$8,1" "${AppRegName} Document" "" "true"

  ReadRegStr $2 SHCTX "$0\http\shell\open\command" ""
  ClearErrors
  ${WordFind} "$2" "${FileMainEXE}" "E+1{" $R1
  ${Unless} ${Errors}
    ${AddHandlerValues} "$0\http" "$1" "$8,0" "" "" "true"
  ${EndUnless}

  ReadRegStr $2 SHCTX "$0\https\shell\open\command" ""
  ClearErrors
  ${WordFind} "$2" "${FileMainEXE}" "E+1{" $R1
  ${Unless} ${Errors}
    ${AddHandlerValues} "$0\https" "$1" "$8,0" "" "" "true"
  ${EndUnless}

  ReadRegStr $2 SHCTX "$0\ftp\shell\open\command" ""
  ClearErrors
  ${WordFind} "$2" "${FileMainEXE}" "E+1{" $R1
  ${Unless} ${Errors}
    ${AddHandlerValues} "$0\ftp" "$1" "$8,0" "" "" "true"
  ${EndUnless}

  ; Only set the gopher key if it doesn't already exist with a default value
  ReadRegStr $2 SHCTX "$0\gopher" ""
  ${If} $2 == ""
    ${AddHandlerValues} "$0\gopher" "$1" "$8,0" "URL:Gopher Protocol" "true" "true"
  ${Else}
    ReadRegStr $2 SHCTX "$0\gopher\shell\open\command" ""
    ClearErrors
    ${WordFind} "$2" "${FileMainEXE}" "E+1{" $R1
    ${Unless} ${Errors}
      ${AddHandlerValues} "$0\gopher" "$1" "$8,0" "URL:Gopher Protocol" "true" "true"
    ${EndUnless}
  ${EndIf}
!macroend
!define FixClassKeys "!insertmacro FixClassKeys"
