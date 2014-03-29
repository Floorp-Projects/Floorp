# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

; The registration ID of the COM server which is used for choosing wether
; to launch the Win8 metro browser or desktop browser.
!define DELEGATE_EXECUTE_HANDLER_ID {5100FEC1-212B-4BF5-9BF8-3E650FD794A3}
;
; Defines for adjust token privs and for enumerating keys
!ifndef TOKEN_QUERY
  !define TOKEN_QUERY             0x0008
!endif
!ifndef TOKEN_ADJUST_PRIVILEGES
  !define TOKEN_ADJUST_PRIVILEGES 0x0020
!endif
!ifndef SE_RESTORE_NAME
  !define SE_RESTORE_NAME         SeRestorePrivilege
!endif
!ifndef SE_PRIVILEGE_ENABLED
  !define SE_PRIVILEGE_ENABLED    0x00000002
!endif
!ifndef HKEY_USERS
  !define HKEY_USERS              0x80000003
!endif

; Does metro registration for the command execute handler
Function RegisterCEH
!ifdef MOZ_METRO
  ${If} ${AtLeastWin8}
    ${CleanupMetroBrowserHandlerValues} ${DELEGATE_EXECUTE_HANDLER_ID} \
                                        "FirefoxURL" \
                                        "FirefoxHTML"
    ${AddMetroBrowserHandlerValues} ${DELEGATE_EXECUTE_HANDLER_ID} \
                                    "$INSTDIR\CommandExecuteHandler.exe" \
                                    $AppUserModelID \
                                    "FirefoxURL" \
                                    "FirefoxHTML"
  ${EndIf}
!endif
FunctionEnd

; If we're in Win8 make sure we have a start menu shortcut and that it has
; the correct AppuserModelID so that the Metro browser has a Metro tile.
Function RegisterStartMenuTile
!ifdef MOZ_METRO
  ${If} ${AtLeastWin8}
    CreateShortCut "$SMPROGRAMS\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandFullName}.lnk" \
                                             "$INSTDIR"
      ${If} "$AppUserModelID" != ""
        ApplicationID::Set "$SMPROGRAMS\${BrandFullName}.lnk" "$AppUserModelID" "true"
      ${EndIf}
    ${EndIf}
  ${EndIf}
!endif
FunctionEnd

!macro PostUpdate

  ; PostUpdate is called from both session 0 and from the user session
  ; for service updates, make sure that we only register with the user session
  ; Otherwise ApplicationID::Set can fail intermittently with a file in use error.
  System::Call "kernel32::GetCurrentProcessId() i.r0"
  System::Call "kernel32::ProcessIdToSessionId(i $0, *i ${NSIS_MAX_STRLEN} r9)"

  ; Determine if we're the protected UserChoice default or not. If so fix the
  ; start menu tile.  In case there are 2 Firefox installations, we only do
  ; this if the application being updated is the default.
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\Shell\Associations\UrlAssociations\http\UserChoice" "ProgId"
  ${If} $0 == "FirefoxURL"
  ${AndIf} $9 != 0 ; We're not running in session 0
    ReadRegStr $0 HKCU "Software\Classes\FirefoxURL\shell\open\command" ""
    ${GetPathFromString} "$0" $0
    ${GetParent} "$0" $0
    ${If} ${FileExists} "$0"
      ${GetLongPath} "$0" $0
    ${EndIf}
    ${If} "$0" == "$INSTDIR"
      ; Win8 specific registration
      Call RegisterStartMenuTile
    ${EndIf}
  ${EndIf}

  ${CreateShortcutsLog}

  ; Remove registry entries for non-existent apps and for apps that point to our
  ; install location in the Software\Mozilla key and uninstall registry entries
  ; that point to our install location for both HKCU and HKLM.
  SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
  ${RegCleanMain} "Software\Mozilla"
  ${RegCleanUninstall}
  ${UpdateProtocolHandlers}

  ; setup the application model id registration value
  ${InitHashAppModelId} "$INSTDIR" "Software\Mozilla\${AppName}\TaskBarIDs"

  ; Win7 taskbar and start menu link maintenance
  Call FixShortcutAppModelIDs

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
    ${FixShellIconHandler} "HKLM"
    ${SetAppLSPCategories} ${LSP_CATEGORIES}

    ; Win7 taskbar and start menu link maintenance
    Call FixShortcutAppModelIDs

    ; Only update the Clients\StartMenuInternet registry key values in HKLM if
    ; they don't exist or this installation is the same as the one set in those
    ; keys.
    ${StrFilter} "${FileMainEXE}" "+" "" "" $1
    ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$1\DefaultIcon" ""
    ${GetPathFromString} "$0" $0
    ${GetParent} "$0" $0
    ${If} ${FileExists} "$0"
      ${GetLongPath} "$0" $0
    ${EndIf}
    ${If} "$0" == "$INSTDIR"
      ${SetStartMenuInternet} "HKLM"
    ${EndIf}

    ; Only update the Clients\StartMenuInternet registry key values in HKCU if
    ; they don't exist or this installation is the same as the one set in those
    ; keys.  This is only done in Windows 8 to avoid a UAC prompt.
    ${If} ${AtLeastWin8}
      ReadRegStr $0 HKCU "Software\Clients\StartMenuInternet\$1\DefaultIcon" ""
      ${GetPathFromString} "$0" $0
      ${GetParent} "$0" $0
      ${If} ${FileExists} "$0"
        ${GetLongPath} "$0" $0
      ${EndIf}
      ${If} "$0" == "$INSTDIR"
        ${SetStartMenuInternet} "HKCU"
      ${EndIf}
    ${EndIf}

    ReadRegStr $0 HKLM "Software\mozilla.org\Mozilla" "CurrentVersion"
    ${If} "$0" != "${GREVersion}"
      WriteRegStr HKLM "Software\mozilla.org\Mozilla" "CurrentVersion" "${GREVersion}"
    ${EndIf}
  ${EndIf}

  ; Migrate the application's Start Menu directory to a single shortcut in the
  ; root of the Start Menu Programs directory.
  ${MigrateStartMenuShortcut}

  ; Adds a pinned Task Bar shortcut (see MigrateTaskBarShortcut for details).
  ${MigrateTaskBarShortcut}

  ${RemoveDeprecatedKeys}

  ${SetAppKeys}
  ${FixClassKeys}
  ${SetUninstallKeys}

  ; Remove files that may be left behind by the application in the
  ; VirtualStore directory.
  ${CleanVirtualStore}

  ${RemoveDeprecatedFiles}

  ; Fix the distribution.ini file if applicable
  ${FixDistributionsINI}

  RmDir /r /REBOOTOK "$INSTDIR\${TO_BE_DELETED}"

