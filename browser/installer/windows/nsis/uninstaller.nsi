# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Required Plugins:
# AppAssocReg
#   http://nsis.sourceforge.net/Application_Association_Registration_plug-in
# BitsUtils
#   http://searchfox.org/mozilla-central/source/other-licenses/nsis/Contrib/BitsUtils
# CityHash
#   http://searchfox.org/mozilla-central/source/other-licenses/nsis/Contrib/CityHash
# HttpPostFile
#   http://searchfox.org/mozilla-central/source/other-licenses/nsis/Contrib/HttpPostFile
# ShellLink
#   http://nsis.sourceforge.net/ShellLink_plug-in
# UAC
#   http://nsis.sourceforge.net/UAC_plug-in

; Set verbosity to 3 (e.g. no script) to lessen the noise in the build logs
!verbose 3

; 7-Zip provides better compression than the lzma from NSIS so we add the files
; uncompressed and use 7-Zip to create a SFX archive of it
SetDatablockOptimize on
SetCompress off
CRCCheck on

RequestExecutionLevel user

Unicode true
ManifestSupportedOS all
ManifestDPIAware true

!addplugindir ./

; Attempt to elevate Standard Users in addition to users that
; are a member of the Administrators group.
!define NONADMIN_ELEVATE

; prevents compiling of the reg write logging.
!define NO_LOG

!define MaintUninstallKey \
 "Software\Microsoft\Windows\CurrentVersion\Uninstall\MozillaMaintenanceService"

Var TmpVal
Var MaintCertKey
Var ShouldOpenSurvey
Var ShouldSendPing
Var InstalledVersion
Var InstalledBuildID
Var ShouldPromptForRefresh
Var RefreshRequested
; AddTaskbarSC is defined here in order to silence warnings from inside
; MigrateTaskBarShortcut and is not intended to be used here.
; See Bug 1329869 for more.
Var AddTaskbarSC

; Other included files may depend upon these includes!
; The following includes are provided by NSIS.
!include FileFunc.nsh
!include InstallOptions.nsh
!include LogicLib.nsh
!include MUI.nsh
!include WinMessages.nsh
!include WinVer.nsh
!include WordFunc.nsh

!insertmacro GetSize
!insertmacro StrFilter
!insertmacro WordReplace

!insertmacro un.GetParent

; The following includes are custom.
!include branding.nsi
!include defines.nsi
!include common.nsh
!include locales.nsi

; This is named BrandShortName helper because we use this for software update
; post update cleanup.
VIAddVersionKey "FileDescription" "${BrandShortName} Helper"
VIAddVersionKey "OriginalFilename" "helper.exe"

!insertmacro AddDisabledDDEHandlerValues
!insertmacro CleanVirtualStore
!insertmacro ElevateUAC
!insertmacro GetLongPath
!insertmacro GetPathFromString
!insertmacro InitHashAppModelId
!insertmacro IsHandlerForInstallDir
!insertmacro IsPinnedToTaskBar
!insertmacro IsUserAdmin
!insertmacro LogDesktopShortcut
!insertmacro LogQuickLaunchShortcut
!insertmacro LogStartMenuShortcut
!insertmacro PinnedToStartMenuLnkCount
!insertmacro RegCleanAppHandler
!insertmacro RegCleanMain
!insertmacro RegCleanUninstall
!insertmacro SetAppLSPCategories
!insertmacro SetBrandNameVars
!insertmacro UpdateShortcutAppModelIDs
!insertmacro UnloadUAC
!insertmacro WriteRegDWORD2
!insertmacro WriteRegStr2

; This needs to be inserted after InitHashAppModelId because it uses
; $AppUserModelID and the compiler can't handle using variables lexically before
; they've been declared.
!insertmacro GetInstallerRegistryPref

!insertmacro un.ChangeMUIHeaderImage
!insertmacro un.ChangeMUISidebarImage
!insertmacro un.CheckForFilesInUse
!insertmacro un.CleanUpdateDirectories
!insertmacro un.CleanVirtualStore
!insertmacro un.DeleteShortcuts
!insertmacro un.GetCommonDirectory
!insertmacro un.GetLongPath
!insertmacro un.GetSecondInstallPath
!insertmacro un.InitHashAppModelId
!insertmacro un.ManualCloseAppPrompt
!insertmacro un.RegCleanAppHandler
!insertmacro un.RegCleanFileHandler
!insertmacro un.RegCleanMain
!insertmacro un.RegCleanPrefs
!insertmacro un.RegCleanUninstall
!insertmacro un.RegCleanProtocolHandler
!insertmacro un.RemoveQuotesFromPath
!insertmacro un.RemovePrecompleteEntries
!insertmacro un.SetAppLSPCategories
!insertmacro un.SetBrandNameVars

!include shared.nsh

; Helper macros for ui callbacks. Insert these after shared.nsh
!insertmacro OnEndCommon
!insertmacro UninstallOnInitCommon

!insertmacro un.OnEndCommon
!insertmacro un.UninstallUnOnInitCommon

Name "${BrandFullName}"
OutFile "helper.exe"
!ifdef HAVE_64BIT_BUILD
  InstallDir "$PROGRAMFILES64\${BrandFullName}\"
!else
  InstallDir "$PROGRAMFILES32\${BrandFullName}\"
!endif
ShowUnInstDetails nevershow

!define URLUninstallSurvey "https://qsurvey.mozilla.com/s3/FF-Desktop-Post-Uninstall?channel=${UpdateChannel}&version=${AppVersion}&osversion="

; Support for the profile refresh feature
!define URLProfileRefreshHelp "https://support.mozilla.org/kb/refresh-firefox-reset-add-ons-and-settings"

