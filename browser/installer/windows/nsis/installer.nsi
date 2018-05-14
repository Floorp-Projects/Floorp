# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Required Plugins:
# AppAssocReg    http://nsis.sourceforge.net/Application_Association_Registration_plug-in
# ApplicationID  http://nsis.sourceforge.net/ApplicationID_plug-in
# CityHash       http://dxr.mozilla.org/mozilla-central/source/other-licenses/nsis/Contrib/CityHash
# ShellLink      http://nsis.sourceforge.net/ShellLink_plug-in
# UAC            http://nsis.sourceforge.net/UAC_plug-in
# ServicesHelper Mozilla specific plugin that is located in /other-licenses/nsis

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

Var TmpVal
Var InstallType
Var AddStartMenuSC
Var AddTaskbarSC
Var AddQuickLaunchSC
Var AddDesktopSC
Var InstallMaintenanceService
Var InstallOptionalExtensions
Var ExtensionRecommender
Var PageName
Var PreventRebootRequired

; By defining NO_STARTMENU_DIR an installer that doesn't provide an option for
; an application's Start Menu PROGRAMS directory and doesn't define the
; StartMenuDir variable can use the common InstallOnInitCommon macro.
!define NO_STARTMENU_DIR

; Attempt to elevate Standard Users in addition to users that
; are a member of the Administrators group.
!define NONADMIN_ELEVATE

!define AbortSurveyURL "http://www.kampyle.com/feedback_form/ff-feedback-form.php?site_code=8166124&form_id=12116&url="

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
!insertmacro WordFind
!insertmacro WordReplace

; The following includes are custom.
!include branding.nsi
!include defines.nsi
!include common.nsh
!include locales.nsi

VIAddVersionKey "FileDescription" "${BrandShortName} Installer"
VIAddVersionKey "OriginalFilename" "setup.exe"

; Must be inserted before other macros that use logging
!insertmacro _LoggingCommon

!insertmacro AddDisabledDDEHandlerValues
!insertmacro ChangeMUIHeaderImage
!insertmacro CheckForFilesInUse
!insertmacro CleanUpdateDirectories
!insertmacro CopyFilesFromDir
!insertmacro CreateRegKey
!insertmacro GetLongPath
!insertmacro GetPathFromString
!insertmacro GetParent
!insertmacro InitHashAppModelId
!insertmacro IsHandlerForInstallDir
!insertmacro IsPinnedToTaskBar
!insertmacro IsUserAdmin
!insertmacro LogDesktopShortcut
!insertmacro LogQuickLaunchShortcut
!insertmacro LogStartMenuShortcut
!insertmacro ManualCloseAppPrompt
!insertmacro PinnedToStartMenuLnkCount
!insertmacro RegCleanAppHandler
!insertmacro RegCleanMain
!insertmacro RegCleanUninstall
!insertmacro RemovePrecompleteEntries
!insertmacro SetAppLSPCategories
!insertmacro SetBrandNameVars
!insertmacro UpdateShortcutAppModelIDs
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
!insertmacro LeaveOptionsCommon
!insertmacro OnEndCommon
!insertmacro PreDirectoryCommon

Name "${BrandFullName}"
OutFile "setup.exe"
!ifdef HAVE_64BIT_BUILD
  InstallDir "$PROGRAMFILES64\${BrandFullName}\"
!else
  InstallDir "$PROGRAMFILES32\${BrandFullName}\"
!endif
ShowInstDetails nevershow

################################################################################
# Modern User Interface - MUI

!define MOZ_MUI_CUSTOM_ABORT
!define MUI_CUSTOMFUNCTION_ABORT "CustomAbort"
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

; Custom Components Page
!ifdef MOZ_MAINTENANCE_SERVICE
Page custom preComponents leaveComponents
!endif

; Custom Shortcuts Page
Page custom preShortcuts leaveShortcuts

; Custom Extensions Page
!ifdef MOZ_OPTIONAL_EXTENSIONS
Page custom preExtensions leaveExtensions
!endif

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

  StrCpy $R9 "true"
  StrCpy $PreventRebootRequired "false"
  ${GetParameters} $R8
  ${GetOptions} "$R8" "/INI=" $R7
  ${Unless} ${Errors}
    ; The configuration file must also exist
    ${If} ${FileExists} "$R7"
      ReadINIStr $R9 $R7 "Install" "RemoveDistributionDir"
      ReadINIStr $R8 $R7 "Install" "PreventRebootRequired"
      ${If} $R8 == "true"
        StrCpy $PreventRebootRequired "true"
      ${EndIf}
    ${EndIf}
  ${EndUnless}

  ${GetParameters} $R8
  ${InstallGetOption} $R8 "RemoveDistributionDir" $R9
  ${If} $R9 == "0"
    StrCpy $R9 "false"
  ${EndIf}
  ${InstallGetOption} $R8 "PreventRebootRequired" $PreventRebootRequired
  ${If} $PreventRebootRequired == "1"
    StrCpy $PreventRebootRequired "true"
  ${EndIf}

  ; Remove directories and files we always control before parsing the uninstall
  ; log so empty directories can be removed.
  ${If} ${FileExists} "$INSTDIR\updates"
    RmDir /r "$INSTDIR\updates"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\updated"
    RmDir /r "$INSTDIR\updated"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\defaults\shortcuts"
    RmDir /r "$INSTDIR\defaults\shortcuts"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\distribution"
  ${AndIf} $R9 != "false"
    RmDir /r "$INSTDIR\distribution"
  ${EndIf}

  ; Delete the app exe if present to prevent launching the app while we are
  ; installing.
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

  ; setup the application model id registration value
  ${InitHashAppModelId} "$INSTDIR" "Software\Mozilla\${AppName}\TaskBarIDs"

  ; Remove the updates directory
  ${CleanUpdateDirectories} "Mozilla\Firefox" "Mozilla\updates"

  ${RemoveDeprecatedFiles}
  ${RemovePrecompleteEntries} "false"

  ${If} ${FileExists} "$INSTDIR\defaults\pref\channel-prefs.js"
    Delete "$INSTDIR\defaults\pref\channel-prefs.js"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\defaults\pref"
    RmDir "$INSTDIR\defaults\pref"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\defaults"
    RmDir "$INSTDIR\defaults"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\uninstall"
    ; Remove the uninstall directory that we control
    RmDir /r "$INSTDIR\uninstall"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\update-settings.ini"
    Delete "$INSTDIR\update-settings.ini"
  ${EndIf}

  ; Explictly remove empty webapprt dir in case it exists (bug 757978).
  RmDir "$INSTDIR\webapprt\components"
  RmDir "$INSTDIR\webapprt"

  ${InstallStartCleanupCommon}