!ifdef MOZ_MAINTENANCE_SERVICE
  Call IsUserAdmin
  Pop $R0
  ${If} $R0 == "true"
  ; Only proceed if we have HKLM write access
  ${AndIf} $TmpVal == "HKLM"
  ; On Windows 2000 we do not install the maintenance service.
  ${AndIf} ${AtLeastWinXP}
    ; Add the registry keys for allowed certificates.
    ${AddMaintCertKeys}

    ; We check to see if the maintenance service install was already attempted.
    ; Since the Maintenance service can be installed either x86 or x64,
    ; always use the 64-bit registry for checking if an attempt was made.
    ${If} ${RunningX64}
      SetRegView 64
    ${EndIf}
    ReadRegDWORD $5 HKLM "Software\Mozilla\MaintenanceService" "Attempted"
    ClearErrors
    ${If} ${RunningX64}
      SetRegView lastused
    ${EndIf}

    ; If the maintenance service is already installed, do nothing.
    ; The maintenance service will launch:
    ; maintenanceservice_installer.exe /Upgrade to upgrade the maintenance
    ; service if necessary.   If the update was done from updater.exe without
    ; the service (i.e. service is failing), updater.exe will do the update of
    ; the service.  The reasons we do not do it here is because we don't want
    ; to have to prompt for limited user accounts when the service isn't used
    ; and we currently call the PostUpdate twice, once for the user and once
    ; for the SYSTEM account.  Also, this would stop the maintenance service
    ; and we need a return result back to the service when run that way.
    ${If} $5 == ""
      ; An install of maintenance service was never attempted.
      ; We know we are an Admin and that we have write access into HKLM
      ; based on the above checks, so attempt to just run the EXE.
      ; In the worst case, in case there is some edge case with the
      ; IsAdmin check and the permissions check, the maintenance service
      ; will just fail to be attempted to be installed.
      nsExec::Exec "$\"$INSTDIR\maintenanceservice_installer.exe$\""
    ${EndIf}
  ${EndIf}
!endif

; Register the DEH
!ifdef MOZ_METRO
  ${If} ${AtLeastWin8}
  ${AndIf} $9 != 0 ; We're not running in session 0
    ; If RegisterCEH is called too close to changing the shortcut AppUserModelID
    ; and if the tile image is not already in cache.  Then Windows won't refresh
    ; the tile image on the start screen.  So wait before calling RegisterCEH.
    ; We only need to do this when the DEH doesn't already exist.
    ReadRegStr $0 HKCU "Software\Classes\FirefoxURL\shell\open\command" "DelegateExecute"
    ${If} $0 != ${DELEGATE_EXECUTE_HANDLER_ID}
      Sleep 3000
    ${EndIf}
    Call RegisterCEH
  ${EndIf}
!else
  ; The metro browser is not enabled by the mozconfig.
  ${If} ${AtLeastWin8}
    ${RemoveDEHRegistration} ${DELEGATE_EXECUTE_HANDLER_ID} \
                             $AppUserModelID \
                             "FirefoxURL" \
                             "FirefoxHTML"
  ${EndIf}
!endif
!macroend
!define PostUpdate "!insertmacro PostUpdate"