; Arguments to add to the command line when launching FileMainEXE for profile refresh
!define ArgsProfileRefresh "-reset-profile -migration -uninstaller-profile-refresh"

################################################################################
# Modern User Interface - MUI

!define MUI_ABORTWARNING
!define MUI_ICON setup.ico
!define MUI_UNICON setup.ico
!define MUI_WELCOMEPAGE_TITLE_3LINES
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_UNWELCOMEFINISHPAGE_BITMAP wizWatermark.bmp
; By default MUI_BGCOLOR is hardcoded to FFFFFF, which is only correct if the
; the Windows theme or high-contrast mode hasn't changed it, so we need to
; override that with GetSysColor(COLOR_WINDOW) (this string ends up getting
; passed to SetCtlColors, which uses this custom syntax to mean that).
!define MUI_BGCOLOR SYSCLR:WINDOW

; Use a right to left header image when the language is right to left
!ifdef ${AB_CD}_rtl
!define MUI_HEADERIMAGE_BITMAP_RTL wizHeaderRTL.bmp
!else
!define MUI_HEADERIMAGE_BITMAP wizHeader.bmp
!endif

!define MUI_CUSTOMFUNCTION_UNGUIINIT un.GUIInit

/**
 * Uninstall Pages
 */
; Welcome Page
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.preWelcome
!define MUI_PAGE_CUSTOMFUNCTION_SHOW un.showWelcome
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE un.leaveWelcome
!insertmacro MUI_UNPAGE_WELCOME

; Custom Uninstall Confirm Page
UninstPage custom un.preConfirm

; Remove Files Page
!insertmacro MUI_UNPAGE_INSTFILES

; Finish Page
!define MUI_FINISHPAGE_SHOWREADME
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_SHOWREADME_TEXT $(UN_SURVEY_CHECKBOX_LABEL)
!define MUI_FINISHPAGE_SHOWREADME_FUNCTION un.Survey
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.preFinish
!define MUI_PAGE_CUSTOMFUNCTION_SHOW un.showFinish
!insertmacro MUI_UNPAGE_FINISH

; Use the default dialog for IDD_VERIFY for a simple Banner
ChangeUI IDD_VERIFY "${NSISDIR}\Contrib\UIs\default.exe"

################################################################################
# Helper Functions

Function un.Survey
  ; We can't actually call ExecInExplorer here because it's going to have to
  ; make some marshalled COM calls and those are not allowed from within a
  ; synchronous message handler (where we currently are); we'll be thrown
  ; RPC_E_CANTCALLOUT_ININPUTSYNCCALL if we try. So all we can do is record
  ; that we need to make the call later, which we'll do from un.onGUIEnd.
  StrCpy $ShouldOpenSurvey "1"
FunctionEnd

; This function is used to uninstall the maintenance service if the
; application currently being uninstalled is the last application to use the
; maintenance service.
Function un.UninstallServiceIfNotUsed
  ; $0 will store if a subkey exists
  ; $1 will store the first subkey if it exists or an empty string if it doesn't
  ; Backup the old values
  Push $0
  Push $1

  ; The maintenance service always uses the 64-bit registry on x64 systems
  ${If} ${RunningX64}
  ${OrIf} ${IsNativeARM64}
    SetRegView 64
  ${EndIf}

  ; Figure out the number of subkeys
  StrCpy $0 0
  ${Do}
    EnumRegKey $1 HKLM "Software\Mozilla\MaintenanceService" $0
    ${If} "$1" == ""
      ${ExitDo}
    ${EndIf}
    IntOp $0 $0 + 1
  ${Loop}

  ; Restore back the registry view
  ${If} ${RunningX64}
  ${OrIf} ${IsNativeARM64}
    SetRegView lastused
  ${EndIf}

  ${If} $0 == 0
    ; Get the path of the maintenance service uninstaller.
    ; Look in both the 32-bit and 64-bit registry views.
    SetRegView 32
    ReadRegStr $1 HKLM ${MaintUninstallKey} "UninstallString"
    SetRegView lastused

    ${If} ${RunningX64}
    ${OrIf} ${IsNativeARM64}
      ${If} $1 == ""
        SetRegView 64
        ReadRegStr $1 HKLM ${MaintUninstallKey} "UninstallString"
        SetRegView lastused
      ${EndIf}
    ${EndIf}

    ; If the uninstall string does not exist, skip executing it
    ${If} $1 != ""
      ; $1 is already a quoted string pointing to the install path
      ; so we're already protected against paths with spaces
      nsExec::Exec "$1 /S"
    ${EndIf}
  ${EndIf}

  ; Restore the old value of $1 and $0
  Pop $1
  Pop $0
FunctionEnd

Function un.LaunchAppForRefresh
  Push $0
  Push $1
  ; Set the current working directory to the installation directory
  SetOutPath "$INSTDIR"
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $1
  ${If} ${Errors}
    ${ExecAndWaitForInputIdle} "$\"$INSTDIR\${FileMainEXE}$\" ${ArgsProfileRefresh}"
  ${Else}
    GetFunctionAddress $0 un.LaunchAppForRefreshFromElevatedProcess
    UAC::ExecCodeSegment $0
  ${EndIf}
  Pop $1
  Pop $0
FunctionEnd

Function un.LaunchAppForRefreshFromElevatedProcess
  ; Set the current working directory to the installation directory
  SetOutPath "$INSTDIR"
  ${ExecAndWaitForInputIdle} "$\"$INSTDIR\${FileMainEXE}$\" ${ArgsProfileRefresh}"
FunctionEnd

Function un.LaunchRefreshHelpPage
  Push $0
  Push $1
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $1
  ${If} ${Errors}
    Call un.OpenRefreshHelpURL
  ${Else}
    GetFunctionAddress $0 un.OpenRefreshHelpURL
    UAC::ExecCodeSegment $0
  ${EndIf}
  Pop $1
  Pop $0