SectionEnd

Section "-Application" APP_IDX
  ${StartUninstallLog}

  SetDetailsPrint both
  DetailPrint $(STATUS_INSTALL_APP)
  SetDetailsPrint none

  ${LogHeader} "Installing Main Files"
  ${CopyFilesFromDir} "$EXEDIR\core" "$INSTDIR" \
                      "$(ERROR_CREATE_DIRECTORY_PREFIX)" \
                      "$(ERROR_CREATE_DIRECTORY_SUFFIX)"

  ; Register DLLs
  ; XXXrstrong - AccessibleMarshal.dll can be used by multiple applications but
  ; is only registered for the last application installed. When the last
  ; application installed is uninstalled AccessibleMarshal.dll will no longer be
  ; registered. bug 338878
  ${LogHeader} "DLL Registration"
  ClearErrors
  ${RegisterDLL} "$INSTDIR\AccessibleMarshal.dll"
  ${If} ${Errors}
    ${LogMsg} "** ERROR Registering: $INSTDIR\AccessibleMarshal.dll **"
  ${Else}
    ${LogUninstall} "DLLReg: \AccessibleMarshal.dll"
    ${LogMsg} "Registered: $INSTDIR\AccessibleMarshal.dll"
  ${EndIf}

  ClearErrors

  ${RegisterDLL} "$INSTDIR\AccessibleHandler.dll"
  ${If} ${Errors}
    ${LogMsg} "** ERROR Registering: $INSTDIR\AccessibleHandler.dll **"
  ${Else}
    ${LogUninstall} "DLLReg: \AccessibleHandler.dll"
    ${LogMsg} "Registered: $INSTDIR\AccessibleHandler.dll"
  ${EndIf}

  ClearErrors

  ; Default for creating Start Menu shortcut
  ; (1 = create, 0 = don't create)
  ${If} $AddStartMenuSC == ""
    StrCpy $AddStartMenuSC "1"
  ${EndIf}

  ; Default for creating Quick Launch shortcut (1 = create, 0 = don't create)
  ${If} $AddQuickLaunchSC == ""
    ; Don't install the quick launch shortcut on Windows 7
    ${If} ${AtLeastWin7}
      StrCpy $AddQuickLaunchSC "0"
    ${Else}
      StrCpy $AddQuickLaunchSC "1"
    ${EndIf}
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
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    StrCpy $TmpVal "HKCU" ; used primarily for logging
  ${Else}
    SetShellVarContext all  ; Set SHCTX to HKLM
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
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
  ${Set32to64DidMigrateReg}

  ; The previous installer adds several regsitry values to both HKLM and HKCU.
  ; We now try to add to HKLM and if that fails to HKCU

  ; The order that reg keys and values are added is important if you use the
  ; uninstall log to remove them on uninstall. When using the uninstall log you
  ; MUST add children first so they will be removed first on uninstall so they
  ; will be empty when the key is deleted. This allows the uninstaller to
  ; specify that only empty keys will be deleted.
  ${SetAppKeys}

  ${FixClassKeys}

  ; Uninstall keys can only exist under HKLM on some versions of windows. Since
  ; it doesn't cause problems always add them.
  ${SetUninstallKeys}

  ; On install always add the FirefoxHTML and FirefoxURL keys.
  ; An empty string is used for the 5th param because FirefoxHTML is not a
  ; protocol handler.
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  StrCpy $2 "$\"$8$\" -osint -url $\"%1$\""

  ; In Win8, the delegate execute handler picks up the value in FirefoxURL and
  ; FirefoxHTML to launch the desktop browser when it needs to.
  ${AddDisabledDDEHandlerValues} "FirefoxHTML-$AppUserModelID" "$2" "$8,1" \
                                 "${AppRegName} Document" ""
  ${AddDisabledDDEHandlerValues} "FirefoxURL-$AppUserModelID" "$2" "$8,1" \
                                 "${AppRegName} URL" "true"

  ; For pre win8, the following keys should only be set if we can write to HKLM.
  ; For post win8, the keys below can be set in HKCU if needed.
  ${If} $TmpVal == "HKLM"
    ; Set the Start Menu Internet and Registered App HKLM registry keys.
    ${SetStartMenuInternet} "HKLM"
    ${FixShellIconHandler} "HKLM"
  ${ElseIf} ${AtLeastWin8}
    ; Set the Start Menu Internet and Registered App HKCU registry keys.
    ${SetStartMenuInternet} "HKCU"
    ${FixShellIconHandler} "HKCU"
  ${EndIf}

!ifdef MOZ_MAINTENANCE_SERVICE
  ; If the maintenance service page was displayed then a value was already
  ; explicitly selected for installing the maintenance service and
  ; and so InstallMaintenanceService will already be 0 or 1.
  ; If the maintenance service page was not displayed then
  ; InstallMaintenanceService will be equal to "".
  ${If} $InstallMaintenanceService == ""
    Call IsUserAdmin
    Pop $R0
    ${If} $R0 == "true"
    ; Only proceed if we have HKLM write access
    ${AndIf} $TmpVal == "HKLM"
      ; The user is an admin, so we should default to installing the service.
      StrCpy $InstallMaintenanceService "1"
    ${Else}
      ; The user is not admin, so we can't install the service.
      StrCpy $InstallMaintenanceService "0"
    ${EndIf}
  ${EndIf}

  ${If} $InstallMaintenanceService == "1"
    ; The user wants to install the maintenance service, so execute
    ; the pre-packaged maintenance service installer.
    ; This option can only be turned on if the user is an admin so there
    ; is no need to use ExecShell w/ verb runas to enforce elevated.
    nsExec::Exec "$\"$INSTDIR\maintenanceservice_installer.exe$\""
  ${EndIf}
!endif

  ; These need special handling on uninstall since they may be overwritten by
  ; an install into a different location.
  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\App Paths\${FileMainEXE}"
  ${WriteRegStr2} $TmpVal "$0" "" "$INSTDIR\${FileMainEXE}" 0
  ${WriteRegStr2} $TmpVal "$0" "Path" "$INSTDIR" 0

  StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\$R9"
  ${CreateRegKey} "$TmpVal" "$0" 0
  StrCpy $0 "Software\Microsoft\MediaPlayer\ShimInclusionList\plugin-container.exe"
  ${CreateRegKey} "$TmpVal" "$0" 0

  ${If} $TmpVal == "HKLM"
    ; Set the permitted LSP Categories
    ${SetAppLSPCategories} ${LSP_CATEGORIES}
  ${EndIf}

  ; Create shortcuts
  ${LogHeader} "Adding Shortcuts"

  ; Remove the start menu shortcuts and directory if the SMPROGRAMS section
  ; exists in the shortcuts_log.ini and the SMPROGRAMS. The installer's shortcut
  ; creation code will create the shortcut in the root of the Start Menu
  ; Programs directory.
  ${RemoveStartMenuDir}

  ; Always add the application's shortcuts to the shortcuts log ini file. The
  ; DeleteShortcuts macro will do the right thing on uninstall if the
  ; shortcuts don't exist.
  ${LogStartMenuShortcut} "${BrandShortName}.lnk"
  ${LogQuickLaunchShortcut} "${BrandShortName}.lnk"
  ${LogDesktopShortcut} "${BrandShortName}.lnk"

  ; Best effort to update the Win7 taskbar and start menu shortcut app model
  ; id's. The possible contexts are current user / system and the user that
  ; elevated the installer.
  Call FixShortcutAppModelIDs
  ; If the current context is all also perform Win7 taskbar and start menu link
  ; maintenance for the current user context.
  ${If} $TmpVal == "HKLM"
    SetShellVarContext current  ; Set SHCTX to HKCU
    Call FixShortcutAppModelIDs
    SetShellVarContext all  ; Set SHCTX to HKLM
  ${EndIf}

  ; If running elevated also perform Win7 taskbar and start menu link
  ; maintenance for the unelevated user context in case that is different than
  ; the current user.
  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $0
  ${Unless} ${Errors}
    GetFunctionAddress $0 FixShortcutAppModelIDs
    UAC::ExecCodeSegment $0
  ${EndUnless}

  ; UAC only allows elevating to an Admin account so there is no need to add
  ; the Start Menu or Desktop shortcuts from the original unelevated process
  ; since this will either add it for the user if unelevated or All Users if
  ; elevated.
  ${If} $AddStartMenuSC == 1
    ; See if there's an existing shortcut for this installation using the old
    ; name that we should just rename, instead of creating a new shortcut.
    ; We could do this renaming even when $AddStartMenuSC is false; the idea
    ; behind not doing that is to interpret "false" as "don't do anything
    ; involving start menu shortcuts at all." We could also try to do this for
    ; both shell contexts, but that won't typically accomplish anything.
    ${If} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
      ShellLink::GetShortCutTarget "$SMPROGRAMS\${BrandFullName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
      ${If} $0 == "$INSTDIR\${FileMainEXE}"
      ${AndIfNot} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
        Rename "$SMPROGRAMS\${BrandFullName}.lnk" \
               "$SMPROGRAMS\${BrandShortName}.lnk"
        ${LogMsg} "Renamed existing shortcut to $SMPROGRAMS\${BrandShortName}.lnk"
      ${EndIf}
    ${Else}
      CreateShortCut "$SMPROGRAMS\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
      ${If} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
        ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandShortName}.lnk" \
                                               "$INSTDIR"
        ${If} "$AppUserModelID" != ""
          ApplicationID::Set "$SMPROGRAMS\${BrandShortName}.lnk" \
                             "$AppUserModelID" "true"
        ${EndIf}
        ${LogMsg} "Added Shortcut: $SMPROGRAMS\${BrandShortName}.lnk"
      ${Else}
        ${LogMsg} "** ERROR Adding Shortcut: $SMPROGRAMS\${BrandShortName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ; Update lastwritetime of the Start Menu shortcut to clear the tile cache.
  ; Do this for both shell contexts in case the user has shortcuts in multiple
  ; locations, then restore the previous context at the end.
  ${If} ${AtLeastWin8}
    SetShellVarContext all
    ${TouchStartMenuShortcut}
    SetShellVarContext current
    ${TouchStartMenuShortcut}
    ${If} $TmpVal == "HKLM"
      SetShellVarContext all
    ${ElseIf} $TmpVal == "HKCU"
      SetShellVarContext current
    ${EndIf}
  ${EndIf}

  ${If} $AddDesktopSC == 1
    ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      ShellLink::GetShortCutTarget "$DESKTOP\${BrandFullName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
      ${If} $0 == "$INSTDIR\${FileMainEXE}"
      ${AndIfNot} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
        Rename "$DESKTOP\${BrandFullName}.lnk" "$DESKTOP\${BrandShortName}.lnk"
        ${LogMsg} "Renamed existing shortcut to $DESKTOP\${BrandShortName}.lnk"
      ${EndIf}
    ${Else}
      CreateShortCut "$DESKTOP\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
      ${If} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
        ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandShortName}.lnk" \
                                               "$INSTDIR"
        ${If} "$AppUserModelID" != ""
          ApplicationID::Set "$DESKTOP\${BrandShortName}.lnk" \
                             "$AppUserModelID" "true"
        ${EndIf}
        ${LogMsg} "Added Shortcut: $DESKTOP\${BrandShortName}.lnk"
      ${Else}
        ${LogMsg} "** ERROR Adding Shortcut: $DESKTOP\${BrandShortName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ; If elevated the Quick Launch shortcut must be added from the unelevated
  ; original process.
  ${If} $AddQuickLaunchSC == 1
    ${Unless} ${AtLeastWin7}
      ClearErrors
      ${GetParameters} $0
      ${GetOptions} "$0" "/UAC:" $0
      ${If} ${Errors}
        Call AddQuickLaunchShortcut
        ${LogMsg} "Added Shortcut: $QUICKLAUNCH\${BrandShortName}.lnk"
      ${Else}
        ; It is not possible to add a log entry from the unelevated process so
        ; add the log entry without the path since there is no simple way to
        ; know the correct full path.
        ${LogMsg} "Added Quick Launch Shortcut: ${BrandShortName}.lnk"
        GetFunctionAddress $0 AddQuickLaunchShortcut
        UAC::ExecCodeSegment $0
      ${EndIf}
    ${EndUnless}
  ${EndIf}