!macro SetAsDefaultAppGlobal
  ${RemoveDeprecatedKeys} ; Does not use SHCTX

  SetShellVarContext all      ; Set SHCTX to all users (e.g. HKLM)
  ${SetHandlers} ; Uses SHCTX
  ${SetStartMenuInternet} "HKLM"
  ${FixShellIconHandler} "HKLM"
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
  ${If} ${AtLeastWin8}
    WriteRegDWORD HKCU "$R1" "IconsVisible" 0
  ${EndIf}

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
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$DESKTOP\${BrandFullName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  SetShellVarContext all  ; Set $SMPROGRAMS to All Users
  ${Unless} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
    SetShellVarContext current  ; Set $SMPROGRAMS to the current user's Start
                                ; Menu Programs directory
  ${EndUnless}

  ${If} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$SMPROGRAMS\${BrandFullName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$SMPROGRAMS\${BrandFullName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$SMPROGRAMS\${BrandFullName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    ShellLink::GetShortCutArgs "$QUICKLAUNCH\${BrandFullName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$QUICKLAUNCH\${BrandFullName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
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
  ${If} ${AtLeastWin8}
    WriteRegDWORD HKCU "$R1" "IconsVisible" 1
  ${EndIf}

  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
    CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR"
      ${If} ${AtLeastWin7}
      ${AndIf} "$AppUserModelID" != ""
        ApplicationID::Set "$DESKTOP\${BrandFullName}.lnk" "$AppUserModelID" "true"
      ${EndIf}
    ${Else}
      SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
      ${Unless} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
        CreateShortCut "$DESKTOP\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}"
        ${If} ${FileExists} "$DESKTOP\${BrandFullName}.lnk"
          ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandFullName}.lnk" \
                                                 "$INSTDIR"
          ${If} ${AtLeastWin7}
          ${AndIf} "$AppUserModelID" != ""
            ApplicationID::Set "$DESKTOP\${BrandFullName}.lnk" "$AppUserModelID" "true"
          ${EndIf}
        ${EndIf}
      ${EndUnless}
    ${EndIf}
  ${EndUnless}

  SetShellVarContext all  ; Set $SMPROGRAMS to All Users
  ${Unless} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
    CreateShortCut "$SMPROGRAMS\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandFullName}.lnk" \
                                             "$INSTDIR"
      ${If} ${AtLeastWin7}
      ${AndIf} "$AppUserModelID" != ""
        ApplicationID::Set "$SMPROGRAMS\${BrandFullName}.lnk" "$AppUserModelID" "true"
      ${EndIf}
    ${Else}
      SetShellVarContext current  ; Set $SMPROGRAMS to the current user's Start
                                  ; Menu Programs directory
      ${Unless} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
        CreateShortCut "$SMPROGRAMS\${BrandFullName}.lnk" "$INSTDIR\${FileMainEXE}"
        ${If} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
          ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandFullName}.lnk" \
                                                 "$INSTDIR"
          ${If} ${AtLeastWin7}
          ${AndIf} "$AppUserModelID" != ""
            ApplicationID::Set "$SMPROGRAMS\${BrandFullName}.lnk" "$AppUserModelID" "true"
          ${EndIf}
        ${EndIf}
      ${EndUnless}
    ${EndIf}
  ${EndUnless}

  ; Windows 7 doesn't use the QuickLaunch directory
  ${Unless} ${AtLeastWin7}
  ${AndUnless} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
    CreateShortCut "$QUICKLAUNCH\${BrandFullName}.lnk" \
                   "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$QUICKLAUNCH\${BrandFullName}.lnk" \
                                             "$INSTDIR"
    ${EndIf}
  ${EndUnless}
!macroend
!define ShowShortcuts "!insertmacro ShowShortcuts"

; Adds the protocol and file handler registry entries for making Firefox the
; default handler (uses SHCTX).
!macro SetHandlers
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8

  StrCpy $0 "SOFTWARE\Classes"
  StrCpy $2 "$\"$8$\" -osint -url $\"%1$\""

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

  ; Only add webm if it's not present
  ${CheckIfRegistryKeyExists} "$0" ".webm" $7
  ${If} $7 == "false"
    WriteRegStr SHCTX "$0\.webm"  "" "FirefoxHTML"
  ${EndIf}

  ; An empty string is used for the 5th param because FirefoxHTML is not a
  ; protocol handler
  ${AddDisabledDDEHandlerValues} "FirefoxHTML" "$2" "$8,1" \
                                 "${AppRegName} HTML Document" ""

  ${AddDisabledDDEHandlerValues} "FirefoxURL" "$2" "$8,1" "${AppRegName} URL" \
                                 "true"
  Call RegisterCEH

  ; An empty string is used for the 4th & 5th params because the following
  ; protocol handlers already have a display name and the additional keys
  ; required for a protocol handler.
  ${AddDisabledDDEHandlerValues} "ftp" "$2" "$8,1" "" ""
  ${AddDisabledDDEHandlerValues} "http" "$2" "$8,1" "" ""
  ${AddDisabledDDEHandlerValues} "https" "$2" "$8,1" "" ""
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
; In Windows 8 this changes slightly, you can store StartMenuInternet entries in
; HKCU.  The icon in start menu for StartMenuInternet is deprecated as of Win7,
; but the subkeys are what's important.  Control panel default programs looks
; for them only in HKLM pre win8.
;
; Note: we might be able to get away with using the full path to the
; application executable for the key name in order to support multiple
; installations.
!macro SetStartMenuInternet RegKey
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  ${GetLongPath} "$INSTDIR\uninstall\helper.exe" $7

  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9

  StrCpy $0 "Software\Clients\StartMenuInternet\$R9"

  WriteRegStr ${RegKey} "$0" "" "${BrandFullName}"

  WriteRegStr ${RegKey} "$0\DefaultIcon" "" "$8,0"

  ; The Reinstall Command is defined at
  ; http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_adv/registeringapps.asp
  WriteRegStr ${RegKey} "$0\InstallInfo" "HideIconsCommand" "$\"$7$\" /HideShortcuts"
  WriteRegStr ${RegKey} "$0\InstallInfo" "ShowIconsCommand" "$\"$7$\" /ShowShortcuts"
  WriteRegStr ${RegKey} "$0\InstallInfo" "ReinstallCommand" "$\"$7$\" /SetAsDefaultAppGlobal"

  ClearErrors
  ReadRegDWORD $1 ${RegKey} "$0\InstallInfo" "IconsVisible"
  ; If the IconsVisible name value pair doesn't exist add it otherwise the
  ; application won't be displayed in Set Program Access and Defaults.
  ${If} ${Errors}
    ${If} ${FileExists} "$QUICKLAUNCH\${BrandFullName}.lnk"
      WriteRegDWORD ${RegKey} "$0\InstallInfo" "IconsVisible" 1
    ${Else}
      WriteRegDWORD ${RegKey} "$0\InstallInfo" "IconsVisible" 0
    ${EndIf}
  ${EndIf}

  WriteRegStr ${RegKey} "$0\shell\open\command" "" "$\"$8$\""

  WriteRegStr ${RegKey} "$0\shell\properties" "" "$(CONTEXT_OPTIONS)"
  WriteRegStr ${RegKey} "$0\shell\properties\command" "" "$\"$8$\" -preferences"

  WriteRegStr ${RegKey} "$0\shell\safemode" "" "$(CONTEXT_SAFE_MODE)"
  WriteRegStr ${RegKey} "$0\shell\safemode\command" "" "$\"$8$\" -safe-mode"

  ; Vista Capabilities registry keys
  WriteRegStr ${RegKey} "$0\Capabilities" "ApplicationDescription" "$(REG_APP_DESC)"
  WriteRegStr ${RegKey} "$0\Capabilities" "ApplicationIcon" "$8,0"
  WriteRegStr ${RegKey} "$0\Capabilities" "ApplicationName" "${BrandShortName}"

  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".htm"   "FirefoxHTML"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".html"  "FirefoxHTML"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".shtml" "FirefoxHTML"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".xht"   "FirefoxHTML"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".xhtml" "FirefoxHTML"

  WriteRegStr ${RegKey} "$0\Capabilities\StartMenu" "StartMenuInternet" "$R9"

  WriteRegStr ${RegKey} "$0\Capabilities\URLAssociations" "ftp"    "FirefoxURL"
  WriteRegStr ${RegKey} "$0\Capabilities\URLAssociations" "http"   "FirefoxURL"
  WriteRegStr ${RegKey} "$0\Capabilities\URLAssociations" "https"  "FirefoxURL"

  ; Vista Registered Application
  WriteRegStr ${RegKey} "Software\RegisteredApplications" "${AppRegName}" "$0\Capabilities"
!macroend
!define SetStartMenuInternet "!insertmacro SetStartMenuInternet"

; The IconHandler reference for FirefoxHTML can end up in an inconsistent state
; due to changes not being detected by the IconHandler for side by side
; installs (see bug 268512). The symptoms can be either an incorrect icon or no
; icon being displayed for files associated with Firefox (does not use SHCTX).
!macro FixShellIconHandler RegKey
  ClearErrors
  ReadRegStr $1 ${RegKey} "Software\Classes\FirefoxHTML\ShellEx\IconHandler" ""
  ${Unless} ${Errors}
    ReadRegStr $1 ${RegKey} "Software\Classes\FirefoxHTML\DefaultIcon" ""
    ${GetLongPath} "$INSTDIR\${FileMainEXE}" $2
    ${If} "$1" != "$2,1"
      WriteRegStr ${RegKey} "Software\Classes\FirefoxHTML\DefaultIcon" "" "$2,1"
    ${EndIf}
  ${EndUnless}
!macroend
!define FixShellIconHandler "!insertmacro FixShellIconHandler"

; Add Software\Mozilla\ registry entries (uses SHCTX).
!macro SetAppKeys
  ; Check if this is an ESR release and if so add registry values so it is
  ; possible to determine that this is an ESR install (bug 726781).
  ClearErrors
  ${WordFind} "${UpdateChannel}" "esr" "E#" $3
  ${If} ${Errors}
    StrCpy $3 ""
  ${Else}
    StrCpy $3 " ESR"
  ${EndIf}

  ${GetLongPath} "$INSTDIR" $8
  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion}$3 (${AB_CD})\Main"
  ${WriteRegStr2} $TmpVal "$0" "Install Directory" "$8" 0
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$8\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion}$3 (${AB_CD})\Uninstall"
  ${WriteRegStr2} $TmpVal "$0" "Description" "${BrandFullNameInternal} ${AppVersion}$3 (${ARCH} ${AB_CD})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion}$3 (${AB_CD})"
  ${WriteRegStr2} $TmpVal  "$0" "" "${AppVersion}$3 (${AB_CD})" 0
  ${If} "$3" == ""
    DeleteRegValue SHCTX "$0" "ESR"
  ${Else}
    ${WriteRegDWORD2} $TmpVal "$0" "ESR" 1 0
  ${EndIf}

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}$3\bin"
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$8\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}$3\extensions"
  ${WriteRegStr2} $TmpVal "$0" "Components" "$8\components" 0
  ${WriteRegStr2} $TmpVal "$0" "Plugins" "$8\plugins" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal} ${AppVersion}$3"
  ${WriteRegStr2} $TmpVal "$0" "GeckoVer" "${GREVersion}" 0
  ${If} "$3" == ""
    DeleteRegValue SHCTX "$0" "ESR"
  ${Else}
    ${WriteRegDWORD2} $TmpVal "$0" "ESR" 1 0
  ${EndIf}

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}$3"
  ${WriteRegStr2} $TmpVal "$0" "" "${GREVersion}" 0
  ${WriteRegStr2} $TmpVal "$0" "CurrentVersion" "${AppVersion}$3 (${AB_CD})" 0
