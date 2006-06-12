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

; Provides the Set Access portion of the "Set Program Access and Defaults" that
; is available with Win2K Pro SP3 and WinXP SP1 (this does not specifically call
; out Windows Vista which has not been released at the time of this comment or
; any other future versions of Windows).

; This is fairly evil. We have a reg key of
; Software\Clients\StartMenuInternet\firefox.exe\InstallInfo
; yet we can have multiple installations of the program.
; this key provides info as to whether shortcuts are displayed and can hide
; and unhide these same shortcuts. This just seems wrong and prone to problems.
; For example, one instance installed in c:\firefox1 has set this key. Another
; instance is then installed in c:\firefox2. Under what specific circumstances
; do we set this key? What if the other instance is not the default browser...
; which shortcuts should be displayed on the desktop and quicklaunch as well as
; which ones affect the "set program access and defaults", etc.

; We probably need to have some verification of whether we are installing as the
; new default browser, etc.
; When installing with defaults we should always install into the previous location
; ReadRegStr $0 HKCR "http\shell\open\command" ""

Function un.SetAccess
  Call un.GetParameters
  Pop $R0

  StrCpy $R1 "Software\Clients\StartMenuInternet\${FileMainEXE}\InstallInfo"
  SetShellVarContext all  ; Set $DESKTOP to All Users

  ; Hide icons - initiated from Set Program Access and Defaults
  ${If} $R0 == '/ua "${AppVersion} (${AB_CD})" /hs browser'
    ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
      ShellLink::GetShortCutTarget "$QUICKLAUNCH\${BrandFullName}.lnk"
      Pop $0
      ${If} $0 == "$INSTDIR\${FileMainEXE}"
        Delete "$QUICKLAUNCH\${BrandFullName}.lnk"
        WriteRegDWORD HKLM $R1 "IconsVisible" 0
      ${EndIf}
    ${EndIf}

    ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
    ${EndUnless}

    ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      ShellLink::GetShortCutTarget "$DESKTOP\${BrandFullName}.lnk"
      Pop $0
      ${If} $0 == "$INSTDIR\${FileMainEXE}"
        Delete "$DESKTOP\${BrandFullName}.lnk"
        WriteRegDWORD HKLM $R1 "IconsVisible" 0
      ${EndIf}
    ${EndIf}
    Abort
  ${EndIf}

  ; Show icons - initiated from Set Program Access and Defaults
  ${If} $R0 == '/ua "${AppVersion} (${AB_CD})" /ss browser'
    WriteRegDWORD HKLM $R1 "IconsVisible" 1
    ${Unless} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
      CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
    ${EndUnless}
    ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
      ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
        SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
        ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
          CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}" "" "$INSTDIR\${FileMainEXE}" 0
        ${EndUnless}
      ${EndUnless}
    ${EndUnless}
    Abort
  ${EndIf}
FunctionEnd
