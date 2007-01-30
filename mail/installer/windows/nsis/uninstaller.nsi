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
#  Scott MacGregor <mscott@mozilla.org>
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

Var TmpVal

; Other included files may depend upon these includes!
; The following includes are provided by NSIS.
!include FileFunc.nsh
!include LogicLib.nsh
!include TextFunc.nsh
!include WinMessages.nsh
!include WordFunc.nsh
!include MUI.nsh

!insertmacro GetParameters
!insertmacro un.LineFind
!insertmacro un.TrimNewLines

; The following includes are custom.
!include branding.nsi
!include defines.nsi
!include common.nsh
!include locales.nsi
!include version.nsh

; This is named BrandShortName helper because we use this for software update
; post update cleanup.
VIAddVersionKey "FileDescription" "${BrandShortName} Helper"

!insertmacro un.RegCleanMain
!insertmacro un.RegCleanUninstall
!insertmacro un.CloseApp
!insertmacro un.GetSecondInstallPath

Name "${BrandFullName}"
OutFile "uninst.exe"
InstallDirRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} (${AppVersion})" "InstallLocation"
InstallDir "$PROGRAMFILES\${BrandFullName}"
ShowUnInstDetails nevershow

################################################################################
# Modern User Interface - MUI

!define MUI_ABORTWARNING
!define MUI_ICON setup.ico
!define MUI_UNICON setup.ico
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_UNWELCOMEFINISHPAGE_BITMAP wizWatermark.bmp

; Use a right to left header image when the language is right to left
!ifdef ${AB_CD}_rtl
!define MUI_HEADERIMAGE_BITMAP_RTL wizHeaderRTL.bmp
!else
!define MUI_HEADERIMAGE_BITMAP wizHeader.bmp
!endif

/**
 * Uninstall Pages
 */
; Welcome Page
!insertmacro MUI_UNPAGE_WELCOME

; Uninstall Confirm Page
!insertmacro MUI_UNPAGE_CONFIRM

; Remove Files Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.preInstFiles
!insertmacro MUI_UNPAGE_INSTFILES

; Finish Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.preFinish
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME ""

; Setup the survey controls, functions, etc. except when the application has
; defined NO_UNINSTALL_SURVEY
!ifndef NO_UNINSTALL_SURVEY
!define MUI_FINISHPAGE_SHOWREADME_TEXT $(SURVEY_TEXT)
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION un.Survey
!endif

!insertmacro MUI_UNPAGE_FINISH

################################################################################
# Install Sections
; Empty section required for the installer to compile as an uninstaller
Section ""
SectionEnd

################################################################################
# Uninstall Sections

Section "Uninstall"
  SetDetailsPrint textonly
  DetailPrint $(STATUS_UNINSTALL_MAIN)
  SetDetailsPrint none
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

  StrCpy $0 "Software\Clients\Mail\${BrandFullNameInternal}\shell\open\command"
  ReadRegStr $1 HKLM "$0" ""
  Push $1
  ${GetPathFromRegStr}
  Pop $R0
  Push $R0
  ${GetParentDir}
  Pop $R1

  ; Only remove the Clients\Mail key if it refers to this install location.
  ; The Clients\Mail registry key is independent of the default app for the OS
  ; settings. The XPInstall base un-installer always removes this key if it is
  ; uninstalling the default app and it will always replace the keys when
  ; installing even if there is another install of Firefox that is set as the
  ; default app. Now the key is always updated on install but it is only
  ; removed if it refers to this install location.
  ${If} $INSTDIR == $R1
    ; XXXrstrong - if there is another installation of the same app ideally we
    ; would just modify these values. The GetSecondInstallPath macro could be
    ; made to provide enough information to do this.
    DeleteRegKey HKLM "Software\Clients\Mail\${BrandFullNameInternal}"
  ${EndIf}

  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\App Paths\${FileMainEXE}"
  ${If} $R9 == "false"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
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

  ; Remove files. If we don't have a log file skip
  ${If} ${FileExists} "$INSTDIR\uninstall\uninstall.log"
    ; Copy the uninstall log file to a temporary file
    GetTempFileName $TmpVal
    CopyFiles "$INSTDIR\uninstall\uninstall.log" "$TmpVal"

    ; Unregister DLL's
    ${un.LineFind} "$TmpVal" "/NUL" "1:-1" "un.UnRegDLLsCallback"

    ; Delete files
    ${un.LineFind} "$TmpVal" "/NUL" "1:-1" "un.RemoveFilesCallback"

    ; Remove directories we always control
    RmDir /r "$INSTDIR\uninstall"
    RmDir /r "$INSTDIR\updates"
    RmDir /r "$INSTDIR\defaults\shortcuts"

    ; Remove empty directories
    ${un.LineFind} "$TmpVal" "/NUL" "1:-1" "un.RemoveDirsCallback"

    ; Delete the temporary uninstall log file
    ${DeleteFile} "$TmpVal"

    ; Remove the installation directory if it is empty
    ${RemoveDir} "$INSTDIR"
  ${EndIf}

  ; Refresh desktop icons otherwise the start menu internet item won't be
  ; removed and other ugly things will happen like recreation of the registry
  ; key by the OS under some conditions.
  System::Call "shell32::SHChangeNotify(i, i, i, i) v (0x08000000, 0, 0, 0)"