!macroend
!define SetAppKeys "!insertmacro SetAppKeys"

; Add uninstall registry entries. This macro tests for write access to determine
; if the uninstall keys should be added to HKLM or HKCU.
!macro SetUninstallKeys
  ; Check if this is an ESR release and if so add registry values so it is
  ; possible to determine that this is an ESR install (bug 726781).
  ClearErrors
  ${WordFind} "${UpdateChannel}" "esr" "E#" $3
  ${If} ${Errors}
    StrCpy $3 ""
  ${Else}
    StrCpy $3 " ESR"
  ${EndIf}

  StrCpy $0 "Software\Microsoft\Windows\CurrentVersion\Uninstall\${BrandFullNameInternal} ${AppVersion}$3 (${ARCH} ${AB_CD})"

  StrCpy $2 ""
  ClearErrors
  WriteRegStr HKLM "$0" "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    ; If the uninstall keys already exist in HKLM don't create them in HKCU
    ClearErrors
    ReadRegStr $2 "HKLM" $0 "DisplayName"
    ${If} $2 == ""
      ; Otherwise we don't have any keys for this product in HKLM so proceeed
      ; to create them in HKCU.  Better handling for this will be done in:
      ; Bug 711044 - Better handling for 2 uninstall icons
      StrCpy $1 "HKCU"
      SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
    ${EndIf}
    ClearErrors
  ${Else}
    StrCpy $1 "HKLM"
    SetShellVarContext all     ; Set SHCTX to all users (e.g. HKLM)
    DeleteRegValue HKLM "$0" "${BrandShortName}InstallerTest"
  ${EndIf}

  ${If} $2 == ""
    ${GetLongPath} "$INSTDIR" $8

    ; Write the uninstall registry keys
    ${WriteRegStr2} $1 "$0" "Comments" "${BrandFullNameInternal} ${AppVersion}$3 (${ARCH} ${AB_CD})" 0
    ${WriteRegStr2} $1 "$0" "DisplayIcon" "$8\${FileMainEXE},0" 0
    ${WriteRegStr2} $1 "$0" "DisplayName" "${BrandFullNameInternal} ${AppVersion}$3 (${ARCH} ${AB_CD})" 0
    ${WriteRegStr2} $1 "$0" "DisplayVersion" "${AppVersion}" 0
    ${WriteRegStr2} $1 "$0" "HelpLink" "${HelpLink}" 0
    ${WriteRegStr2} $1 "$0" "InstallLocation" "$8" 0
    ${WriteRegStr2} $1 "$0" "Publisher" "Mozilla" 0
    ${WriteRegStr2} $1 "$0" "UninstallString" "$\"$8\uninstall\helper.exe$\"" 0
    DeleteRegValue SHCTX "$0" "URLInfoAbout"
; Don't add URLUpdateInfo which is the release notes url except for the release
; and esr channels since nightly, aurora, and beta do not have release notes.
; Note: URLUpdateInfo is only defined in the official branding.nsi.
!ifdef URLUpdateInfo
!ifndef BETA_UPDATE_CHANNEL
    ${WriteRegStr2} $1 "$0" "URLUpdateInfo" "${URLUpdateInfo}" 0
!endif
!endif
    ${WriteRegStr2} $1 "$0" "URLInfoAbout" "${URLInfoAbout}" 0
    ${WriteRegDWORD2} $1 "$0" "NoModify" 1 0
    ${WriteRegDWORD2} $1 "$0" "NoRepair" 1 0

    ${GetSize} "$8" "/S=0K" $R2 $R3 $R4
    ${WriteRegDWORD2} $1 "$0" "EstimatedSize" $R2 0

    ${If} "$TmpVal" == "HKLM"
      SetShellVarContext all     ; Set SHCTX to all users (e.g. HKLM)
    ${Else}
      SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)
    ${EndIf}
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
  StrCpy $2 "$\"$8$\" -osint -url $\"%1$\""

  ; Only set the file and protocol handlers if the existing one under HKCR is
  ; for this install location.

  ${IsHandlerForInstallDir} "FirefoxHTML" $R9
  ${If} "$R9" == "true"
    ; An empty string is used for the 5th param because FirefoxHTML is not a
    ; protocol handler.
    ${AddDisabledDDEHandlerValues} "FirefoxHTML" "$2" "$8,1" \
                                   "${AppRegName} HTML Document" ""
  ${EndIf}

  ${IsHandlerForInstallDir} "FirefoxURL" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "FirefoxURL" "$2" "$8,1" \
                                   "${AppRegName} URL" "true"
  ${EndIf}

  ; An empty string is used for the 4th & 5th params because the following
  ; protocol handlers already have a display name and the additional keys
  ; required for a protocol handler.
  ${IsHandlerForInstallDir} "ftp" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "ftp" "$2" "$8,1" "" ""
  ${EndIf}

  ${IsHandlerForInstallDir} "http" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "http" "$2" "$8,1" "" ""
  ${EndIf}

  ${IsHandlerForInstallDir} "https" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "https" "$2" "$8,1" "" ""
  ${EndIf}
!macroend
!define UpdateProtocolHandlers "!insertmacro UpdateProtocolHandlers"

!ifdef MOZ_MAINTENANCE_SERVICE
; Adds maintenance service certificate keys for the install dir.
; For the cert to work, it must also be signed by a trusted cert for the user.
!macro AddMaintCertKeys
  Push $R0
  ; Allow main Mozilla cert information for updates
  ; This call will push the needed key on the stack
  ServicesHelper::PathToUniqueRegistryPath "$INSTDIR"
  Pop $R0
  ${If} $R0 != ""
    ; More than one certificate can be specified in a different subfolder
    ; for example: $R0\1, but each individual binary can be signed
    ; with at most one certificate.  A fallback certificate can only be used
    ; if the binary is replaced with a different certificate.
    ; We always use the 64bit registry for certs.
    ${If} ${RunningX64}
      SetRegView 64
    ${EndIf}
    DeleteRegKey HKLM "$R0"
    WriteRegStr HKLM "$R0" "prefetchProcessName" "FIREFOX"
    WriteRegStr HKLM "$R0\0" "name" "${CERTIFICATE_NAME}"
    WriteRegStr HKLM "$R0\0" "issuer" "${CERTIFICATE_ISSUER}"
    ${If} ${RunningX64}
      SetRegView lastused
    ${EndIf}
    ClearErrors
  ${EndIf}
  ; Restore the previously used value back
  Pop $R0