!ifdef MOZ_OPTIONAL_EXTENSIONS
  ${If} ${FileExists} "$INSTDIR\distribution\optional-extensions"
    ${LogHeader} "Installing optional extensions if requested"

    ${If} $InstallOptionalExtensions != "0"
    ${AndIf} ${FileExists} "$INSTDIR\distribution\setup.ini"
      ${Unless} ${FileExists} "$INSTDIR\distribution\extensions"
        CreateDirectory "$INSTDIR\distribution\extensions"
      ${EndUnless}

      StrCpy $0 0
      ${Do}
        ClearErrors
        ReadINIStr $1 "$INSTDIR\distribution\setup.ini" "OptionalExtensions" \
                                                        "extension.$0.id"
        ${If} ${Errors}
          ${ExitDo}
        ${EndIf}

        ReadINIStr $2 "$INSTDIR\distribution\setup.ini" "OptionalExtensions" \
                                                        "extension.$0.checked"
        ${If} $2 != ${BST_UNCHECKED}
          ${LogMsg} "Installing optional extension: $1"
          CopyFiles /SILENT "$INSTDIR\distribution\optional-extensions\$1.xpi" \
                            "$INSTDIR\distribution\extensions"
        ${EndIf}

        IntOp $0 $0 + 1
      ${Loop}
    ${EndIf}

    ${LogMsg} "Removing the optional-extensions directory"
    RMDir /r /REBOOTOK "$INSTDIR\distribution\optional-extensions"
  ${EndIf}