SectionEnd

################################################################################
# Helper Functions

Function un.RemoveFilesCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 5
  ${If} $R1 == "File:"
    StrCpy $R9 "$R9" "" 6
    StrCpy $R0 "$R9" 1
    ; If the path is relative prepend the install directory
    ${If} $R0 == "\"
      StrCpy $R0 "$INSTDIR$R9"
    ${Else}
      StrCpy $R0 "$R9"
    ${EndIf}
    ${If} ${FileExists} "$R0"
      ${DeleteFile} "$R0"
    ${EndIf}
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

; Using locate will leave file handles open to some of the directories which
; will prevent the deletion of these directories. This parses the uninstall.log
; and uses the file entries to find / remove empty directories.
Function un.RemoveDirsCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 5
  ${If} $R1 == "File:"
    StrCpy $R9 "$R9" "" 6
    StrCpy $R1 "$R9" 1
    ${If} $R1 == "\"
      StrCpy $R2 "$INSTDIR"
      StrCpy $R1 "$INSTDIR$R9"
    ${Else}
      StrCpy $R2 ""
      StrCpy $R1 "$R9"
    ${EndIf}
    loop:
      Push $R1
      ${GetParentDir}
      Pop $R0
      GetFullPathName $R1 "$R0"
      ; We only try to remove empty directories but the Desktop, StartMenu, and
      ; QuickLaunch directories can be empty so guard against removing them.
      ${If} "$R2" != "$INSTDIR"
        SetShellVarContext all
        ${If} $R1 == "$DESKTOP"
        ${OrIf} $R1 == "$STARTMENU"
          GoTo end
        ${EndIf}
        SetShellVarContext current
        ${If} $R1 == "$QUICKLAUNCH"
        ${OrIf} $R1 == "$DESKTOP"
        ${OrIf} $R1 == "$STARTMENU"
          GoTo end
        ${EndIf}
      ${ElseIf} "$R1" == "$INSTDIR"
        GoTo end
      ${EndIf}
      ${If} ${FileExists} "$R1"
        RmDir "$R1"
      ${EndIf}
      ${If} ${Errors}
      ${OrIf} "$R2" != "$INSTDIR"
        GoTo end
      ${EndIf}
      GoTo loop
  ${EndIf}

  end:
    ClearErrors
    Push 0
FunctionEnd

Function un.UnRegDLLsCallback
  ${un.TrimNewLines} "$R9" "$R9"
  StrCpy $R1 "$R9" 7
  ${If} $R1 == "DLLReg:"
    StrCpy $R9 "$R9" "" 8
    StrCpy $R1 "$R9" 1
    ${If} $R1 == "\"
      StrCpy $R1 "$INSTDIR$R9"
    ${Else}
      StrCpy $R1 "$R9"
    ${EndIf}
    UnRegDLL $R1
  ${EndIf}
  ClearErrors
  Push 0