!macroend
!define AddMaintCertKeys "!insertmacro AddMaintCertKeys"
!endif

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

; Resets Win8+ specific toast keys Windows sets. We call this on a
; fresh install and on uninstall.
!macro ResetWin8PromptKeys KEY PREFIX
  ${If} ${AtLeastWin8}
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxHTML_.htm"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxHTML_.html"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxHTML_.xht"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxHTML_.xhtml"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxHTML_.shtml"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxURL_ftp"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxURL_http"
    DeleteRegValue ${KEY} "${PREFIX}Software\Microsoft\Windows\CurrentVersion\ApplicationAssociationToasts" "FirefoxURL_https"
  ${EndIf}
!macroend
!define ResetWin8PromptKeys "!insertmacro ResetWin8PromptKeys"

!ifdef MOZ_METRO
; Resets Win8+ Metro specific splash screen info. Relies
; on AppUserModelID.
!macro ResetWin8MetroSplash
  ${If} ${AtLeastWin8}
  ${AndIf} "$AppUserModelID" != ""
    DeleteRegKey HKCR "Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\SystemAppData\DefaultBrowser_NOPUBLISHERID\SplashScreen\DefaultBrowser_NOPUBLISHERID!$AppUserModelID"
  ${EndIf}
!macroend
!define ResetWin8MetroSplash "!insertmacro ResetWin8MetroSplash"
!endif

; Adds SE_RESTORE_NAME privs
!macro AcquireSERestoreName
  StrCpy $R1 0

  System::Call "kernel32::GetCurrentProcess() i .R0"
  System::Call "advapi32::OpenProcessToken(i R0, i ${TOKEN_QUERY}|${TOKEN_ADJUST_PRIVILEGES}, \
                                          *i R1R1) i .R0"
  ${If} $R0 != 0
    System::Call "advapi32::LookupPrivilegeValue(t n, t '${SE_RESTORE_NAME}', *l .R2) i .R0"
    ${If} $R0 != 0
      System::Call "*(i 1, l R2, i ${SE_PRIVILEGE_ENABLED}) i .R0"
      System::Call "advapi32::AdjustTokenPrivileges(i R1, i 0, i R0, i 0, i 0, i 0)"
      System::Free $R0
    ${EndIf}
    System::Call "kernel32::CloseHandle(i R1)"
  ${EndIf}
!macroend
!define AcquireSERestoreName "!insertmacro AcquireSERestoreName"
!define un.AcquireSERestoreName "!insertmacro AcquireSERestoreName"

; Mounts all user ntuser.dat files into the registry as a subkey of HKU
!macro MountRegistryIntoHKU
  ; $0 is used as an index for HKEY_USERS enumeration
  StrCpy $0 0
  ${Do}
    EnumRegKey $1 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    ${If} $1 == ""
      ${Break}
    ${EndIf}
    ReadRegStr $2 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath"
    System::Call "advapi32::RegLoadKey(i ${HKEY_USERS}, t 'User-$0', t '$2\ntuser.dat')"
    System::Call "advapi32::RegLoadKey(i ${HKEY_USERS}, t 'User-$0_Classes', t '$2\AppData\Local\Microsoft\Windows\UsrClass.dat')"
    IntOp $0 $0 + 1
  ${Loop}
!macroend
!define MountRegistryIntoHKU "!insertmacro MountRegistryIntoHKU"
!define un.MountRegistryIntoHKU "!insertmacro MountRegistryIntoHKU"
;
; Unmounts all user ntuser.dat files into the registry as a subkey of HKU
!macro UnmountRegistryIntoHKU
  ; $0 is used as an index for HKEY_USERS enumeration
  StrCpy $0 0
  ${Do}
    EnumRegKey $1 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList" $0
    ${If} $1 == ""
      ${Break}
    ${EndIf}
    ReadRegStr $2 HKLM "SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\$1" "ProfileImagePath"
    System::Call "advapi32::RegUnLoadKey(i ${HKEY_USERS}, t 'User-$0')"
    System::Call "advapi32::RegUnLoadKey(i ${HKEY_USERS}, t 'User-$0_Classes')"
    IntOp $0 $0 + 1
  ${Loop}
!macroend
!define UnmountRegistryIntoHKU "!insertmacro UnmountRegistryIntoHKU"
!define un.UnmountRegistryIntoHKU "!insertmacro UnmountRegistryIntoHKU"