!endif

!ifdef MOZ_MAINTENANCE_SERVICE
  ${If} $TmpVal == "HKLM"
    ; Add the registry keys for allowed certificates.
    ${AddMaintCertKeys}
  ${EndIf}
!endif
SectionEnd

; Cleanup operations to perform at the end of the installation.
Section "-InstallEndCleanup"
  SetDetailsPrint both
  DetailPrint "$(STATUS_CLEANUP)"
  SetDetailsPrint none

  ${Unless} ${Silent}
    ClearErrors
    ${MUI_INSTALLOPTIONS_READ} $0 "summary.ini" "Field 4" "State"
    ${If} "$0" == "1"
      ; NB: this code is duplicated in stub.nsi. Please keep in sync.
      ; For data migration in the app, we want to know what the default browser
      ; value was before we changed it. To do so, we read it here and store it
      ; in our own registry key.
      StrCpy $0 ""
      AppAssocReg::QueryCurrentDefault "http" "protocol" "effective"
      Pop $1
      ; If the method hasn't failed, $1 will contain the progid. Check:
      ${If} "$1" != "method failed"
      ${AndIf} "$1" != "method not available"
        ; Read the actual command from the progid
        ReadRegStr $0 HKCR "$1\shell\open\command" ""
      ${EndIf}
      ; If using the App Association Registry didn't happen or failed, fall back
      ; to the effective http default:
      ${If} "$0" == ""
        ReadRegStr $0 HKCR "http\shell\open\command" ""
      ${EndIf}
      ; If we have something other than empty string now, write the value.
      ${If} "$0" != ""
        ClearErrors
        WriteRegStr HKCU "Software\Mozilla\Firefox" "OldDefaultBrowserCommand" "$0"
      ${EndIf}

      ${LogHeader} "Setting as the default browser"
      ClearErrors
      ${GetParameters} $0
      ${GetOptions} "$0" "/UAC:" $0
      ${If} ${Errors}
        Call SetAsDefaultAppUserHKCU
      ${Else}
        GetFunctionAddress $0 SetAsDefaultAppUserHKCU
        UAC::ExecCodeSegment $0
      ${EndIf}
    ${ElseIfNot} ${Errors}
      ${LogHeader} "Writing default-browser opt-out"
      ClearErrors
      WriteRegStr HKCU "Software\Mozilla\Firefox" "DefaultBrowserOptOut" "True"
      ${If} ${Errors}
        ${LogMsg} "Error writing default-browser opt-out"
      ${EndIf}
    ${EndIf}
  ${EndUnless}

  ; Adds a pinned Task Bar shortcut (see MigrateTaskBarShortcut for details).
  ${MigrateTaskBarShortcut}

  ; Add the Firewall entries during install
  Call AddFirewallEntries

  ; Refresh desktop icons
  System::Call "shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i ${SHCNF_DWORDFLUSH}, i 0, i 0)"

  ${InstallEndCleanupCommon}

  ${If} $PreventRebootRequired == "true"
    SetRebootFlag false
  ${EndIf}

  ${If} ${RebootFlag}
    ; Admin is required to delete files on reboot so only add the moz-delete if
    ; the user is an admin. After calling UAC::IsAdmin $0 will equal 1 if the
    ; user is an admin.
    UAC::IsAdmin
    ${If} "$0" == "1"
      ; When a reboot is required give SHChangeNotify time to finish the
      ; refreshing the icons so the OS doesn't display the icons from helper.exe
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
      ${EndIf}

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
  ${EndIf}
SectionEnd

################################################################################
# Install Abort Survey Functions