FunctionEnd

Function un.OpenRefreshHelpURL
  ExecShell "open" "${URLProfileRefreshHelp}"
FunctionEnd

Function un.SendUninstallPing
  ; Notably, we only check the non-private AUMID here. There's no good reason
  ; to send the uninstall ping twice.
  ${If} $AppUserModelID == ""
    Return
  ${EndIf}

  Push $0   ; $0 = Find handle
  Push $1   ; $1 = Found ping file name
  Push $2   ; $2 = Directory containing the pings
  Push $3   ; $3 = Ping file name filespec
  Push $4   ; $4 = Offset of ID in file name
  Push $5   ; $5 = URL, POST result
  Push $6   ; $6 = Full path to the ping file

  Call un.GetCommonDirectory
  Pop $2

  ; The ping ID is in the file name, so that we can get it for the submission URL
  ; without having to parse the ping. Since we don't know the exact name, use FindFirst
  ; to locate the file.
  ; Format is uninstall_ping_$AppUserModelID_$PingUUID.json

  ; File name base
  StrCpy $3 "uninstall_ping_$AppUserModelID_"
  ; Get length of the fixed prefix, this is the offset of ping ID in the file name
  StrLen $4 $3
  ; Finish building filespec
  StrCpy $3 "$2\$3*.json"

  ClearErrors
  FindFirst $0 $1 $3
  ; Build the full path
  StrCpy $6 "$2\$1"

  ${IfNot} ${Errors}
    ; Copy the ping ID, starting after $AppUserModelID_, ending 5 from the end to remove .json
    StrCpy $5 $1 -5 $4

    ; Build the full submission URL from the ID
    ; https://docs.telemetry.mozilla.org/concepts/pipeline/http_edge_spec.html#special-handling-for-firefox-desktop-telemetry
    ; It's possible for the path components to disagree with the contents of the ping,
    ; but this should be rare, and shouldn't affect the collection.
    StrCpy $5 "${TELEMETRY_BASE_URL}/${TELEMETRY_UNINSTALL_PING_NAMESPACE}/$5/${TELEMETRY_UNINSTALL_PING_DOCTYPE}/${AppName}/$InstalledVersion/${UpdateChannel}/$InstalledBuildID?v=4"

    HttpPostFile::Post $6 "Content-Type: application/json$\r$\n" $5
    ; Pop the result. This could indicate an error if it's something other than
    ; "success", but we don't have any recovery path here anyway.
    Pop $5

    ${Do}
      Delete $6

      ; Continue to delete any other pings from this install. Only the first found will be sent:
      ; there should only be one ping, if there are more than one then something has gone wrong,
      ; it seems preferable to not try to send them all.
      ClearErrors
      FindNext $0 $1
      ; Build the full path
      StrCpy $6 "$2\$1"
    ${LoopUntil} ${Errors}

    FindClose $0
  ${Endif}

  Pop $6
  Pop $5
  Pop $4
  Pop $3
  Pop $2
  Pop $1
  Pop $0