FunctionEnd

; Setup the survey controls, functions, etc. except when the application has
; defined NO_UNINSTALL_SURVEY
!ifndef NO_UNINSTALL_SURVEY
Function un.Survey
  ExecShell "open" "${SurveyURL}"
FunctionEnd
!endif

################################################################################
# Language

!insertmacro MOZ_MUI_LANGUAGE 'baseLocale'
!verbose push
!verbose 3
!include "overrideLocale.nsh"
!include "customLocale.nsh"
!verbose pop

; Set this after the locale files to override it if it is in the locale. Using
; " " for BrandingText will hide the "Nullsoft Install System..." branding.
BrandingText " "

################################################################################
# Page pre and leave functions

; Checks if the app being uninstalled is running.
Function un.preInstFiles
  ; Try to delete the app executable and if we can't delete it try to close the
  ; app. This allows running an instance that is located in another directory.
  ClearErrors
  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${EndIf}
  ${If} ${Errors}
    ClearErrors
    ${un.CloseApp} "true" $(WARN_APP_RUNNING_UNINSTALL)
    ; Delete the app exe to prevent launching the app while we are uninstalling.
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
    ClearErrors
  ${EndIf}
FunctionEnd

; When we add an optional action to the finish page the cancel button is
; enabled. This disables it and leaves the finish button as the only choice.
Function un.preFinish
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"

  ; Setup the survey controls, functions, etc. except when the application has
  ; defined NO_UNINSTALL_SURVEY
  !ifdef NO_UNINSTALL_SURVEY
    !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "NumFields" "3"
  !endif
FunctionEnd

################################################################################
# Initialization Functions

Function .onInit
  GetFullPathName $INSTDIR "$EXEDIR\.."
  ${Unless} ${FileExists} "$INSTDIR\${FileMainEXE}"
    Abort
  ${EndUnless}
  ${GetParameters} $R0

  StrCpy $R1 "Software\Clients\Mail\${BrandFullNameInternal}\InstallInfo"
  SetShellVarContext all  ; Set $DESKTOP to All Users

  ; Hide icons - initiated from Set Program Access and Defaults
  ${If} $R0 == '/ua "${AppVersion} (${AB_CD})" /hs mail'
    WriteRegDWORD HKLM $R1 "IconsVisible" 0
    ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
    ${EndUnless}

    ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      ShellLink::GetShortCutArgs "$DESKTOP\${BrandFullName}.lnk"
      Pop $0
      ${If} $0 == ""
        ShellLink::GetShortCutTarget "$DESKTOP\${BrandFullName}.lnk"
        Pop $0
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
        ${If} $0 == "$INSTDIR\${FileMainEXE}"
          Delete "$QUICKLAUNCH\${BrandFullName}.lnk"
        ${EndIf}
      ${EndIf}
    ${EndIf}
    Abort
  ${EndIf}

  ; Show icons - initiated from Set Program Access and Defaults
  ${If} $R0 == '/ua "${AppVersion} (${AB_CD})" /ss mail'
    WriteRegDWORD HKLM $R1 "IconsVisible" 1
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
    Abort
  ${EndIf}

  ; If we made it this far then this installer is being used as an uninstaller.
  WriteUninstaller "$EXEDIR\uninstaller.exe"

  ${If} $R0 == "/S"
    StrCpy $TmpVal "$\"$EXEDIR\uninstaller.exe$\" /S"
  ${Else}
    StrCpy $TmpVal "$\"$EXEDIR\uninstaller.exe$\""
  ${EndIf}

  ; When the uninstaller is launched it copies itself to the temp directory so
  ; it won't be in use so it can delete itself.
  ExecWait $TmpVal
  ${DeleteFile} "$EXEDIR\uninstaller.exe"
  SetErrorLevel 0
  Quit
FunctionEnd

Function un.onInit
  GetFullPathName $INSTDIR "$INSTDIR\.."
  ${Unless} ${FileExists} "$INSTDIR\${FileMainEXE}"
    Abort
  ${EndUnless}
  StrCpy $LANGUAGE 0
FunctionEnd