Function CustomAbort
  ${If} "${AB_CD}" == "en-US"
  ${AndIf} "$PageName" != ""
  ${AndIf} ${FileExists} "$EXEDIR\core\distribution\distribution.ini"
    ReadINIStr $0 "$EXEDIR\core\distribution\distribution.ini" "Global" "about"
    ClearErrors
    ${WordFind} "$0" "Funnelcake" "E#" $1
    ${Unless} ${Errors}
      ; Yes = fill out the survey and exit, No = don't fill out survey and exit,
      ; Cancel = don't exit.
      MessageBox MB_YESNO|MB_ICONEXCLAMATION \
                 "Would you like to tell us why you are canceling this installation?" \
                 IDYes +1 IDNO CustomAbort_finish
      ${If} "$PageName" == "Welcome"
          GetFunctionAddress $0 AbortSurveyWelcome
      ${ElseIf} "$PageName" == "Options"
          GetFunctionAddress $0 AbortSurveyOptions
      ${ElseIf} "$PageName" == "Directory"
          GetFunctionAddress $0 AbortSurveyDirectory
      ${ElseIf} "$PageName" == "Shortcuts"
          GetFunctionAddress $0 AbortSurveyShortcuts
      ${ElseIf} "$PageName" == "Summary"
          GetFunctionAddress $0 AbortSurveySummary
      ${EndIf}
      ClearErrors
      ${GetParameters} $1
      ${GetOptions} "$1" "/UAC:" $2
      ${If} ${Errors}
        Call $0
      ${Else}
        UAC::ExecCodeSegment $0
      ${EndIf}

      CustomAbort_finish:
      Return
    ${EndUnless}
  ${EndIf}

  MessageBox MB_YESNO|MB_ICONEXCLAMATION "$(MOZ_MUI_TEXT_ABORTWARNING)" \
             IDYES +1 IDNO +2
  Return
  Abort
FunctionEnd

Function AbortSurveyWelcome
  ExecShell "open" "${AbortSurveyURL}step1"
FunctionEnd

Function AbortSurveyOptions
  ExecShell "open" "${AbortSurveyURL}step2"
FunctionEnd

Function AbortSurveyDirectory
  ExecShell "open" "${AbortSurveyURL}step3"
FunctionEnd

Function AbortSurveyShortcuts
  ExecShell "open" "${AbortSurveyURL}step4"
FunctionEnd

Function AbortSurveySummary
  ExecShell "open" "${AbortSurveyURL}step5"
FunctionEnd

################################################################################
# Helper Functions

Function AddQuickLaunchShortcut
  CreateShortCut "$QUICKLAUNCH\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
  ${If} ${FileExists} "$QUICKLAUNCH\${BrandShortName}.lnk"
    ShellLink::SetShortCutWorkingDirectory "$QUICKLAUNCH\${BrandShortName}.lnk" \
                                           "$INSTDIR"
  ${EndIf}
FunctionEnd

Function CheckExistingInstall
  ; If there is a pending file copy from a previous upgrade don't allow
  ; installing until after the system has rebooted.
  IfFileExists "$INSTDIR\${FileMainEXE}.moz-upgrade" +1 +4
  MessageBox MB_YESNO|MB_ICONEXCLAMATION "$(WARN_RESTART_REQUIRED_UPGRADE)" IDNO +2
  Reboot
  Quit

  ; If there is a pending file deletion from a previous uninstall don't allow
  ; installing until after the system has rebooted.
  IfFileExists "$INSTDIR\${FileMainEXE}.moz-delete" +1 +4
  MessageBox MB_YESNO|MB_ICONEXCLAMATION "$(WARN_RESTART_REQUIRED_UNINSTALL)" IDNO +2
  Reboot
  Quit

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    ; Disable the next, cancel, and back buttons
    GetDlgItem $0 $HWNDPARENT 1 ; Next button
    EnableWindow $0 0
    GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
    EnableWindow $0 0
    GetDlgItem $0 $HWNDPARENT 3 ; Back button
    EnableWindow $0 0

    Banner::show /NOUNLOAD "$(BANNER_CHECK_EXISTING)"

    ${If} "$TmpVal" == "FoundMessageWindow"
      Sleep 5000
    ${EndIf}

    ${PushFilesToCheck}

    ; Store the return value in $TmpVal so it is less likely to be accidentally
    ; overwritten elsewhere.
    ${CheckForFilesInUse} $TmpVal

    Banner::destroy

    ; Enable the next, cancel, and back buttons
    GetDlgItem $0 $HWNDPARENT 1 ; Next button
    EnableWindow $0 1
    GetDlgItem $0 $HWNDPARENT 2 ; Cancel button
    EnableWindow $0 1
    GetDlgItem $0 $HWNDPARENT 3 ; Back button
    EnableWindow $0 1

    ${If} "$TmpVal" == "true"
      StrCpy $TmpVal "FoundMessageWindow"
      ${ManualCloseAppPrompt} "${WindowClass}" "$(WARN_MANUALLY_CLOSE_APP_INSTALL)"
      StrCpy $TmpVal "true"
    ${EndIf}
  ${EndIf}
FunctionEnd

Function LaunchApp
!ifndef DEV_EDITION
  ${ManualCloseAppPrompt} "${WindowClass}" "$(WARN_MANUALLY_CLOSE_APP_LAUNCH)"
!endif

  ClearErrors
  ${GetParameters} $0
  ${GetOptions} "$0" "/UAC:" $1
  ${If} ${Errors}
    ${ExecAndWaitForInputIdle} "$\"$INSTDIR\${FileMainEXE}$\""
  ${Else}
    GetFunctionAddress $0 LaunchAppFromElevatedProcess
    UAC::ExecCodeSegment $0
  ${EndIf}
FunctionEnd

Function LaunchAppFromElevatedProcess
  ; Set our current working directory to the application's install directory
  ; otherwise the 7-Zip temp directory will be in use and won't be deleted.
  SetOutPath "$INSTDIR"
  ${ExecAndWaitForInputIdle} "$\"$INSTDIR\${FileMainEXE}$\""
FunctionEnd

################################################################################
# Language