FunctionEnd

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

  ; Some system cleanup is most easily handled when XPCOM functionality is
  ; available - e.g. removing notifications from Window's Action Center. We
  ; handle this in the `uninstall` background task.
  ;
  ; Return value is saved to an unused variable to prevent the the error flag
  ; from being set.
  Var /GLOBAL UnusedExecCatchReturn
  ExecWait '"$INSTDIR\${FileMainEXE}" --backgroundtask uninstall' $UnusedExecCatchReturn

  ; Delete the app exe to prevent launching the app while we are uninstalling.
  ClearErrors
  ${DeleteFile} "$INSTDIR\${FileMainEXE}"
  ${If} ${Errors}
    ; If the user closed the application it can take several seconds for it to
    ; shut down completely. If the application is being used by another user we
    ; can still delete the files when the system is restarted.
    Sleep 5000
    ${DeleteFile} "$INSTDIR\${FileMainEXE}"
    ClearErrors
  ${EndIf}

  SetShellVarContext current  ; Set SHCTX to HKCU
  ${un.RegCleanMain} "Software\Mozilla"
  ${un.RegCleanPrefs} "Software\Mozilla\${AppName}"
  ${un.RegCleanUninstall}
  ${un.DeleteShortcuts}

  ${If} "$AppUserModelID" != ""
    ; Unregister resources associated with Win7 taskbar jump lists.
    ${If} ${AtLeastWin7}
      ApplicationID::UninstallJumpLists "$AppUserModelID"
    ${EndIf}
    ; Remove the update sync manager's multi-instance lock file
    Call un.GetCommonDirectory
    Pop $0
    Delete /REBOOTOK "$0\UpdateLock-$AppUserModelID"
  ${EndIf}

  ${If} "$AppUserModelIDPrivate" != ""
    ${If} ${AtLeastWin7}
      ApplicationID::UninstallJumpLists "$AppUserModelIDPrivate"
    ${EndIf}
  ${EndIf}

  ; Remove the updates directory
  ${un.CleanUpdateDirectories} "Mozilla\Firefox" "Mozilla\updates"

  ; Remove any app model id's stored in the registry for this install path
  DeleteRegValue HKCU "Software\Mozilla\${AppName}\TaskBarIDs" "$INSTDIR"
  DeleteRegValue HKLM "Software\Mozilla\${AppName}\TaskBarIDs" "$INSTDIR"

  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    StrCpy $TmpVal "HKCU" ; used primarily for logging
  ${Else}
    SetShellVarContext all  ; Set SHCTX to HKLM
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    StrCpy $TmpVal "HKLM" ; used primarily for logging
    ${un.RegCleanMain} "Software\Mozilla"
    ${un.RegCleanUninstall}
    ${un.DeleteShortcuts}
    ${un.SetAppLSPCategories}
  ${EndIf}

  ${un.RegCleanAppHandler} "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanAppHandler} "FirefoxPDF-$AppUserModelID"
  ${un.RegCleanAppHandler} "FirefoxURL-$AppUserModelID"
  ${un.RegCleanProtocolHandler} "http"
  ${un.RegCleanProtocolHandler} "https"
  ${un.RegCleanProtocolHandler} "mailto"
  ${un.RegCleanFileHandler}  ".htm"   "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".html"  "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".shtml" "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".xht"   "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".xhtml" "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".oga"   "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".ogg"   "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".ogv"   "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".webm"  "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".svg"   "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".webp"  "FirefoxHTML-$AppUserModelID"
  ${un.RegCleanFileHandler}  ".avif"  "FirefoxHTML-$AppUserModelID"

  ${un.RegCleanFileHandler}  ".pdf"   "FirefoxPDF-$AppUserModelID"

  SetShellVarContext all  ; Set SHCTX to HKLM
  ${un.GetSecondInstallPath} "Software\Mozilla" $R9
  ${If} $R9 == "false"
    SetShellVarContext current  ; Set SHCTX to HKCU
    ${un.GetSecondInstallPath} "Software\Mozilla" $R9
  ${EndIf}

  DeleteRegKey HKLM "Software\Clients\StartMenuInternet\${AppRegName}-$AppUserModelID"
  DeleteRegValue HKLM "Software\RegisteredApplications" "${AppRegName}-$AppUserModelID"

  DeleteRegKey HKCU "Software\Clients\StartMenuInternet\${AppRegName}-$AppUserModelID"
  DeleteRegValue HKCU "Software\RegisteredApplications" "${AppRegName}-$AppUserModelID"

  ; Remove old protocol handler and StartMenuInternet keys without install path
  ; hashes, but only if they're for this installation.  We've never supported
  ; bare FirefoxPDF.
  ReadRegStr $0 HKLM "Software\Classes\FirefoxHTML\DefaultIcon" ""
  StrCpy $0 $0 -2
  ${If} $0 == "$INSTDIR\${FileMainEXE}"
    DeleteRegKey HKLM "Software\Classes\FirefoxHTML"
    DeleteRegKey HKLM "Software\Classes\FirefoxURL"
    ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
    DeleteRegKey HKLM "Software\Clients\StartMenuInternet\$R9"
    DeleteRegValue HKLM "Software\RegisteredApplications" "$R9"
    DeleteRegValue HKLM "Software\RegisteredApplications" "${AppRegName}"
  ${EndIf}
  ReadRegStr $0 HKCU "Software\Classes\FirefoxHTML\DefaultIcon" ""
  StrCpy $0 $0 -2
  ${If} $0 == "$INSTDIR\${FileMainEXE}"
    DeleteRegKey HKCU "Software\Classes\FirefoxHTML"
    DeleteRegKey HKCU "Software\Classes\FirefoxURL"
    ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
    DeleteRegKey HKCU "Software\Clients\StartMenuInternet\$R9"
    DeleteRegValue HKCU "Software\RegisteredApplications" "$R9"
    DeleteRegValue HKCU "Software\RegisteredApplications" "${AppRegName}"
  ${EndIf}

  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\App Paths\${FileMainEXE}"
  ${If} $R9 == "false"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
    StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\${FileMainEXE}"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
    StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\plugin-container.exe"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
    StrCpy $0 "Software\Classes\MIME\Database\Content Type\application/x-xpinstall;app=firefox"
    DeleteRegKey HKLM "$0"
    DeleteRegKey HKCU "$0"
  ${Else}
    ReadRegStr $R1 HKLM "$0" ""
    ${un.RemoveQuotesFromPath} "$R1" $R1
    ${un.GetParent} "$R1" $R1
    ${If} "$INSTDIR" == "$R1"
      WriteRegStr HKLM "$0" "" "$R9"
      ${un.GetParent} "$R9" $R1
      WriteRegStr HKLM "$0" "Path" "$R1"
    ${EndIf}
  ${EndIf}

  ; Remove our HKCR/Applications key, if it's for this installation.
  ReadRegStr $0 HKLM "Software\Classes\Applications\${FileMainEXE}\DefaultIcon" ""
  StrCpy $0 $0 -2
  ${If} $0 == "$INSTDIR\${FileMainEXE}"
    DeleteRegKey HKLM "Software\Classes\Applications\${FileMainEXE}"
  ${EndIf}
  ReadRegStr $0 HKCU "Software\Classes\Applications\${FileMainEXE}\DefaultIcon" ""
  StrCpy $0 $0 -2
  ${If} $0 == "$INSTDIR\${FileMainEXE}"
    DeleteRegKey HKCU "Software\Classes\Applications\${FileMainEXE}"
  ${EndIf}

  ; Remove directories and files we always control before parsing the uninstall
  ; log so empty directories can be removed.
  ${If} ${FileExists} "$INSTDIR\updates"
    RmDir /r /REBOOTOK "$INSTDIR\updates"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\updated"
    RmDir /r /REBOOTOK "$INSTDIR\updated"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\defaults\shortcuts"
    RmDir /r /REBOOTOK "$INSTDIR\defaults\shortcuts"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\distribution"
    RmDir /r /REBOOTOK "$INSTDIR\distribution"
  ${EndIf}

  ; Remove files that may be left behind by the application in the
  ; VirtualStore directory.
  ${un.CleanVirtualStore}

  ; Only unregister the dll if the registration points to this installation
  ReadRegStr $R1 HKCR "CLSID\{0D68D6D0-D93D-4D08-A30D-F00DD1F45B24}\InProcServer32" ""
  ${If} "$INSTDIR\AccessibleMarshal.dll" == "$R1"
    ${UnregisterDLL} "$INSTDIR\AccessibleMarshal.dll"
  ${EndIf}

  ; Only unregister the dll if the registration points to this installation
  ReadRegStr $R1 HKCR "CLSID\${AccessibleHandlerCLSID}\InprocHandler32" ""
  ${If} "$INSTDIR\AccessibleHandler.dll" == "$R1"
    ${UnregisterDLL} "$INSTDIR\AccessibleHandler.dll"
  ${EndIf}

  ; Remove the Windows Reporter Module entry
  DeleteRegValue HKLM "SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules" "$INSTDIR\mozwer.dll"

