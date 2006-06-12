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

  ${LogHeader} "Adding Additional Files"
  ; Only for Firefox and only if they don't already exist
  ; Check if QuickTime is installed and copy the contents of its plugins
  ; directory into the app's plugins directory. Previously only the
  ; nsIQTScriptablePlugin.xpt files was copied which is not enough to enable
  ; QuickTime as a plugin.
  ClearErrors
  ReadRegStr $R0 HKLM "Software\Apple Computer, Inc.\QuickTime" "InstallDir"
  ${Unless} ${Errors}
    Push $R0
    ${GetPathFromRegStr}
    Pop $R0
    ${Unless} ${Errors}
      GetFullPathName $R0 "$R0\Plugins"
      ${Unless} ${Errors}
        ${LogHeader} "Copying QuickTime Plugin Files"
        ${LogMsg} "Source Directory: $R0\Plugins"
        StrCpy $R1 "$INSTDIR\plugins"
        Call DoCopyFiles
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

  StrCpy $0 "Software\Mozilla\${BrandFullName}\${AppVersion} (${AB_CD})\Main"
  ${WriteRegStr2} $TmpVal "$0" "Install Directory" "$INSTDIR" 0
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$INSTDIR\${FileMainEXE}" 0
  ${WriteRegStr2} $TmpVal "$0" "Program Folder Path" "$SMPROGRAMS\$StartMenuDir" 0
  ${WriteRegDWORD2} $TmpVal "$0" "Create Quick Launch Shortcut" $AddQuickLaunchSC 0
  ${WriteRegDWORD2} $TmpVal "$0" "Create Desktop Shortcut" $AddDesktopSC 0
  ${WriteRegDWORD2} $TmpVal "$0" "Create Start Menu Shortcut" $AddStartMenuSC 0

  StrCpy $0 "Software\Mozilla\${BrandFullName}\${AppVersion} (${AB_CD})\Uninstall"
  ${WriteRegStr2} $TmpVal "$0" "Uninstall Log Folder" "$INSTDIR\uninstall" 0
  ${WriteRegStr2} $TmpVal "$0" "Description" "${BrandFullName} (${AppVersion})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullName}\${AppVersion} (${AB_CD})"
  ${WriteRegStr2} $TmpVal  "$0" "" "${AppVersion} (${AB_CD})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullName} ${AppVersion}\bin"
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$INSTDIR\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullName} ${AppVersion}\extensions"
  ${WriteRegStr2} $TmpVal "$0" "Components" "$INSTDIR\components" 0
  ${WriteRegStr2} $TmpVal "$0" "Plugins" "$INSTDIR\plugins" 0

  StrCpy $0 "Software\Mozilla\${BrandFullName} ${AppVersion}"
  ${WriteRegStr2} $TmpVal "$0" "GeckoVer" "${GREVersion}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullName}"
  ${WriteRegStr2} $TmpVal "$0" "" "${GREVersion}" 0
  ${WriteRegStr2} $TmpVal "$0" "CurrentVersion" "${AppVersion} (${AB_CD})" 0

  ; XXXrstrong - there are several values that will be overwritten by and
  ; overwrite other installs of the same application.
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  StrCpy $0 "Software\Clients\StartMenuInternet\$R9"
  ${WriteRegStr2} $TmpVal "$0" "" "${BrandFullName}" 0

  StrCpy $0 "Software\Clients\StartMenuInternet\$R9\DefaultIcon"
  StrCpy $1 "$\"$INSTDIR\${FileMainEXE}$\",0"
  ${WriteRegStr2} $TmpVal "$0" "" "$1" 0

  ; The Reinstall Command is defined at
  ; http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_adv/registeringapps.asp
  StrCpy $0 "Software\Clients\StartMenuInternet\$R9\InstallInfo"
  StrCpy $1 "$\"$INSTDIR\uninstall\uninstaller.exe$\" /ua $\"${AppVersion} (${AB_CD})$\" /hs browser"
  ${WriteRegStr2} $TmpVal "$0" "HideIconsCommand" "$1" 0
  ${WriteRegDWORD2} $TmpVal "$0" "IconsVisible" 1 0

  StrCpy $0 "Software\Clients\StartMenuInternet\$R9\InstallInfo"
  StrCpy $1 "$\"$INSTDIR\${FileMainEXE}$\" -silent -setDefaultBrowser"
  ${WriteRegStr2} $TmpVal "$0" "ReinstallCommand" "$1" 0
  StrCpy $1 "$\"$INSTDIR\uninstall\uninstaller.exe$\" /ua $\"${AppVersion} (${AB_CD})$\" /ss browser"
  ${WriteRegStr2} $TmpVal "$0" "ShowIconsCommand" "$1" 0

  StrCpy $0 "Software\Clients\StartMenuInternet\$R9\shell\open\command"
  ${WriteRegStr2} $TmpVal "$0" "" "$INSTDIR\${FileMainEXE}" 0

  ; These need special handling on uninstall since they may be overwritten by
  ; an install into a different location.
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\App Paths\${FileMainEXE}"
  ${WriteRegStr2} $TmpVal "$0" "" "$INSTDIR\${FileMainEXE}" 0
  ${WriteRegStr2} $TmpVal "$0" "Path" "$INSTDIR" 0

  StrCpy $0 "MIME\Database\Content Type\application/x-xpinstall;app=firefox"
  ${WriteRegStrHKCR} "HKCR" "$0" "Extension" ".xpi" 0

  StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\$R9"
  ${CreateRegKey} "$TmpVal" "$0" 0

  ; Write the uninstall registry keys
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullName} (${AppVersion})"
  StrCpy $1 "$\"$INSTDIR\uninstall\uninstaller.exe$\" $\"/ua ${AppVersion} (${AB_CD})$\""

  ${WriteRegStr2} $TmpVal "$0" "Comments" "${BrandFullName}" 0
  ${WriteRegStr2} $TmpVal "$0" "DisplayIcon" "$INSTDIR\${FileMainEXE},0" 0
  ${WriteRegStr2} $TmpVal "$0" "DisplayName" "${BrandFullName} (${AppVersion})" 0
  ${WriteRegStr2} $TmpVal "$0" "DisplayVersion" "${AppVersion} (${AB_CD})" 0
  ${WriteRegStr2} $TmpVal "$0" "InstallLocation" "$INSTDIR" 0
  ${WriteRegStr2} $TmpVal "$0" "Publisher" "Mozilla" 0
  ${WriteRegStr2} $TmpVal "$0" "UninstallString" "$1" 0
  ${WriteRegStr2} $TmpVal "$0" "URLInfoAbout" "${URLInfoAbout}" 0
  ${WriteRegStr2} $TmpVal "$0" "URLUpdateInfo" "${URLUpdateInfo}" 0
  ${WriteRegDWORD2} $TmpVal "$0" "NoModify" 1 0
  ${WriteRegDWORD2} $TmpVal "$0" "NoRepair" 1 0
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application