!insertmacro MOZ_MUI_LANGUAGE 'baseLocale'
!verbose push
!verbose 3
!include "overrideLocale.nsh"
!include "customLocale.nsh"
!ifdef MOZ_OPTIONAL_EXTENSIONS
!include "extensionsLocale.nsh"
!endif
!verbose pop

; Set this after the locale files to override it if it is in the locale
; using " " for BrandingText will hide the "Nullsoft Install System..." branding
BrandingText " "

################################################################################
# Page pre, show, and leave functions

Function preWelcome
  StrCpy $PageName "Welcome"
  ${If} ${FileExists} "$EXEDIR\core\distribution\modern-wizard.bmp"
    Delete "$PLUGINSDIR\modern-wizard.bmp"
    CopyFiles /SILENT "$EXEDIR\core\distribution\modern-wizard.bmp" "$PLUGINSDIR\modern-wizard.bmp"
  ${EndIf}
FunctionEnd

Function preOptions
  StrCpy $PageName "Options"
  ${If} ${FileExists} "$EXEDIR\core\distribution\modern-header.bmp"
  ${AndIf} $hHeaderBitmap == ""
    Delete "$PLUGINSDIR\modern-header.bmp"
    CopyFiles /SILENT "$EXEDIR\core\distribution\modern-header.bmp" "$PLUGINSDIR\modern-header.bmp"
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

  ${LeaveOptionsCommon}

  ${If} $InstallType == ${INSTALLTYPE_BASIC}
    Call CheckExistingInstall
  ${EndIf}
FunctionEnd

Function preDirectory
  StrCpy $PageName "Directory"
  ${PreDirectoryCommon}
FunctionEnd

Function leaveDirectory
  ${If} $InstallType == ${INSTALLTYPE_BASIC}
    Call CheckExistingInstall
  ${EndIf}
  ${LeaveDirectoryCommon} "$(WARN_DISK_SPACE)" "$(WARN_WRITE_ACCESS)"
FunctionEnd

Function preShortcuts
  StrCpy $PageName "Shortcuts"
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

  ; Don't install the quick launch shortcut on Windows 7
  ${Unless} ${AtLeastWin7}
    ${MUI_INSTALLOPTIONS_READ} $AddQuickLaunchSC "shortcuts.ini" "Field 4" "State"
  ${EndUnless}

  ${If} $InstallType == ${INSTALLTYPE_CUSTOM}
    Call CheckExistingInstall
  ${EndIf}
FunctionEnd

!ifdef MOZ_MAINTENANCE_SERVICE
Function preComponents
  ; If the service already exists, don't show this page
  ServicesHelper::IsInstalled "MozillaMaintenance"
  Pop $R9
  ${If} $R9 == 1
    ; The service already exists so don't show this page.
    Abort
  ${EndIf}

  ; Don't show the custom components page if the
  ; user is not an admin
  Call IsUserAdmin
  Pop $R9
  ${If} $R9 != "true"
    Abort
  ${EndIf}

  ; Only show the maintenance service page if we have write access to HKLM
  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" \
              "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    ClearErrors
    Abort
  ${Else}
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
  ${EndIf}

  StrCpy $PageName "Components"
  ${CheckCustomCommon}
  !insertmacro MUI_HEADER_TEXT "$(COMPONENTS_PAGE_TITLE)" "$(COMPONENTS_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "components.ini"
FunctionEnd

Function leaveComponents
  ${MUI_INSTALLOPTIONS_READ} $0 "components.ini" "Settings" "State"
  ${If} $0 != 0
    Abort
  ${EndIf}
  ${MUI_INSTALLOPTIONS_READ} $InstallMaintenanceService "components.ini" "Field 2" "State"
  ${If} $InstallType == ${INSTALLTYPE_CUSTOM}
    Call CheckExistingInstall
  ${EndIf}
FunctionEnd
!endif

!ifdef MOZ_OPTIONAL_EXTENSIONS
Function preExtensions
  StrCpy $PageName "Extensions"
  ${CheckCustomCommon}

  ; Abort if no optional extensions configured in distribution/setup.ini
  ${If} ${FileExists} "$EXEDIR\core\distribution\setup.ini"
    ClearErrors
    ReadINIStr $ExtensionRecommender "$EXEDIR\core\distribution\setup.ini" \
      "OptionalExtensions" "Recommender.${AB_CD}"
    ${If} ${Errors}
      ClearErrors
      ReadINIStr $ExtensionRecommender "$EXEDIR\core\distribution\setup.ini" \
        "OptionalExtensions" "Recommender"
    ${EndIf}

    ${If} ${Errors}
      ClearErrors
      Abort
    ${EndIf}
  ${Else}
    Abort
  ${EndIf}

  !insertmacro MUI_HEADER_TEXT "$(EXTENSIONS_PAGE_TITLE)" "$(EXTENSIONS_PAGE_SUBTITLE)"
  !insertmacro MUI_INSTALLOPTIONS_DISPLAY "extensions.ini"
FunctionEnd