!ifdef MOZ_LAUNCHER_PROCESS
  DeleteRegValue HKCU ${MOZ_LAUNCHER_SUBKEY} "$INSTDIR\${FileMainEXE}|Launcher"
  DeleteRegValue HKCU ${MOZ_LAUNCHER_SUBKEY} "$INSTDIR\${FileMainEXE}|Browser"
  DeleteRegValue HKCU ${MOZ_LAUNCHER_SUBKEY} "$INSTDIR\${FileMainEXE}|Image"
  DeleteRegValue HKCU ${MOZ_LAUNCHER_SUBKEY} "$INSTDIR\${FileMainEXE}|Telemetry"
!endif

  ; Remove Toast Notification registration.
  ${If} ${AtLeastWin10}
    ; Find any GUID used for this installation.
    ClearErrors
    ReadRegStr $0 HKLM "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "CustomActivator"

    DeleteRegValue HKLM "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "CustomActivator"
    DeleteRegValue HKLM "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "DisplayName"
    DeleteRegValue HKLM "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "IconUri"
    DeleteRegKey HKLM "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID"
    ${If} "$0" != ""
      DeleteRegValue HKLM "Software\Classes\AppID\$0" "DllSurrogate"
      DeleteRegKey HKLM "Software\Classes\AppID\$0"
      DeleteRegValue HKLM "Software\Classes\CLSID\$0" "AppID"
      DeleteRegValue HKLM "Software\Classes\CLSID\$0\InProcServer32" ""
      DeleteRegKey HKLM "Software\Classes\CLSID\$0\InProcServer32"
      DeleteRegKey HKLM "Software\Classes\CLSID\$0"
    ${EndIf}

    ClearErrors
    ReadRegStr $0 HKCU "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "CustomActivator"

    DeleteRegValue HKCU "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "CustomActivator"
    DeleteRegValue HKCU "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "DisplayName"
    DeleteRegValue HKCU "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID" "IconUri"
    DeleteRegKey HKCU "Software\Classes\AppUserModelId\${ToastAumidPrefix}$AppUserModelID"
    ${If} "$0" != ""
      DeleteRegValue HKCU "Software\Classes\AppID\$0" "DllSurrogate"
      DeleteRegKey HKCU "Software\Classes\AppID\$0"
      DeleteRegValue HKCU "Software\Classes\CLSID\$0" "AppID"
      DeleteRegValue HKCU "Software\Classes\CLSID\$0\InProcServer32" ""
      DeleteRegKey HKCU "Software\Classes\CLSID\$0\InProcServer32"
      DeleteRegKey HKCU "Software\Classes\CLSID\$0"
    ${EndIf}
  ${EndIf}

  ; Uninstall the default browser agent scheduled task and all other scheduled
  ; tasks registered by Firefox.
  ; This also removes the registry entries that the WDBA creates.
  ; One of the scheduled tasks that this will remove is the Background Update
  ; Task. Ideally, this will eventually be changed so that it doesn't rely on
  ; the WDBA. See Bug 1710143.
  ExecWait '"$INSTDIR\default-browser-agent.exe" uninstall $AppUserModelID'
  ${RemoveDefaultBrowserAgentShortcut}

  ${un.RemovePrecompleteEntries} "false"

  ${If} ${FileExists} "$INSTDIR\defaults\pref\channel-prefs.js"
    Delete /REBOOTOK "$INSTDIR\defaults\pref\channel-prefs.js"
  ${EndIf}
  RmDir "$INSTDIR\defaults\pref"
  RmDir "$INSTDIR\defaults"
  ${If} ${FileExists} "$INSTDIR\uninstall"
    ; Remove the uninstall directory that we control
    RmDir /r /REBOOTOK "$INSTDIR\uninstall"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\install.log"
    Delete /REBOOTOK "$INSTDIR\install.log"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\update-settings.ini"
    Delete /REBOOTOK "$INSTDIR\update-settings.ini"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\installation_telemetry.json"
    Delete /REBOOTOK "$INSTDIR\installation_telemetry.json"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\postSigningData"
    Delete /REBOOTOK "$INSTDIR\postSigningData"
  ${EndIf}

  ; Explicitly remove empty webapprt dir in case it exists (bug 757978).
  RmDir "$INSTDIR\webapprt\components"
  RmDir "$INSTDIR\webapprt"

  ; Remove the installation directory if it is empty
  RmDir "$INSTDIR"

  ; If firefox.exe was successfully deleted yet we still need to restart to
  ; remove other files create a dummy firefox.exe.moz-delete to prevent the
  ; installer from allowing an install without restart when it is required
  ; to complete an uninstall.
  ${If} ${RebootFlag}
    ; Admin is required to delete files on reboot so only add the moz-delete if
    ; the user is an admin. After calling UAC::IsAdmin $0 will equal 1 if the
    ; user is an admin.
    UAC::IsAdmin
    ${If} "$0" == "1"
      ${Unless} ${FileExists} "$INSTDIR\${FileMainEXE}.moz-delete"
        FileOpen $0 "$INSTDIR\${FileMainEXE}.moz-delete" w
        FileWrite $0 "Will be deleted on restart"
        Delete /REBOOTOK "$INSTDIR\${FileMainEXE}.moz-delete"
        FileClose $0
      ${EndUnless}
    ${EndIf}
  ${EndIf}

  ; Refresh desktop icons otherwise the start menu internet item won't be
  ; removed and other ugly things will happen like recreation of the app's
  ; clients registry key by the OS under some conditions.
  ${RefreshShellIcons}

  ; Users who uninstall then reinstall expecting Firefox to use a clean profile
  ; may be surprised during first-run. This key is checked during startup of Firefox and
  ; subsequently deleted after checking. If the value is found during startup
  ; the browser will offer to Reset Firefox. We use the UpdateChannel to match
  ; uninstalls of Firefox-release with reinstalls of Firefox-release, for example.
  WriteRegStr HKCU "Software\Mozilla\Firefox" "Uninstalled-${UpdateChannel}" "True"