; Unconditionally removes the delegate execute handler registration used to
; launch the metro browser and misc. metro related registry values.
!macro RemoveDEHRegistration DELEGATE_EXECUTE_HANDLER_ID \
                             APP_USER_MODEL_ID \
                             PROTOCOL_ACTIVATION_ID \
                             FILE_ACTIVATION_ID
  ${AcquireSERestoreName}
  ${MountRegistryIntoHKU}

  ; $0 is used as an index for HKEY_USERS enumeration
  StrCpy $0 0

  ${Do}
    EnumRegKey $1 HKU "" $0
    ${If} $1 == ""
      ${Break}
    ${EndIf}

    ClearErrors
    ${WordFind} "$1" "_Classes" "E#" $3
    ${Unless} ${Errors}
      ; remove the app user model id root registration. We don't need this
      ; here anymore, we just use it for tray registrationdown in widget,
      ; which we read out of the mozilla keys.
      ${If} "${APP_USER_MODEL_ID}" != ""
        ; The removal of this key intermittently fails, so do the best we can in cleanup
        DeleteRegValue HKU "$1\${APP_USER_MODEL_ID}\.exe\shell\open\command" "DelegateExecute"
        DeleteRegKey HKU "$1\${APP_USER_MODEL_ID}\.exe\shell\open"
        DeleteRegKey HKU "$1\${APP_USER_MODEL_ID}\.exe\shell"
        DeleteRegKey HKU "$1\${APP_USER_MODEL_ID}\.exe"
        DeleteRegKey HKU "$1\\${APP_USER_MODEL_ID}"
      ${EndIf}
      ;
      ; Remove delegate execute handler clsid registration
      DeleteRegKey HKU "$1\CLSID\${DELEGATE_EXECUTE_HANDLER_ID}"

      ; Remove protocol and file delegate execute handler id assoc
      DeleteRegValue HKU "$1\${PROTOCOL_ACTIVATION_ID}" "AppUserModelID"
      DeleteRegValue HKU "$1\${FILE_ACTIVATION_ID}" "AppUserModelID"

      ; Remove delegate execute application registry keys
      DeleteRegKey HKU "$1\${PROTOCOL_ACTIVATION_ID}\Application"
      DeleteRegKey HKU "$1\${FILE_ACTIVATION_ID}\Application"

      ; Remove misc. shell open info
      DeleteRegValue HKU "$1\${PROTOCOL_ACTIVATION_ID}\shell\open" "CommandId"
      DeleteRegValue HKU "$1\${FILE_ACTIVATION_ID}\shell\open" "CommandId"
      DeleteRegValue HKU "$1\${PROTOCOL_ACTIVATION_ID}\shell\open\command" "DelegateExecute"
      DeleteRegValue HKU "$1\${FILE_ACTIVATION_ID}\shell\open\command" "DelegateExecute"

      ; remove metro browser splash image data
      DeleteRegKey HKU "$1\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\SystemAppData\DefaultBrowser_NOPUBLISHERID\SplashScreen\DefaultBrowser_NOPUBLISHERID!${APP_USER_MODEL_ID}"
    ${Else}
      ; misc. Metro keys
      DeleteRegKey HKU "$1\Software\Mozilla\Firefox\Metro"
      DeleteRegValue HKU "$1\Software\Mozilla\Firefox" "CEHDump"
      DeleteRegValue HKU "$1\Software\Mozilla\Firefox" "MetroD3DAvailable"
      DeleteRegValue HKU "$1\Software\Mozilla\Firefox" "MetroLastAHE"
      ${ResetWin8PromptKeys} "HKU" "$1\"
    ${EndIf}
    IntOp $0 $0 + 1
  ${Loop}
  ${UnmountRegistryIntoHKU}

  ; The removal of this key intermittently fails, so do the best we can in cleanup
  ${If} "${APP_USER_MODEL_ID}" != ""
    DeleteRegValue HKLM "Software\Classes\${APP_USER_MODEL_ID}\.exe\shell\open\command" "DelegateExecute"
    DeleteRegKey HKLM "Software\Classes\${APP_USER_MODEL_ID}\.exe\shell\open"
    DeleteRegKey HKLM "Software\Classes\${APP_USER_MODEL_ID}\.exe\shell"
    DeleteRegKey HKLM "Software\Classes\${APP_USER_MODEL_ID}\.exe"
    DeleteRegKey HKLM "Software\Classes\${APP_USER_MODEL_ID}"
  ${EndIf}

  ; Remove HKLM entries
  DeleteRegKey HKLM "Software\Classes\CLSID\${DELEGATE_EXECUTE_HANDLER_ID}"
  DeleteRegKey HKLM "Software\Classes\${PROTOCOL_ACTIVATION_ID}\Application"
  DeleteRegKey HKLM "Software\Classes\${FILE_ACTIVATION_ID}\Application"
  DeleteRegValue HKLM "Software\Classes\${PROTOCOL_ACTIVATION_ID}\shell\open\command" "DelegateExecute"
  DeleteRegValue HKLM "Software\Classes\${FILE_ACTIVATION_ID}\shell\open\command" "DelegateExecute"
  DeleteRegValue HKLM "Software\Classes\${PROTOCOL_ACTIVATION_ID}" "AppUserModelID"
  DeleteRegValue HKLM "Software\Classes\${FILE_ACTIVATION_ID}" "AppUserModelID"
  DeleteRegValue HKLM "Software\Classes\${PROTOCOL_ACTIVATION_ID}\shell\open" "CommandId"
  DeleteRegValue HKLM "Software\Classes\${FILE_ACTIVATION_ID}\shell\open" "CommandId"

  ClearErrors
!macroend

!define RemoveDEHRegistration "!insertmacro RemoveDEHRegistration"
!define un.RemoveDEHRegistration "!insertmacro RemoveDEHRegistration"

; Removes various directories and files for reasons noted below.
!macro RemoveDeprecatedFiles
  ; Remove talkback if it is present (remove after bug 386760 is fixed)
  ${If} ${FileExists} "$INSTDIR\extensions\talkback@mozilla.org"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\talkback@mozilla.org"
  ${EndIf}

  ; Remove the Java Console extension (bug 597235)
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0012-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0012-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0013-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0013-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0014-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0014-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0015-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0015-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0016-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0016-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0017-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0017-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0018-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0018-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0019-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0019-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0020-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0020-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0021-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0021-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0022-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0015-0000-0022-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0000-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0000-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0001-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0001-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0002-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0002-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0003-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0003-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0004-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0004-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0005-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0005-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0006-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0006-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0007-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0007-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0010-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0010-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0011-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0011-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0012-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0012-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0013-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0013-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0014-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0014-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0015-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0015-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0016-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0016-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0017-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0017-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0018-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0018-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0019-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0019-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0020-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0020-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0021-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0021-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0022-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0022-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0023-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0023-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0024-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0024-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0025-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0025-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0026-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0026-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0027-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0027-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0028-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0028-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0029-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0029-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0030-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0030-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0031-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0031-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0032-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0032-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0000-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0000-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0001-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0001-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0002-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0002-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0003-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0003-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0004-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0004-ABCDEFFEDCBA}"
  ${EndIf}
!macroend
!define RemoveDeprecatedFiles "!insertmacro RemoveDeprecatedFiles"

; Converts specific partner distribution.ini from ansi to utf-8 (bug 882989)
!macro FixDistributionsINI
  StrCpy $1 "$INSTDIR\distribution\distribution.ini"
  StrCpy $2 "$INSTDIR\distribution\utf8fix"
  StrCpy $0 "0" ; Default to not attempting to fix

  ; Check if the distribution.ini settings are for a partner build that needs
  ; to have its distribution.ini converted from ansi to utf-8.
  ${If} ${FileExists} "$1"
    ${Unless} ${FileExists} "$2"
      ReadINIStr $3 "$1" "Preferences" "app.distributor"
      ${If} "$3" == "yahoo"
        ReadINIStr $3 "$1" "Preferences" "app.distributor.channel"
        ${If} "$3" == "de"
        ${OrIf} "$3" == "es"
        ${OrIf} "$3" == "e1"
        ${OrIf} "$3" == "mx"
          StrCpy $0 "1"
        ${EndIf}
      ${EndIf}
      ; Create the utf8fix so this only runs once
      FileOpen $3 "$2" w
      FileClose $3
    ${EndUnless}
  ${EndIf}

  ${If} "$0" == "1"
    StrCpy $0 "0"
    ClearErrors
    ReadINIStr $3 "$1" "Global" "version"
    ${Unless} ${Errors}
      StrCpy $4 "$3" 2
      ${If} "$4" == "1."
        StrCpy $4 "$3" "" 2 ; Everything after "1."
        ${If} $4 < 23
          StrCpy $0 "1"
        ${EndIf}
      ${EndIf}
    ${EndUnless}
  ${EndIf}

  ${If} "$0" == "1"
    ClearErrors
    FileOpen $3 "$1" r
    ${If} ${Errors}
      FileClose $3
    ${Else}
      StrCpy $2 "$INSTDIR\distribution\distribution.new"
      ClearErrors
      FileOpen $4 "$2" w
      ${If} ${Errors}
        FileClose $3
        FileClose $4
      ${Else}
        StrCpy $0 "0" ; Default to not replacing the original distribution.ini
        ${Do}
          FileReadByte $3 $5
          ${If} $5 == ""
            ${Break}
          ${EndIf}
          ${If} $5 == 233 ; ansi é
            StrCpy $0 "1"
            FileWriteByte $4 195
            FileWriteByte $4 169
          ${ElseIf} $5 == 241 ; ansi ñ
            StrCpy $0 "1"
            FileWriteByte $4 195
            FileWriteByte $4 177
          ${ElseIf} $5 == 252 ; ansi ü
            StrCpy $0 "1"
            FileWriteByte $4 195
            FileWriteByte $4 188
          ${ElseIf} $5 < 128
            FileWriteByte $4 $5
          ${EndIf}
        ${Loop}
        FileClose $3
        FileClose $4
        ${If} "$0" == "1"
          ClearErrors
          Rename "$1" "$1.bak"
          ${Unless} ${Errors}
            Rename "$2" "$1"
            Delete "$1.bak"
          ${EndUnless}
        ${Else}
          Delete "$2"
        ${EndIf}
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define FixDistributionsINI "!insertmacro FixDistributionsINI"

