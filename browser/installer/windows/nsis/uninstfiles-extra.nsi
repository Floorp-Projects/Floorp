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

  ; Remove registry entries for non-existent apps and for apps that point to our
  ; install location in the Software\Mozilla key.
  SetShellVarContext current  ; Sets SHCTX to HKCU
  ${un.RegCleanMain} "Software\Mozilla"
  SetShellVarContext all  ; Sets SHCTX to HKLM
  ${un.RegCleanMain} "Software\Mozilla"

  ; Remove uninstall entries that point to our install location
  ${un.RegCleanUninstall}

  SetShellVarContext all  ; Set SHCTX to HKLM
  ${un.GetSecondInstallPath} "Software\Mozilla" $R9
  ${If} $R9 == "false"
    SetShellVarContext current  ; Set SHCTX to HKCU
    ${un.GetSecondInstallPath} "Software\Mozilla" $R9
  ${EndIf}

;  ${StrFilter} "${FileMainEXE}" "+" "" "" $R8
  StrCpy $0 "Software\Clients\StartMenuInternet\${FileMainEXE}\shell\open\command"
  ReadRegStr $1 HKLM "$0" ""
  Push $1
  ${GetPathFromRegStr}
  Pop $R0
  Push $R0
  ${GetParentDir}
  Pop $R1

  ; Only remove the StartMenuInternet key if it refers to this install location.
  ; The StartMenuInternet registry key is independent of the default browser
  ; settings. The XPInstall base un-installer always removes this key if it is
  ; uninstalling the default browser and it will always replace the keys when
  ; installing even if there is another install of Firefox that is set as the
  ; default browser. Now the key is always updated on install but it is only
  ; removed if it refers to this install location.
  ${If} $INSTDIR == $R1
    ; XXXrstrong - if there is another installation of the same app ideally we
    ; would just modify these values. The GetSecondInstallPath macro could be
    ; made to provide enough information to do this.
    DeleteRegKey HKLM "Software\Clients\StartMenuInternet\${FileMainEXE}"
  ${EndIf}

  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\App Paths\${FileMainEXE}"
  ${If} $R9 == "false"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
    StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\$R9"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
    StrCpy $0 "MIME\Database\Content Type\application/x-xpinstall;app=firefox"
    DeleteRegKey HKCR "$0"
  ${Else}
    ReadRegStr $1 HKLM "$0" ""
    Push $1
    ${GetPathFromRegStr}
    Pop $R0
    Push $R0
    ${GetParentDir}
    Pop $R1
    ${If} $INSTDIR == $R1
      WriteRegStr HKLM "$0" "" "$R9"
      Push $R9
      ${GetParentDir}
      Pop $R1
      WriteRegStr HKLM "$0" "Path" "$R1"
    ${EndIf}
  ${EndIf}