!ifdef MOZ_MAINTENANCE_SERVICE
  ; Get the path the allowed cert is at and remove it
  ; Keep this block of code last since it modfies the reg view
  ServicesHelper::PathToUniqueRegistryPath "$INSTDIR"
  Pop $MaintCertKey
  ${If} $MaintCertKey != ""
    ; Always use the 64bit registry for certs on 64bit systems.
    ${If} ${RunningX64}
    ${OrIf} ${IsNativeARM64}
      SetRegView 64
    ${EndIf}
    DeleteRegKey HKLM "$MaintCertKey"
    ${If} ${RunningX64}
    ${OrIf} ${IsNativeARM64}
      SetRegView lastused
    ${EndIf}
  ${EndIf}
  Call un.UninstallServiceIfNotUsed
!endif

!ifdef MOZ_BITS_DOWNLOAD
  BitsUtils::CancelBitsJobsByName "MozillaUpdate $AppUserModelID"
  Pop $0
!endif

  ${un.IsFirewallSvcRunning}
  Pop $0
  ${If} "$0" == "true"
    liteFirewallW::RemoveRule "$INSTDIR\${FileMainEXE}" "${BrandShortName} ($INSTDIR)"
  ${EndIf}
SectionEnd

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
# Page pre, show, and leave functions

Function un.preWelcome
  ${If} ${FileExists} "$INSTDIR\distribution\modern-wizard.bmp"
    Delete "$PLUGINSDIR\modern-wizard.bmp"
    CopyFiles /SILENT "$INSTDIR\distribution\modern-wizard.bmp" "$PLUGINSDIR\modern-wizard.bmp"
  ${EndIf}
!ifdef MOZ_BITS_DOWNLOAD
  BitsUtils::StartBitsServiceBackground
!endif

  ; We don't want the header bitmap showing on the welcome page.
  GetDlgItem $0 $HWNDPARENT 1046
  ShowWindow $0 ${SW_HIDE}

  ${If} $ShouldPromptForRefresh == "1"
    ; Note: INI strings added here (rather than overwriting an existing value)
    ; should be removed in un.leaveWelcome, since ioSpecial.ini is reused
    ; for the Finish page.

    ; Replace title and body text
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 2" Text "$(UN_REFRESH_PAGE_TITLE)"
    ; Convert to translate newlines, this includes $PLUGINSDIR internally.
    !insertmacro INSTALLOPTIONS_WRITE_UNCONVERT "ioSpecial.ini" "Field 3" Text "$(UN_REFRESH_PAGE_EXPLANATION)"

    ; Make room for the link and button
    StrCpy $0 "148"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 3" Bottom  $0

    ; Show the help link
    IntOp $1 $0 + 14
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Type    "link"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Text    "$(UN_REFRESH_LEARN_MORE)"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Left    "120"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Top     $0
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Right   "315"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Bottom  $1
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Flags   "NOTIFY"

    ; Show the refresh button.
    IntOp $2 $1 + 14
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Type    "button"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Text    "$(UN_REFRESH_BUTTON)"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Left    "120"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Top     $1
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Right   "240"
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Bottom  $2
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 5" Flags   "NOTIFY"

    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Settings" NumFields 5
  ${EndIf}
FunctionEnd

Function un.ShowWelcome
  ; The welcome and finish pages don't get the correct colors for their labels
  ; like the other pages do, presumably because they're built by filling in an
  ; InstallOptions .ini file instead of from a dialog resource like the others.
  ; Field 2 is the header and Field 3 is the body text.
  ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 2" "HWND"
  SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW

  ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 3" "HWND"
  SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW

  ${If} $ShouldPromptForRefresh == "1"
    ; Field 4 is the profile refresh help link
    ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 4" "HWND"
    SetCtlColors $0 SYSCLR:HOTLIGHT SYSCLR:WINDOW
  ${EndIf}

  ; We need to overwrite the sidebar image so that we get it drawn with proper
  ; scaling if the display is scaled at anything above 100%.
  ${un.ChangeMUISidebarImage} "$PLUGINSDIR\modern-wizard.bmp"
FunctionEnd