; Adds a pinned shortcut to Task Bar on update for Windows 7 and above if this
; macro has never been called before and the application is default (see
; PinToTaskBar for more details).
; Since defaults handling is handled by Windows in Win8 and later, we always
; attempt to pin a taskbar on that OS.  If Windows sets the defaults at
; installation time, then we don't get the opportunity to run this code at
; that time.
!macro MigrateTaskBarShortcut
  ${GetShortcutsLogPath} $0
  ${If} ${FileExists} "$0"
    ClearErrors
    ReadINIStr $1 "$0" "TASKBAR" "Migrated"
    ${If} ${Errors}
      ClearErrors
      WriteIniStr "$0" "TASKBAR" "Migrated" "true"
      ${If} ${AtLeastWin7}
        ; No need to check the default on Win8 and later
        ${If} ${AtMostWin2008R2}
          ; Check if the Firefox is the http handler for this user
          SetShellVarContext current ; Set SHCTX to the current user
          ${IsHandlerForInstallDir} "http" $R9
          ${If} $TmpVal == "HKLM"
            SetShellVarContext all ; Set SHCTX to all users
          ${EndIf}
        ${EndIf}
        ${If} "$R9" == "true"
        ${OrIf} ${AtLeastWin8}
          ${PinToTaskBar}
        ${EndIf}
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define MigrateTaskBarShortcut "!insertmacro MigrateTaskBarShortcut"

; Adds a pinned Task Bar shortcut on Windows 7 if there isn't one for the main
; application executable already. Existing pinned shortcuts for the same
; application model ID must be removed first to prevent breaking the pinned
; item's lists but multiple installations with the same application model ID is
; an edgecase. If removing existing pinned shortcuts with the same application
; model ID removes a pinned pinned Start Menu shortcut this will also add a
; pinned Start Menu shortcut.
!macro PinToTaskBar
  ${If} ${AtLeastWin7}
    StrCpy $8 "false" ; Whether a shortcut had to be created
    ${IsPinnedToTaskBar} "$INSTDIR\${FileMainEXE}" $R9
    ${If} "$R9" == "false"
      ; Find an existing Start Menu shortcut or create one to use for pinning
      ${GetShortcutsLogPath} $0
      ${If} ${FileExists} "$0"
        ClearErrors
        ReadINIStr $1 "$0" "STARTMENU" "Shortcut0"
        ${Unless} ${Errors}
          SetShellVarContext all ; Set SHCTX to all users
          ${Unless} ${FileExists} "$SMPROGRAMS\$1"
            SetShellVarContext current ; Set SHCTX to the current user
            ${Unless} ${FileExists} "$SMPROGRAMS\$1"
              StrCpy $8 "true"
              CreateShortCut "$SMPROGRAMS\$1" "$INSTDIR\${FileMainEXE}"
              ${If} ${FileExists} "$SMPROGRAMS\$1"
                ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\$1" \
                                                       "$INSTDIR"
                ${If} "$AppUserModelID" != ""
                  ApplicationID::Set "$SMPROGRAMS\$1" "$AppUserModelID" "true"
                ${EndIf}
              ${EndIf}
            ${EndUnless}
          ${EndUnless}

          ${If} ${FileExists} "$SMPROGRAMS\$1"
            ; Count of Start Menu pinned shortcuts before unpinning.
            ${PinnedToStartMenuLnkCount} $R9

            ; Having multiple shortcuts pointing to different installations with
            ; the same AppUserModelID (e.g. side by side installations of the
            ; same version) will make the TaskBar shortcut's lists into an bad
            ; state where the lists are not shown. To prevent this first
            ; uninstall the pinned item.
            ApplicationID::UninstallPinnedItem "$SMPROGRAMS\$1"

            ; Count of Start Menu pinned shortcuts after unpinning.
            ${PinnedToStartMenuLnkCount} $R8

            ; If there is a change in the number of Start Menu pinned shortcuts
            ; assume that unpinning unpinned a side by side installation from
            ; the Start Menu and pin this installation to the Start Menu.
            ${Unless} $R8 == $R9
              ; Pin the shortcut to the Start Menu. 5381 is the shell32.dll
              ; resource id for the "Pin to Start Menu" string.
              InvokeShellVerb::DoIt "$SMPROGRAMS" "$1" "5381"
            ${EndUnless}

            ; Pin the shortcut to the TaskBar. 5386 is the shell32.dll resource
            ; id for the "Pin to Taskbar" string.
            InvokeShellVerb::DoIt "$SMPROGRAMS" "$1" "5386"

            ; Delete the shortcut if it was created
            ${If} "$8" == "true"
              Delete "$SMPROGRAMS\$1"
            ${EndIf}
          ${EndIf}

          ${If} $TmpVal == "HKCU"
            SetShellVarContext current ; Set SHCTX to the current user
          ${Else}
            SetShellVarContext all ; Set SHCTX to all users
          ${EndIf}
        ${EndUnless}
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define PinToTaskBar "!insertmacro PinToTaskBar"