Function leaveExtensions
  ${MUI_INSTALLOPTIONS_READ} $0 "extensions.ini" "Settings" "NumFields"
  ${MUI_INSTALLOPTIONS_READ} $1 "extensions.ini" "Settings" "State"

  ; $0 is count of checkboxes
  IntOp $0 $0 - 1

  ${If} $1 > $0
    Abort
  ${ElseIf} $1 == 0
    ; $1 is count of selected optional extension(s)
    StrCpy $1 0

    StrCpy $2 2
    ${Do}
      ${MUI_INSTALLOPTIONS_READ} $3 "extensions.ini" "Field $2" "State"
      ${If} $3 == ${BST_CHECKED}
        IntOp $1 $1 + 1
      ${EndIf}

      IntOp $4 $2 - 2
      WriteINIStr "$EXEDIR\core\distribution\setup.ini" \
        "OptionalExtensions" "extension.$4.checked" "$3"

      ${If} $0 == $2
        ${ExitDo}
      ${Else}
        IntOp $2 $2 + 1
      ${EndIf}
    ${Loop}

    ; Different from state of field 1, "0" means no optional extensions selected
    ${If} $1 > 0
      StrCpy $InstallOptionalExtensions "1"
    ${Else}
      StrCpy $InstallOptionalExtensions "0"
    ${EndIf}

    ${If} $InstallType == ${INSTALLTYPE_CUSTOM}
      Call CheckExistingInstall
    ${EndIf}
  ${ElseIf} $1 == 1
    ; Check/uncheck all optional extensions with field 1
    ${MUI_INSTALLOPTIONS_READ} $1 "extensions.ini" "Field 1" "State"

    StrCpy $2 2
    ${Do}
      ${MUI_INSTALLOPTIONS_READ} $3 "extensions.ini" "Field $2" "HWND"
      SendMessage $3 ${BM_SETCHECK} $1 0

      ${If} $0 == $2
        ${ExitDo}
      ${Else}
        IntOp $2 $2 + 1
      ${EndIf}
    ${Loop}

    Abort
  ${ElseIf} $1 > 1
    StrCpy $1 ${BST_CHECKED}

    StrCpy $2 2
    ${Do}
      ${MUI_INSTALLOPTIONS_READ} $3 "extensions.ini" "Field $2" "State"
      ${If} $3 == ${BST_UNCHECKED}
        StrCpy $1 ${BST_UNCHECKED}
        ${ExitDo}
      ${EndIf}

      ${If} $0 == $2
        ${ExitDo}
      ${Else}
        IntOp $2 $2 + 1
      ${EndIf}
    ${Loop}

    ; Check field 1 only if all optional extensions are selected
    ${MUI_INSTALLOPTIONS_READ} $3 "extensions.ini" "Field 1" "HWND"
    SendMessage $3 ${BM_SETCHECK} $1 0

    Abort
  ${EndIf}
FunctionEnd
!endif

Function preSummary
  StrCpy $PageName "Summary"
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
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Left   "0"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Top    "130"
  WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Bottom "150"

  ${If} ${FileExists} "$INSTDIR\${FileMainEXE}"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Text "$(SUMMARY_UPGRADE_CLICK)"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Settings" NextButtonText "$(UPGRADE_BUTTON)"
  ${Else}
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field 3" Text "$(SUMMARY_INSTALL_CLICK)"
    DeleteINIStr "$PLUGINSDIR\summary.ini" "Settings" NextButtonText
  ${EndIf}


  ; Remove the "Field 4" ini section in case the user hits back and changes the
  ; installation directory which could change whether the make default checkbox
  ; should be displayed.
  DeleteINISec "$PLUGINSDIR\summary.ini" "Field 4"

  ; Check if it is possible to write to HKLM
  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" "Write Test"
  ${Unless} ${Errors}
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    ; Check if Firefox is the http handler for this user.
    SetShellVarContext current ; Set SHCTX to the current user
    ${IsHandlerForInstallDir} "http" $R9
    ${If} $TmpVal == "HKLM"
      SetShellVarContext all ; Set SHCTX to all users
    ${EndIf}
    ; If Firefox isn't the http handler for this user show the option to set
    ; Firefox as the default browser.
    ${If} "$R9" != "true"
    ${AndIf} ${AtMostWin2008R2}
      WriteINIStr "$PLUGINSDIR\summary.ini" "Settings" NumFields "4"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Type   "checkbox"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Text   "$(SUMMARY_TAKE_DEFAULTS)"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Left   "0"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Right  "-1"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" State  "1"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Top    "32"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field 4" Bottom "53"
    ${EndIf}
  ${EndUnless}

  ${If} "$TmpVal" == "true"
    ; If there is already a Type entry in the "Field 4" section with a value of
    ; checkbox then the set as the default browser checkbox is displayed and
    ; this text must be moved below it.
    ReadINIStr $0 "$PLUGINSDIR\summary.ini" "Field 4" "Type"
    ${If} "$0" == "checkbox"
      StrCpy $0 "5"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Top    "53"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Bottom "68"
    ${Else}
      StrCpy $0 "4"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Top    "35"
      WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Bottom "50"
    ${EndIf}
    WriteINIStr "$PLUGINSDIR\summary.ini" "Settings" NumFields "$0"

    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Type   "label"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Text   "$(SUMMARY_REBOOT_REQUIRED_INSTALL)"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Left   "0"
    WriteINIStr "$PLUGINSDIR\summary.ini" "Field $0" Right  "-1"
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
  StrCpy $PageName ""
  ${EndInstallLog} "${BrandFullName}"
  !insertmacro MUI_INSTALLOPTIONS_WRITE "ioSpecial.ini" "settings" "cancelenabled" "0"
FunctionEnd

################################################################################
# Initialization Functions

Function .onInit
  ; Remove the current exe directory from the search order.
  ; This only effects LoadLibrary calls and not implicitly loaded DLLs.
  System::Call 'kernel32::SetDllDirectoryW(w "")'

  StrCpy $PageName ""
  StrCpy $LANGUAGE 0
  ${SetBrandNameVars} "$EXEDIR\core\distribution\setup.ini"

  ; Don't install on systems that don't support SSE2. The parameter value of
  ; 10 is for PF_XMMI64_INSTRUCTIONS_AVAILABLE which will check whether the
  ; SSE2 instruction set is available. Result returned in $R7.
  System::Call "kernel32::IsProcessorFeaturePresent(i 10)i .R7"

  ; Windows NT 6.0 (Vista/Server 2008) and lower are not supported.
  ${Unless} ${AtLeastWin7}
    ${If} "$R7" == "0"
      strCpy $R7 "$(WARN_MIN_SUPPORTED_OSVER_CPU_MSG)"
    ${Else}
      strCpy $R7 "$(WARN_MIN_SUPPORTED_OSVER_MSG)"
    ${EndIf}
    MessageBox MB_OKCANCEL|MB_ICONSTOP "$R7" IDCANCEL +2
    ExecShell "open" "${URLSystemRequirements}"
    Quit
  ${EndUnless}

  ; SSE2 CPU support
  ${If} "$R7" == "0"
    MessageBox MB_OKCANCEL|MB_ICONSTOP "$(WARN_MIN_SUPPORTED_CPU_MSG)" IDCANCEL +2
    ExecShell "open" "${URLSystemRequirements}"
    Quit
  ${EndIf}