Function un.leaveWelcome
  StrCpy $RefreshRequested "0"

  ${If} $ShouldPromptForRefresh == "1"
    ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Settings" "State"
    ${If} $0 == "5"
      ; Refresh button
      StrCpy $RefreshRequested "1"
    ${ElseIf} $0 == "4"
      ; Launch refresh help link, stay on this page
      Call un.LaunchRefreshHelpPage
      Abort
    ${EndIf}
  ${EndIf}

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    Banner::show /NOUNLOAD "$(BANNER_CHECK_EXISTING)"

    ; If we already found a window once and we're checking again, wait for an
    ; additional five seconds for the app to close.
    ${If} "$TmpVal" == "FoundAppWindow"
      Sleep 5000
    ${EndIf}

    ${PushFilesToCheck}

    ${un.CheckForFilesInUse} $TmpVal

    Banner::destroy

    ; If there are files in use $TmpVal will be "true"
    ${If} "$TmpVal" == "true"
      ; If it finds a window of the right class, then ManualCloseAppPrompt will
      ; abort leaving the value of $TmpVal set to "FoundAppWindow".
      StrCpy $TmpVal "FoundAppWindow"

      ${If} $RefreshRequested == "1"
        ${un.ManualCloseAppPrompt} "${MainWindowClass}" "$(WARN_MANUALLY_CLOSE_APP_REFRESH)"
        ${un.ManualCloseAppPrompt} "${DialogWindowClass}" "$(WARN_MANUALLY_CLOSE_APP_REFRESH)"
      ${Else}
        ${un.ManualCloseAppPrompt} "${MainWindowClass}" "$(WARN_MANUALLY_CLOSE_APP_UNINSTALL)"
        ${un.ManualCloseAppPrompt} "${DialogWindowClass}" "$(WARN_MANUALLY_CLOSE_APP_UNINSTALL)"
      ${EndIf}
      ; If the message window is not found set $TmpVal to "true" so the restart
      ; required message is displayed.
      ; In the case of a refresh request the restart required message will not be displayed;
      ; we're not trying to change the installation, so files in use only matter if the
      ; window is shown.
      StrCpy $TmpVal "true"
    ${EndIf}
  ${EndIf}

  ${If} $RefreshRequested == "1"
    Call un.LaunchAppForRefresh
    Quit
  ${EndIf}

  ${If} $ShouldPromptForRefresh == "1"
    ; Remove the custom controls.
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Settings" NumFields 3
    DeleteIniSec "$PLUGINSDIR\ioSpecial.ini" "Field 4"
    DeleteIniSec "$PLUGINSDIR\ioSpecial.ini" "Field 5"
  ${EndIf}

  ; Bring back the header bitmap for the next pages.
  GetDlgItem $0 $HWNDPARENT 1046
  ShowWindow $0 ${SW_SHOW}
FunctionEnd

Function un.preConfirm
  ; The header on the wizard pages doesn't get the correct text
  ; color by default for some reason, even though the other controls do.
  GetDlgItem $0 $HWNDPARENT 1037
  SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW
  ; Hide unused subheader (to avoid overlapping moved header)
  GetDlgItem $0 $HWNDPARENT 1038
  ShowWindow $0 ${SW_HIDE}

  ${If} ${FileExists} "$INSTDIR\distribution\modern-header.bmp"
    Delete "$PLUGINSDIR\modern-header.bmp"
    CopyFiles /SILENT "$INSTDIR\distribution\modern-header.bmp" "$PLUGINSDIR\modern-header.bmp"
  ${EndIf}
  ${un.ChangeMUIHeaderImage} "$PLUGINSDIR\modern-header.bmp"

  ; Setup the unconfirm.ini file for the Custom Uninstall Confirm Page
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Settings" NumFields "3"

  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 1" Text   "$(UN_CONFIRM_UNINSTALLED_FROM)"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 1" Bottom "15"

  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" Type   "text"
  ; The contents of this control must be set as follows in the pre function
  ; ${MUI_INSTALLOPTIONS_READ} $1 "unconfirm.ini" "Field 2" "HWND"
  ; SendMessage $1 ${WM_SETTEXT} 0 "STR:$INSTDIR"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" State  ""
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" Left   "0"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" Top    "17"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" Bottom "30"
  WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 2" flags  "READONLY"

  ${If} "$TmpVal" == "true"
    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 3" Type   "label"
    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 3" Text   "$(SUMMARY_REBOOT_REQUIRED_UNINSTALL)"
    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 3" Left   "0"
    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 3" Right  "-1"
    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 3" Top    "35"
    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Field 3" Bottom "45"

    WriteINIStr "$PLUGINSDIR\unconfirm.ini" "Settings" NumFields "3"
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "$(UN_CONFIRM_PAGE_TITLE)" ""

  ; The Summary custom page has a textbox that will automatically receive
  ; focus. This sets the focus to the Install button instead.
  !insertmacro MUI_INSTALLOPTIONS_INITDIALOG "unconfirm.ini"
  GetDlgItem $0 $HWNDPARENT 1
  System::Call "user32::SetFocus(i r0, i 0x0007, i,i)i"
  ${MUI_INSTALLOPTIONS_READ} $1 "unconfirm.ini" "Field 2" "HWND"
  SendMessage $1 ${WM_SETTEXT} 0 "STR:$INSTDIR"
  !insertmacro MUI_INSTALLOPTIONS_SHOW
FunctionEnd

Function un.onUninstSuccess
  ; Send a ping at un.onGUIEnd, to avoid freezing the GUI.
  StrCpy $ShouldSendPing "1"

  ${If} ${Silent}
    ; If this is a silent uninstall then un.onGUIEnd doesn't run, so do it now.
    Call un.SendUninstallPing
  ${EndIf}
FunctionEnd