; Adds a shortcut to the root of the Start Menu Programs directory if the
; application's Start Menu Programs directory exists with a shortcut pointing to
; this installation directory. This will also remove the old shortcuts and the
; application's Start Menu Programs directory by calling the RemoveStartMenuDir
; macro.
!macro MigrateStartMenuShortcut
  ${GetShortcutsLogPath} $0
  ${If} ${FileExists} "$0"
    ClearErrors
    ReadINIStr $5 "$0" "SMPROGRAMS" "RelativePathToDir"
    ${Unless} ${Errors}
      ClearErrors
      ReadINIStr $1 "$0" "STARTMENU" "Shortcut0"
      ${If} ${Errors}
        ; The STARTMENU ini section doesn't exist.
        ${LogStartMenuShortcut} "${BrandFullName}.lnk"
        ${GetLongPath} "$SMPROGRAMS" $2
        ${GetLongPath} "$2\$5" $1
        ${If} "$1" != ""
          ClearErrors
          ReadINIStr $3 "$0" "SMPROGRAMS" "Shortcut0"
          ${Unless} ${Errors}
            ${If} ${FileExists} "$1\$3"
              ShellLink::GetShortCutTarget "$1\$3"
              Pop $4
              ${If} "$INSTDIR\${FileMainEXE}" == "$4"
                CreateShortCut "$SMPROGRAMS\${BrandFullName}.lnk" \
                               "$INSTDIR\${FileMainEXE}"
                ${If} ${FileExists} "$SMPROGRAMS\${BrandFullName}.lnk"
                  ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandFullName}.lnk" \
                                                         "$INSTDIR"
                  ${If} ${AtLeastWin7}
                  ${AndIf} "$AppUserModelID" != ""
                    ApplicationID::Set "$SMPROGRAMS\${BrandFullName}.lnk" \
                                       "$AppUserModelID" "true"
                  ${EndIf}
                ${EndIf}
              ${EndIf}
            ${EndIf}
          ${EndUnless}
        ${EndIf}
      ${EndIf}
      ; Remove the application's Start Menu Programs directory, shortcuts, and
      ; ini section.
      ${RemoveStartMenuDir}
    ${EndUnless}
  ${EndIf}
!macroend
!define MigrateStartMenuShortcut "!insertmacro MigrateStartMenuShortcut"

; Removes the application's start menu directory along with its shortcuts if
; they exist and if they exist creates a start menu shortcut in the root of the
; start menu directory (bug 598779). If the application's start menu directory
; is not empty after removing the shortucts the directory will not be removed
; since these additional items were not created by the installer (uses SHCTX).
!macro RemoveStartMenuDir
  ${GetShortcutsLogPath} $0
  ${If} ${FileExists} "$0"
    ; Delete Start Menu Programs shortcuts, directory if it is empty, and
    ; parent directories if they are empty up to but not including the start
    ; menu directory.
    ${GetLongPath} "$SMPROGRAMS" $1
    ClearErrors
    ReadINIStr $2 "$0" "SMPROGRAMS" "RelativePathToDir"
    ${Unless} ${Errors}
      ${GetLongPath} "$1\$2" $2
      ${If} "$2" != ""
        ; Delete shortucts in the Start Menu Programs directory.
        StrCpy $3 0
        ${Do}
          ClearErrors
          ReadINIStr $4 "$0" "SMPROGRAMS" "Shortcut$3"
          ; Stop if there are no more entries
          ${If} ${Errors}
            ${ExitDo}
          ${EndIf}
          ${If} ${FileExists} "$2\$4"
            ShellLink::GetShortCutTarget "$2\$4"
            Pop $5
            ${If} "$INSTDIR\${FileMainEXE}" == "$5"
              Delete "$2\$4"
            ${EndIf}
          ${EndIf}
          IntOp $3 $3 + 1 ; Increment the counter
        ${Loop}
        ; Delete Start Menu Programs directory and parent directories
        ${Do}
          ; Stop if the current directory is the start menu directory
          ${If} "$1" == "$2"
            ${ExitDo}
          ${EndIf}
          ClearErrors
          RmDir "$2"
          ; Stop if removing the directory failed
          ${If} ${Errors}
            ${ExitDo}
          ${EndIf}
          ${GetParent} "$2" $2
        ${Loop}
      ${EndIf}
      DeleteINISec "$0" "SMPROGRAMS"
    ${EndUnless}
  ${EndIf}
!macroend
!define RemoveStartMenuDir "!insertmacro RemoveStartMenuDir"

; Creates the shortcuts log ini file with the appropriate entries if it doesn't
; already exist.
!macro CreateShortcutsLog
  ${GetShortcutsLogPath} $0
  ${Unless} ${FileExists} "$0"
    ${LogStartMenuShortcut} "${BrandFullName}.lnk"
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
!ifdef MOZ_CONTENT_SANDBOX
  Push "sandboxbroker.dll"
!endif
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
  ReadRegStr $0 HKCU "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${If} ${Errors}
  ${OrIf} ${AtMostWin2008R2}
    ClearErrors
    ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${EndIf}
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

  ${If} ${AtLeastWin8}
    ${SetStartMenuInternet} "HKCU"
    ${FixShellIconHandler} "HKCU"
    ${FixClassKeys} ; Does not use SHCTX
    Call RegisterStartMenuTile
  ${EndIf}

  ${SetHandlers}

  ${If} ${AtLeastWinVista}
    ; Only register as the handler on Vista and above if the app registry name
    ; exists under the RegisteredApplications registry key. The protocol and
    ; file handlers set previously at the user level will associate this install
    ; as the default browser.
    ClearErrors
    ReadRegStr $0 HKLM "Software\RegisteredApplications" "${AppRegName}"
    ${Unless} ${Errors}
      ; This is all protected by a user choice hash in Windows 8 so it won't
      ; help, but it also won't hurt.
      AppAssocReg::SetAppAsDefaultAll "${AppRegName}"
    ${EndUnless}
  ${EndIf}
  ${RemoveDeprecatedKeys}
  ${PinToTaskBar}
FunctionEnd

; Helper for updating the shortcut application model IDs.
Function FixShortcutAppModelIDs
  ${If} ${AtLeastWin7}
  ${AndIf} "$AppUserModelID" != ""
    ${UpdateShortcutAppModelIDs} "$INSTDIR\${FileMainEXE}" "$AppUserModelID" $0
  ${EndIf}
FunctionEnd

; The !ifdef NO_LOG prevents warnings when compiling the installer.nsi due to
; this function only being used by the uninstaller.nsi.
!ifdef NO_LOG

Function SetAsDefaultAppUser
  ; On Win8, we want to avoid having a UAC prompt since we'll already have
  ; another action for control panel default browser selection popping up
  ; to the user.  Win8 is the first OS where the start menu keys can be
  ; added into HKCU.  The call to SetAsDefaultAppUserHKCU will have already
  ; set the HKCU keys for SetStartMenuInternet.
  ${If} ${AtLeastWin8}
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

  ; Before Win8, it is only possible to set this installation of the application
  ; as the StartMenuInternet handler if it was added to the HKLM
  ; StartMenuInternet registry keys.
  ; http://support.microsoft.com/kb/297878

  ; Check if this install location registered as the StartMenuInternet client
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  ClearErrors
  ReadRegStr $0 HKCU "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${If} ${Errors}
  ${OrIf} ${AtMostWin2008R2}
    ClearErrors
    ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${EndIf}

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

  ${SetStartMenuInternet} "HKLM"

  SetShellVarContext all  ; Set SHCTX to all users (e.g. HKLM)

  ${FixClassKeys} ; Does not use SHCTX
  ${FixShellIconHandler} "HKLM"
  ${RemoveDeprecatedKeys} ; Does not use SHCTX

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

!endif ; NO_LOG