!ifdef HAVE_64BIT_BUILD
  ${Unless} ${RunningX64}
    MessageBox MB_OKCANCEL|MB_ICONSTOP "$(WARN_MIN_SUPPORTED_OSVER_MSG)" IDCANCEL +2
    ExecShell "open" "${URLSystemRequirements}"
    Quit
  ${EndUnless}
  SetRegView 64
!endif

  ${InstallOnInitCommon} "$(WARN_MIN_SUPPORTED_OSVER_CPU_MSG)"

  !insertmacro InitInstallOptionsFile "options.ini"
  !insertmacro InitInstallOptionsFile "shortcuts.ini"
  !insertmacro InitInstallOptionsFile "components.ini"
  !insertmacro InitInstallOptionsFile "extensions.ini"
  !insertmacro InitInstallOptionsFile "summary.ini"

  WriteINIStr "$PLUGINSDIR\options.ini" "Settings" NumFields "5"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Text   "$(OPTIONS_SUMMARY)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Top    "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 1" Bottom "10"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Text   "$(OPTION_STANDARD_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Top    "25"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Bottom "35"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Type   "RadioButton"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Text   "$(OPTION_CUSTOM_RADIO)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Left   "0"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Top    "55"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" Bottom "65"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 3" State  "0"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Text   "$(OPTION_STANDARD_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Top    "37"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 4" Bottom "57"

  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Type   "label"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Text   "$(OPTION_CUSTOM_DESC)"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Left   "15"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Right  "-1"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Top    "67"
  WriteINIStr "$PLUGINSDIR\options.ini" "Field 5" Bottom "87"

  ; Setup the shortcuts.ini file for the Custom Shortcuts Page
  ; Don't offer to install the quick launch shortcut on Windows 7
  ${If} ${AtLeastWin7}
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Settings" NumFields "3"
  ${Else}
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Settings" NumFields "4"
  ${EndIf}

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Text   "$(CREATE_ICONS_DESC)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 1" Bottom "15"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Text   "$(ICONS_DESKTOP)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Left   "0"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Top    "20"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Bottom "30"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 2" Flags  "GROUP"

  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Text   "$(ICONS_STARTMENU)"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Left   "0"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Right  "-1"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Top    "40"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" Bottom "50"
  WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 3" State  "1"

  ; Don't offer to install the quick launch shortcut on Windows 7
  ${Unless} ${AtLeastWin7}
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Type   "checkbox"
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Text   "$(ICONS_QUICKLAUNCH)"
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Left   "0"
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Right  "-1"
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Top    "60"
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" Bottom "70"
    WriteINIStr "$PLUGINSDIR\shortcuts.ini" "Field 4" State  "1"
  ${EndUnless}

  ; Setup the components.ini file for the Components Page
  WriteINIStr "$PLUGINSDIR\components.ini" "Settings" NumFields "2"

  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Type   "label"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Text   "$(OPTIONAL_COMPONENTS_DESC)"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 1" Bottom "25"

  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Text   "$(MAINTENANCE_SERVICE_CHECKBOX_DESC)"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Left   "0"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Right  "-1"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Top    "27"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Bottom "37"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" State  "1"
  WriteINIStr "$PLUGINSDIR\components.ini" "Field 2" Flags  "GROUP"

  ; Setup the extensions.ini file for the Custom Extensions Page
  StrCpy $R9 0
  StrCpy $R8 ${BST_CHECKED}

  ${If} ${FileExists} "$EXEDIR\core\distribution\setup.ini"
    ${Do}
      IntOp $R7 $R9 + 2

      ClearErrors
      ReadINIStr $R6 "$EXEDIR\core\distribution\setup.ini" \
        "OptionalExtensions" "extension.$R9.name.${AB_CD}"
      ${If} ${Errors}
        ClearErrors
        ReadINIStr $R6 "$EXEDIR\core\distribution\setup.ini" \
          "OptionalExtensions" "extension.$R9.name"
      ${EndIf}

      ${If} ${Errors}
        ${ExitDo}
      ${EndIf}

      ; Each row moves down by 13 DLUs
      IntOp $R2 $R9 * 13
      IntOp $R2 $R2 + 21
      IntOp $R1 $R2 + 10

      ClearErrors
      ReadINIStr $R0 "$EXEDIR\core\distribution\setup.ini" \
        "OptionalExtensions" "extension.$R9.checked"
      ${If} ${Errors}
        StrCpy $R0 ${BST_CHECKED}
      ${ElseIf} $R0 == "0"
        StrCpy $R8 ${BST_UNCHECKED}
      ${EndIf}

      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Type   "checkbox"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Text   "$R6"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Left   "11"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Right  "-1"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Top    "$R2"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Bottom "$R1"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" State  "$R0"
      WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R7" Flags  "NOTIFY"

      IntOp $R9 $R9 + 1
    ${Loop}
  ${EndIf}

  IntOp $R9 $R9 + 2

  WriteINIStr "$PLUGINSDIR\extensions.ini" "Settings" NumFields "$R9"

  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Type   "checkbox"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Text   "$(OPTIONAL_EXTENSIONS_CHECKBOX_DESC)"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Left   "0"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Right  "-1"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Top    "5"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Bottom "15"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" State  "$R8"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field 1" Flags  "GROUP|NOTIFY"

  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R9" Type   "label"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R9" Text   "$(OPTIONAL_EXTENSIONS_DESC)"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R9" Left   "0"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R9" Right  "-1"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R9" Top    "-23"
  WriteINIStr "$PLUGINSDIR\extensions.ini" "Field $R9" Bottom "-5"

  ; There must always be a core directory.
  ${GetSize} "$EXEDIR\core\" "/S=0K" $R5 $R7 $R8
  SectionSetSize ${APP_IDX} $R5

  ; Initialize $hHeaderBitmap to prevent redundant changing of the bitmap if
  ; the user clicks the back button
  StrCpy $hHeaderBitmap ""
FunctionEnd

Function .onGUIEnd
  ${OnEndCommon}
FunctionEnd