Function un.preFinish
  ; Need to give the survey (readme) checkbox a few extra DU's of height
  ; to accommodate a potentially multi-line label. If the reboot flag is set,
  ; then we're not showing the survey checkbox and Field 4 is the "reboot now"
  ; radio button; setting it to go from 90 to 120 (instead of 90 to 100) would
  ; cover up Field 5 which is "reboot later", running from 110 to 120. For
  ; whatever reason child windows get created at the bottom of the z-order, so
  ; 4 overlaps 5.
  ${IfNot} ${RebootFlag}
    WriteINIStr "$PLUGINSDIR\ioSpecial.ini" "Field 4" Bottom "120"
  ${EndIf}

  ; We don't want the header bitmap showing on the finish page.
  GetDlgItem $0 $HWNDPARENT 1046
  ShowWindow $0 ${SW_HIDE}
FunctionEnd

Function un.ShowFinish
  ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 2" "HWND"
  SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW

  ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 3" "HWND"
  SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW

  ; We need to overwrite the sidebar image so that we get it drawn with proper
  ; scaling if the display is scaled at anything above 100%.
  ${un.ChangeMUISidebarImage} "$PLUGINSDIR\modern-wizard.bmp"

  ; Either Fields 4 and 5 are the reboot option radio buttons, or Field 4 is
  ; the survey checkbox and Field 5 doesn't exist. Either way, we need to
  ; clear the theme from them before we can set their background colors.
  ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 4" "HWND"
  System::Call 'uxtheme::SetWindowTheme(i $0, w " ", w " ")'
  SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW

  ClearErrors
  ReadINIStr $0 "$PLUGINSDIR\ioSpecial.ini" "Field 5" "HWND"
  ${IfNot} ${Errors}
    System::Call 'uxtheme::SetWindowTheme(i $0, w " ", w " ")'
    SetCtlColors $0 SYSCLR:WINDOWTEXT SYSCLR:WINDOW
  ${EndIf}
FunctionEnd

################################################################################
# Initialization Functions

Function .onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  ; We need this set up for most of the helper.exe operations.
  ${UninstallOnInitCommon}
FunctionEnd

Function un.onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  StrCpy $LANGUAGE 0
  StrCpy $ShouldOpenSurvey "0"

  ${un.UninstallUnOnInitCommon}

  ; setup the application model id registration value
  ${un.InitHashAppModelId} "$INSTDIR" "Software\Mozilla\${AppName}\TaskBarIDs"

  ; Find a default profile for this install.
  SetShellVarContext current
  ${un.FindInstallSpecificProfile}
  Pop $1
  GetFullPathName $1 $1

  ; If there is an existing default profile, offer profile refresh.
  StrCpy $ShouldPromptForRefresh "0"
  StrCpy $RefreshRequested "0"
  ${If} $1 != ""
    StrCpy $ShouldPromptForRefresh "1"
  ${EndIf}

  ; Load version info for uninstall ping (sent after uninstall is complete)
  ReadINIStr $InstalledVersion "$INSTDIR\application.ini" "App" "Version"
  ReadINIStr $InstalledBuildID "$INSTDIR\application.ini" "App" "BuildID"

  !insertmacro InitInstallOptionsFile "unconfirm.ini"
FunctionEnd

Function un.GUIInit
  ; Move header text down, roughly vertically centered in the header.
  ; Even if we're not changing the X, we can't set Y without also setting X.
  ; Child window positions have to be set in client coordinates, and these are
  ; left-to-right mirrored for RTL.
  GetDlgItem $0 $HWNDPARENT 1037
  ; Get current rect in screen coordinates
  System::Call "*(i 0, i 0, i 0, i 0) i .r2"
  System::Call "user32::GetWindowRect(p $0, p $2)"
  ; Convert screen coordinates to client coordinates (handles RTL mirroring)
  System::Call "user32::MapWindowPoints(p 0, p $HWNDPARENT, p $2, i 2)"
  System::Call "*$2(i . r3, i . r4, i, i)"
  System::Free $2
  ; Move down
  ${DialogUnitsToPixels} 8 Y $1
  IntOp $4 $4 + $1
  ; Set position, 0x0015 is SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE
  System::Call "user32::SetWindowPos(p $0, p 0, i $3, i $4, i 0, i 0, i 0x0015)"
FunctionEnd

Function .onGUIEnd
  ${OnEndCommon}
FunctionEnd

Function un.onGUIEnd
  ${un.OnEndCommon}

  ${If} $ShouldOpenSurvey == "1"
    ; Though these values are sometimes incorrect due to bug 444664 it happens
    ; so rarely it isn't worth working around it by reading the registry values.
    ${WinVerGetMajor} $0
    ${WinVerGetMinor} $1
    ${WinVerGetBuild} $2
    ${WinVerGetServicePackLevel} $3
    StrCpy $R1 "${URLUninstallSurvey}$0.$1.$2.$3"

    ; We can't just open the URL normally because we are most likely running
    ; elevated without an unelevated process to redirect through, and we're
    ; not going to go around starting elevated web browsers. But to start an
    ; unelevated process directly from here we need a pretty nasty hack; see
    ; the ExecInExplorer plugin code itself for the details.
    ; If we were the default browser and we've now been uninstalled, we need
    ; to take steps to make sure the user doesn't see an "open with" dialog;
    ; they're helping us out by answering this survey, they don't need more
    ; friction. Sometimes Windows 7 and 8 automatically switch the default to
    ; IE, but it isn't reliable, so we'll manually invoke IE in that case.
    ; Windows 10 always seems to just clear the default browser, so for it
    ; we'll manually invoke Edge using Edge's custom URI scheme.
    ${If} ${AtLeastWin10}
      ExecInExplorer::Exec "microsoft-edge:$R1"
    ${Else}
      ExecInExplorer::Exec "iexplore.exe" /cmdargs "$R1"
    ${EndIf}
  ${EndIf}

  ; Finally send the ping, there's no GUI to freeze in case it is slow.
  ${If} $ShouldSendPing == "1"
    Call un.SendUninstallPing
  ${EndIf}
FunctionEnd
