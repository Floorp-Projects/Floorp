# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
  ${WordFind} "$0" "-" "+1{" $0
  ${If} $0 == "FirefoxURL"
  ${AndIf} $9 != 0 ; We're not running in session 0
    ReadRegStr $0 HKCU "Software\Classes\FirefoxURL\shell\open\command" ""
    ${GetPathFromString} "$0" $0
    ${GetParent} "$0" $0
    ${If} ${FileExists} "$0"
      ${GetLongPath} "$0" $0
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

  ClearErrors
  WriteRegStr HKLM "Software\Mozilla" "${BrandShortName}InstallerTest" "Write Test"
  ${If} ${Errors}
    StrCpy $TmpVal "HKCU"
  ${Else}
    SetShellVarContext all    ; Set SHCTX to all users (e.g. HKLM)
    DeleteRegValue HKLM "Software\Mozilla" "${BrandShortName}InstallerTest"
    StrCpy $TmpVal "HKLM"
    ${RegCleanMain} "Software\Mozilla"
    ${RegCleanUninstall}
    ${UpdateProtocolHandlers}
    ${FixShellIconHandler} "HKLM"
    ${SetAppLSPCategories} ${LSP_CATEGORIES}

    ; Add the Firewall entries after an update
    Call AddFirewallEntries

    ReadRegStr $0 HKLM "Software\mozilla.org\Mozilla" "CurrentVersion"
    ${If} "$0" != "${GREVersion}"
      WriteRegStr HKLM "Software\mozilla.org\Mozilla" "CurrentVersion" "${GREVersion}"
    ${EndIf}
  ${EndIf}

  ; Adds a pinned Task Bar shortcut (see MigrateTaskBarShortcut for details).
  ${MigrateTaskBarShortcut}

  ; Update the name/icon/AppModelID of our shortcuts as needed, then update the
  ; lastwritetime of the Start Menu shortcut to clear the tile icon cache.
  ; Do this for both shell contexts in case the user has shortcuts in multiple
  ; locations, then restore the previous context at the end.
  SetShellVarContext all
  ${UpdateShortcutsBranding}
  ${If} ${AtLeastWin8}
    ${TouchStartMenuShortcut}
  ${EndIf}
  Call FixShortcutAppModelIDs
  SetShellVarContext current
  ${UpdateShortcutsBranding}
  ${If} ${AtLeastWin8}
    ${TouchStartMenuShortcut}
  ${EndIf}
  Call FixShortcutAppModelIDs
  ${If} $TmpVal == "HKLM"
    SetShellVarContext all
  ${ElseIf} $TmpVal == "HKCU"
    SetShellVarContext current
  ${EndIf}

  ${RemoveDeprecatedKeys}
  ${Set32to64DidMigrateReg}

  ${SetAppKeys}
  ${FixClassKeys}
  ${SetUninstallKeys}
  ${If} $TmpVal == "HKLM"
    ${SetStartMenuInternet} HKLM
  ${ElseIf} $TmpVal == "HKCU"
    ${SetStartMenuInternet} HKCU
  ${EndIf}

  ; Remove files that may be left behind by the application in the
  ; VirtualStore directory.
  ${CleanVirtualStore}

  ${RemoveDeprecatedFiles}

  ; Fix the distribution.ini file if applicable
  ${FixDistributionsINI}

  RmDir /r /REBOOTOK "$INSTDIR\${TO_BE_DELETED}"

  ; Register AccessibleHandler.dll with COM (this requires write access to HKLM)
  ${RegisterAccessibleHandler}

  ; Register AccessibleMarshal.dll with COM (this requires write access to HKLM)
  ${RegisterAccessibleMarshal}

  ; Record the Windows Error Reporting module
  WriteRegDWORD HKLM "SOFTWARE\Microsoft\Windows\Windows Error Reporting\RuntimeExceptionHelperModules" "$INSTDIR\mozwer.dll" 0

!ifdef MOZ_MAINTENANCE_SERVICE
  Call IsUserAdmin
  Pop $R0
  ${If} $R0 == "true"
  ; Only proceed if we have HKLM write access
  ${AndIf} $TmpVal == "HKLM"
    ; We check to see if the maintenance service install was already attempted.
    ; Since the Maintenance service can be installed either x86 or x64,
    ; always use the 64-bit registry for checking if an attempt was made.
    ${If} ${RunningX64}
    ${OrIf} ${IsNativeARM64}
      SetRegView 64
    ${EndIf}
    ReadRegDWORD $5 HKLM "Software\Mozilla\MaintenanceService" "Attempted"
    ClearErrors
    ${If} ${RunningX64}
    ${OrIf} ${IsNativeARM64}
      SetRegView lastused
    ${EndIf}

    ; Add the registry keys for allowed certificates.
    ${AddMaintCertKeys}

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

!ifdef MOZ_LAUNCHER_PROCESS
  ${ResetLauncherProcessDefaults}
!endif

; Make sure the scheduled task registration for the default browser agent gets
; updated, but only if we're not the instance of PostUpdate that was started
; by the service, because this needs to run as the actual user. Also, don't do
; that if the installer was told not to register the agent task at all.
!ifdef MOZ_DEFAULT_BROWSER_AGENT
${If} $TmpVal == "HKCU"
  ClearErrors
  ReadRegDWORD $0 HKCU "Software\Mozilla\${AppName}\Installer\$AppUserModelID" \
                    "DidRegisterDefaultBrowserAgent"
  ${If} $0 != 0
  ${OrIf} ${Errors}
    ExecWait '"$INSTDIR\default-browser-agent.exe" register-task $AppUserModelID'
  ${EndIf}
${ElseIf} $TmpVal == "HKLM"
  ; If we're the privileged PostUpdate, make sure that the unprivileged one
  ; will have permission to create a task by clearing out the old one first.
  ExecWait '"$INSTDIR\default-browser-agent.exe" unregister-task $AppUserModelID'
${EndIf}
!endif

${RemoveDefaultBrowserAgentShortcut}
!macroend
!define PostUpdate "!insertmacro PostUpdate"

; Update the last modified time on the Start Menu shortcut, so that its icon
; gets refreshed. Should be called on Win8+ after UpdateShortcutBranding.
!macro TouchStartMenuShortcut
  ${If} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
    FileOpen $0 "$SMPROGRAMS\${BrandShortName}.lnk" a
    ${IfNot} ${Errors}
      System::Call '*(i, i) p .r1'
      System::Call 'kernel32::GetSystemTimeAsFileTime(p r1)'
      System::Call 'kernel32::SetFileTime(p r0, i 0, i 0, p r1) i .r2'
      System::Free $1
      FileClose $0
    ${EndIf}
  ${EndIf}
!macroend
!define TouchStartMenuShortcut "!insertmacro TouchStartMenuShortcut"

!macro SetAsDefaultAppGlobal
  ${RemoveDeprecatedKeys} ; Does not use SHCTX

  SetShellVarContext all      ; Set SHCTX to all users (e.g. HKLM)
  ${SetHandlers} ; Uses SHCTX
  ${SetStartMenuInternet} "HKLM"
  ${FixShellIconHandler} "HKLM"
  ${ShowShortcuts}
!macroend
!define SetAsDefaultAppGlobal "!insertmacro SetAsDefaultAppGlobal"

; Removes shortcuts for this installation. This should also remove the
; application from Open With for the file types the application handles
; (bug 370480).
!macro HideShortcuts
  ; Find the correct registry path to clear IconsVisible.
  StrCpy $R1 "Software\Clients\StartMenuInternet\${AppRegName}-$AppUserModelID\InstallInfo"
  ReadRegDWORD $0 HKLM "$R1" "ShowIconsCommand"
  ${If} ${Errors}
    ${StrFilter} "${FileMainEXE}" "+" "" "" $0
    StrCpy $R1 "Software\Clients\StartMenuInternet\$0\InstallInfo"
  ${EndIf}
  WriteRegDWORD HKLM "$R1" "IconsVisible" 0
  ${If} ${AtLeastWin8}
    WriteRegDWORD HKCU "$R1" "IconsVisible" 0
  ${EndIf}

  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
    SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
  ${EndUnless}

  ${If} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
    ShellLink::GetShortCutArgs "$DESKTOP\${BrandShortName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$DESKTOP\${BrandShortName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$DESKTOP\${BrandShortName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  SetShellVarContext all  ; Set $SMPROGRAMS to All Users
  ${Unless} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
    SetShellVarContext current  ; Set $SMPROGRAMS to the current user's Start
                                ; Menu Programs directory
  ${EndUnless}

  ${If} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
    ShellLink::GetShortCutArgs "$SMPROGRAMS\${BrandShortName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$SMPROGRAMS\${BrandShortName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$SMPROGRAMS\${BrandShortName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}

  ${If} ${FileExists} "$QUICKLAUNCH\${BrandShortName}.lnk"
    ShellLink::GetShortCutArgs "$QUICKLAUNCH\${BrandShortName}.lnk"
    Pop $0
    ${If} "$0" == ""
      ShellLink::GetShortCutTarget "$QUICKLAUNCH\${BrandShortName}.lnk"
      Pop $0
      ${GetLongPath} "$0" $0
      ${If} "$0" == "$INSTDIR\${FileMainEXE}"
        Delete "$QUICKLAUNCH\${BrandShortName}.lnk"
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define HideShortcuts "!insertmacro HideShortcuts"

; Adds shortcuts for this installation. This should also add the application
; to Open With for the file types the application handles (bug 370480).
!macro ShowShortcuts
  ; Find the correct registry path to set IconsVisible.
  StrCpy $R1 "Software\Clients\StartMenuInternet\${AppRegName}-$AppUserModelID\InstallInfo"
  ReadRegDWORD $0 HKLM "$R1" "ShowIconsCommand"
  ${If} ${Errors}
    ${StrFilter} "${FileMainEXE}" "+" "" "" $0
    StrCpy $R1 "Software\Clients\StartMenuInternet\$0\InstallInfo"
  ${EndIf}
  WriteRegDWORD HKLM "$R1" "IconsVisible" 1
  ${If} ${AtLeastWin8}
    WriteRegDWORD HKCU "$R1" "IconsVisible" 1
  ${EndIf}

  SetShellVarContext all  ; Set $DESKTOP to All Users
  ${Unless} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
    CreateShortCut "$DESKTOP\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandShortName}.lnk" "$INSTDIR"
      ${If} ${AtLeastWin7}
      ${AndIf} "$AppUserModelID" != ""
        ApplicationID::Set "$DESKTOP\${BrandShortName}.lnk" "$AppUserModelID" "true"
      ${EndIf}
    ${Else}
      SetShellVarContext current  ; Set $DESKTOP to the current user's desktop
      ${Unless} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
        CreateShortCut "$DESKTOP\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
        ${If} ${FileExists} "$DESKTOP\${BrandShortName}.lnk"
          ShellLink::SetShortCutWorkingDirectory "$DESKTOP\${BrandShortName}.lnk" \
                                                 "$INSTDIR"
          ${If} ${AtLeastWin7}
          ${AndIf} "$AppUserModelID" != ""
            ApplicationID::Set "$DESKTOP\${BrandShortName}.lnk" "$AppUserModelID" "true"
          ${EndIf}
        ${EndIf}
      ${EndUnless}
    ${EndIf}
  ${EndUnless}

  SetShellVarContext all  ; Set $SMPROGRAMS to All Users
  ${Unless} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
    CreateShortCut "$SMPROGRAMS\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandShortName}.lnk" \
                                             "$INSTDIR"
      ${If} ${AtLeastWin7}
      ${AndIf} "$AppUserModelID" != ""
        ApplicationID::Set "$SMPROGRAMS\${BrandShortName}.lnk" "$AppUserModelID" "true"
      ${EndIf}
    ${Else}
      SetShellVarContext current  ; Set $SMPROGRAMS to the current user's Start
                                  ; Menu Programs directory
      ${Unless} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
        CreateShortCut "$SMPROGRAMS\${BrandShortName}.lnk" "$INSTDIR\${FileMainEXE}"
        ${If} ${FileExists} "$SMPROGRAMS\${BrandShortName}.lnk"
          ShellLink::SetShortCutWorkingDirectory "$SMPROGRAMS\${BrandShortName}.lnk" \
                                                 "$INSTDIR"
          ${If} ${AtLeastWin7}
          ${AndIf} "$AppUserModelID" != ""
            ApplicationID::Set "$SMPROGRAMS\${BrandShortName}.lnk" "$AppUserModelID" "true"
          ${EndIf}
        ${EndIf}
      ${EndUnless}
    ${EndIf}
  ${EndUnless}

  ; Windows 7 doesn't use the QuickLaunch directory
  ${Unless} ${AtLeastWin7}
  ${AndUnless} ${FileExists} "$QUICKLAUNCH\${BrandShortName}.lnk"
    CreateShortCut "$QUICKLAUNCH\${BrandShortName}.lnk" \
                   "$INSTDIR\${FileMainEXE}"
    ${If} ${FileExists} "$QUICKLAUNCH\${BrandShortName}.lnk"
      ShellLink::SetShortCutWorkingDirectory "$QUICKLAUNCH\${BrandShortName}.lnk" \
                                             "$INSTDIR"
    ${EndIf}
  ${EndUnless}
!macroend
!define ShowShortcuts "!insertmacro ShowShortcuts"

; Update the branding name on all shortcuts our installer created
; to convert from BrandFullName (which is what we used to name shortcuts)
; to BrandShortName (which is what we now name shortcuts). We only rename
; desktop and start menu shortcuts, because touching taskbar pins often
; (but inconsistently) triggers various broken behaviors in the shell.
; This assumes SHCTX is set correctly.
!macro UpdateShortcutsBranding
  ${UpdateOneShortcutBranding} "STARTMENU" "$SMPROGRAMS"
  ${UpdateOneShortcutBranding} "DESKTOP" "$DESKTOP"
!macroend
!define UpdateShortcutsBranding "!insertmacro UpdateShortcutsBranding"

!macro UpdateOneShortcutBranding LOG_SECTION SHORTCUT_DIR
  ; Only try to rename the shortcuts found in the shortcuts log, to avoid
  ; blowing away a name that the user created.
  ${GetLongPath} "$INSTDIR\uninstall\${SHORTCUTS_LOG}" $R9
  ${If} ${FileExists} "$R9"
    ClearErrors
    ; The shortcuts log contains a numbered list of entries for each section,
    ; but we never actually create more than one.
    ReadINIStr $R8 "$R9" "${LOG_SECTION}" "Shortcut0"
    ${IfNot} ${Errors}
      ${If} ${FileExists} "${SHORTCUT_DIR}\$R8"
        ShellLink::GetShortCutTarget "${SHORTCUT_DIR}\$R8"
        Pop $R7
        ${GetLongPath} "$R7" $R7
        ${If} $R7 == "$INSTDIR\${FileMainEXE}"
        ${AndIf} $R8 != "${BrandShortName}.lnk"
        ${AndIfNot} ${FileExists} "${SHORTCUT_DIR}\${BrandShortName}.lnk"
          ClearErrors
          Rename "${SHORTCUT_DIR}\$R8" "${SHORTCUT_DIR}\${BrandShortName}.lnk"
          ${IfNot} ${Errors}
            ; Update the shortcut log manually instead of calling LogShortcut
            ; because it would add a Shortcut1 entry, and we really do want to
            ; overwrite the existing entry 0, since we just renamed the file.
            WriteINIStr "$R9" "${LOG_SECTION}" "Shortcut0" \
                        "${BrandShortName}.lnk"
          ${EndIf}
        ${EndIf}
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend
!define UpdateOneShortcutBranding "!insertmacro UpdateOneShortcutBranding"

; Remove a shortcut unintentionally added by the default browser agent (bug 1672957, 1681207)
!macro RemoveDefaultBrowserAgentShortcut
  Push $0
  Push $1
  Push $2
  Push $3

  ; Get the current user's Start Menu Programs.
  ${GetProgramsFolder} $1

  ; The shortcut would have been named MOZ_BASE_NAME regardless of branding.
  ; According to defines.nsi.in AppName should match application.ini, and application.ini.in sets
  ; [App] Name from MOZ_BASE_NAME.
  StrCpy $1 "$1\${AppName}.lnk"
  ShellLink::GetShortCutTarget $1
  Pop $0

  ; ShellLink::GetShortCutTarget, and the underlying IShellLink::GetPath(), have an issue
  ; where "C:\Program Files" becomes "C:\Program Files (x86)" in some cases.
  ; It should be OK to remove the shortcut (which matches our app name) even if it isn't from this
  ; install, as long as the file name portion of the target path matches.
  StrCpy $2 "\default-browser-agent.exe"
  StrLen $3 $2
  ; Select the substring to match from the end of the target path.
  StrCpy $0 $0 $3 -$3
  ${If} $0 == $2
    Delete $1
  ${EndIf}

  Pop $3
  Pop $2
  Pop $1
  Pop $0
!macroend
!define RemoveDefaultBrowserAgentShortcut "!insertmacro RemoveDefaultBrowserAgentShortcut"

!macro AddAssociationIfNoneExist FILE_TYPE KEY
  ClearErrors
  EnumRegKey $7 HKCR "${FILE_TYPE}" 0
  ${If} ${Errors}
    WriteRegStr SHCTX "SOFTWARE\Classes\${FILE_TYPE}"  "" ${KEY}
  ${EndIf}
  WriteRegStr SHCTX "SOFTWARE\Classes\${FILE_TYPE}\OpenWithProgids" ${KEY} ""
!macroend
!define AddAssociationIfNoneExist "!insertmacro AddAssociationIfNoneExist"

; Adds the protocol and file handler registry entries for making Firefox the
; default handler (uses SHCTX).
!macro SetHandlers
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8

  ; See if we're using path hash suffixed registry keys for this install.
  StrCpy $5 ""
  ${StrFilter} "${FileMainEXE}" "+" "" "" $2
  ReadRegStr $0 SHCTX "Software\Clients\StartMenuInternet\$2\DefaultIcon" ""
  StrCpy $0 $0 -2
  ${If} $0 != $8
    StrCpy $5 "-$AppUserModelID"
  ${EndIf}

  StrCpy $0 "SOFTWARE\Classes"
  StrCpy $2 "$\"$8$\" -osint -url $\"%1$\""

  ; Associate the file handlers with FirefoxHTML, if they aren't already.
  ReadRegStr $6 SHCTX "$0\.htm" ""
  ${WordFind} "$6" "-" "+1{" $6
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.htm"   "" "FirefoxHTML$5"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.html" ""
  ${WordFind} "$6" "-" "+1{" $6
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.html"  "" "FirefoxHTML$5"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.shtml" ""
  ${WordFind} "$6" "-" "+1{" $6
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.shtml" "" "FirefoxHTML$5"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.xht" ""
  ${WordFind} "$6" "-" "+1{" $6
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.xht"   "" "FirefoxHTML$5"
  ${EndIf}

  ReadRegStr $6 SHCTX "$0\.xhtml" ""
  ${WordFind} "$6" "-" "+1{" $6
  ${If} "$6" != "FirefoxHTML"
    WriteRegStr SHCTX "$0\.xhtml" "" "FirefoxHTML$5"
  ${EndIf}


  ${AddAssociationIfNoneExist} ".pdf" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".oga" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".ogg" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".ogv" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".pdf" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".webm" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".svg" "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".webp"  "FirefoxHTML$5"
  ${AddAssociationIfNoneExist} ".avif" "FirefoxHTML$5"

  ; An empty string is used for the 5th param because FirefoxHTML is not a
  ; protocol handler
  ${AddDisabledDDEHandlerValues} "FirefoxHTML$5" "$2" "$8,1" \
                                 "${AppRegName} HTML Document" ""

  ${AddDisabledDDEHandlerValues} "FirefoxURL$5" "$2" "$8,1" "${AppRegName} URL" \
                                 "true"
  ; An empty string is used for the 4th & 5th params because the following
  ; protocol handlers already have a display name and the additional keys
  ; required for a protocol handler.
  ${AddDisabledDDEHandlerValues} "http" "$2" "$8,1" "" ""
  ${AddDisabledDDEHandlerValues} "https" "$2" "$8,1" "" ""
  ${AddDisabledDDEHandlerValues} "mailto" "$2" "$8,1" "" ""
!macroend
!define SetHandlers "!insertmacro SetHandlers"

!macro WriteApplicationsSupportedType RegKey Type
  WriteRegStr ${RegKey} "Software\Classes\Applications\${FileMainEXE}\SupportedTypes" "${Type}" ""
!macroend
!define WriteApplicationsSupportedType "!insertmacro WriteApplicationsSupportedType"

; Adds the HKLM\Software\Clients\StartMenuInternet\Firefox-[pathhash] registry
; entries (does not use SHCTX).
;
; The values for StartMenuInternet are only valid under HKLM and there can only
; be one installation registerred under StartMenuInternet per application since
; the key name is derived from the main application executable.
;
; In Windows 8 this changes slightly, you can store StartMenuInternet entries in
; HKCU.  The icon in start menu for StartMenuInternet is deprecated as of Win7,
; but the subkeys are what's important.  Control panel default programs looks
; for them only in HKLM pre win8.
;
; The StartMenuInternet key and friends are documented at
; https://msdn.microsoft.com/en-us/library/windows/desktop/cc144109(v=vs.85).aspx
;
; This function also writes our RegisteredApplications entry, which gets us
; listed in the Settings app's default browser options on Windows 8+, and in
; Set Program Access and Defaults on earlier versions.
!macro SetStartMenuInternet RegKey
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  ${GetLongPath} "$INSTDIR\uninstall\helper.exe" $7

  ; If we already have keys at the old FIREFOX.EXE path, then just update those.
  ; We have to be careful to update the existing keys in place so that we don't
  ; create duplicate keys for the same installation, or cause Windows to think
  ; something "suspicious" has happened and it should reset the default browser.
  ${StrFilter} "${FileMainEXE}" "+" "" "" $1
  ReadRegStr $0 ${RegKey} "Software\Clients\StartMenuInternet\$1\DefaultIcon" ""
  StrCpy $0 $0 -2
  ${If} $0 != $8
    StrCpy $1 "${AppRegName}-$AppUserModelID"
    StrCpy $2 "-$AppUserModelID"
  ${Else}
    StrCpy $2 ""
  ${EndIf}
  StrCpy $0 "Software\Clients\StartMenuInternet\$1"

  WriteRegStr ${RegKey} "$0" "" "${BrandFullName}"

  WriteRegStr ${RegKey} "$0\DefaultIcon" "" "$8,0"

  ; The Reinstall Command is defined at
  ; http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/programmersguide/shell_adv/registeringapps.asp
  WriteRegStr ${RegKey} "$0\InstallInfo" "HideIconsCommand" "$\"$7$\" /HideShortcuts"
  WriteRegStr ${RegKey} "$0\InstallInfo" "ShowIconsCommand" "$\"$7$\" /ShowShortcuts"
  WriteRegStr ${RegKey} "$0\InstallInfo" "ReinstallCommand" "$\"$7$\" /SetAsDefaultAppGlobal"
  WriteRegDWORD ${RegKey} "$0\InstallInfo" "IconsVisible" 1

  WriteRegStr ${RegKey} "$0\shell\open\command" "" "$\"$8$\""

  WriteRegStr ${RegKey} "$0\shell\properties" "" "$(CONTEXT_OPTIONS)"
  WriteRegStr ${RegKey} "$0\shell\properties\command" "" "$\"$8$\" -preferences"

  WriteRegStr ${RegKey} "$0\shell\safemode" "" "$(CONTEXT_SAFE_MODE)"
  WriteRegStr ${RegKey} "$0\shell\safemode\command" "" "$\"$8$\" -safe-mode"

  ; Capabilities registry keys
  WriteRegStr ${RegKey} "$0\Capabilities" "ApplicationDescription" "$(REG_APP_DESC)"
  WriteRegStr ${RegKey} "$0\Capabilities" "ApplicationIcon" "$8,0"
  WriteRegStr ${RegKey} "$0\Capabilities" "ApplicationName" "${BrandShortName}"

  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".htm"   "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".html"  "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".pdf"   "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".shtml" "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".xht"   "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".xhtml" "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".svg"   "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".webp"  "FirefoxHTML$2"
  WriteRegStr ${RegKey} "$0\Capabilities\FileAssociations" ".avif"  "FirefoxHTML$2"

  WriteRegStr ${RegKey} "$0\Capabilities\StartMenu" "StartMenuInternet" "$1"

  ; In the past, we supported ftp.  Since we don't delete and re-create the
  ; entire key, we need to remove any existing registration.
  DeleteRegValue ${RegKey} "$0\Capabilities\URLAssociations" "ftp"

  WriteRegStr ${RegKey} "$0\Capabilities\URLAssociations" "http"   "FirefoxURL$2"
  WriteRegStr ${RegKey} "$0\Capabilities\URLAssociations" "https"  "FirefoxURL$2"
  WriteRegStr ${RegKey} "$0\Capabilities\URLAssociations" "mailto" "FirefoxURL$2"

  WriteRegStr ${RegKey} "Software\RegisteredApplications" "$1" "$0\Capabilities"

  ; This key would be created by the Open With dialog when a user creates an
  ; association for us with a file type that we haven't registered as a handler
  ; for. We need to preemptively create it ourselves so that we can control the
  ; command line that's used to launch us in that situation. If it's too late
  ; and one already exists, then we need to edit its command line to make sure
  ; it contains the -osint flag.
  ReadRegStr $6 ${RegKey} "Software\Classes\Applications\${FileMainEXE}\shell\open\command" ""
  ${If} $6 != ""
    ${GetPathFromString} "$6" $6
    WriteRegStr ${RegKey} "Software\Classes\Applications\${FileMainEXE}\shell\open\command" \
                "" "$\"$6$\" -osint -url $\"%1$\""
  ${Else}
    WriteRegStr ${RegKey} "Software\Classes\Applications\${FileMainEXE}\shell\open\command" \
                "" "$\"$8$\" -osint -url $\"%1$\""
    ; Make sure files associated this way use the document icon instead of the
    ; application icon.
    WriteRegStr ${RegKey} "Software\Classes\Applications\${FileMainEXE}\DefaultIcon" \
                "" "$8,1"
    ; If we're going to create this key at all, we also need to list our supported
    ; file types in it, because otherwise we'll be shown as a suggestion for every
    ; single file type, whether we support it in any way or not.
    ; We take a more expansive approach to the set of file types registered
    ; here compared to elsewhere because this key is interpreted by the OS as
    ; containing every file type that we can possibly open, so if something
    ; isn't listed it assumes we can't open it and hides us from e.g. the Open
    ; With context menu, even if the user has tried to add us there manually.
    ; The list here was derived from the file /layout/build/components.conf,
    ; filtered down to only those types which make sense to open on their own
    ; in Firefox, basically meaning that plain text file types were left out,
    ; but not JSON or XML types because we have specific viewers for those.
    ${WriteApplicationsSupportedType} ${RegKey} ".apng"
    ${WriteApplicationsSupportedType} ${RegKey} ".bmp"
    ${WriteApplicationsSupportedType} ${RegKey} ".flac"
    ${WriteApplicationsSupportedType} ${RegKey} ".gif"
    ${WriteApplicationsSupportedType} ${RegKey} ".htm"
    ${WriteApplicationsSupportedType} ${RegKey} ".html"
    ${WriteApplicationsSupportedType} ${RegKey} ".ico"
    ${WriteApplicationsSupportedType} ${RegKey} ".jfif"
    ${WriteApplicationsSupportedType} ${RegKey} ".jpeg"
    ${WriteApplicationsSupportedType} ${RegKey} ".jpg"
    ${WriteApplicationsSupportedType} ${RegKey} ".json"
    ${WriteApplicationsSupportedType} ${RegKey} ".m4a"
    ${WriteApplicationsSupportedType} ${RegKey} ".mp3"
    ${WriteApplicationsSupportedType} ${RegKey} ".oga"
    ${WriteApplicationsSupportedType} ${RegKey} ".ogg"
    ${WriteApplicationsSupportedType} ${RegKey} ".ogv"
    ${WriteApplicationsSupportedType} ${RegKey} ".opus"
    ${WriteApplicationsSupportedType} ${RegKey} ".pdf"
    ${WriteApplicationsSupportedType} ${RegKey} ".pjpeg"
    ${WriteApplicationsSupportedType} ${RegKey} ".pjp"
    ${WriteApplicationsSupportedType} ${RegKey} ".png"
    ${WriteApplicationsSupportedType} ${RegKey} ".rdf"
    ${WriteApplicationsSupportedType} ${RegKey} ".shtml"
    ${WriteApplicationsSupportedType} ${RegKey} ".svg"
    ${WriteApplicationsSupportedType} ${RegKey} ".webm"
    ${WriteApplicationsSupportedType} ${RegKey} ".webp"
    ${WriteApplicationsSupportedType} ${RegKey} ".avif"
    ${WriteApplicationsSupportedType} ${RegKey} ".xht"
    ${WriteApplicationsSupportedType} ${RegKey} ".xhtml"
    ${WriteApplicationsSupportedType} ${RegKey} ".xml"
  ${EndIf}
!macroend
!define SetStartMenuInternet "!insertmacro SetStartMenuInternet"

; Add registry keys to support the Firefox 32 bit to 64 bit migration. These
; registry entries are not removed on uninstall at this time. After the Firefox
; 32 bit to 64 bit migration effort is completed these registry entries can be
; removed during install, post update, and uninstall.
!macro Set32to64DidMigrateReg
  ${GetLongPath} "$INSTDIR" $1
  ; These registry keys are always in the 32 bit hive since they are never
  ; needed by a Firefox 64 bit install unless it has been updated from Firefox
  ; 32 bit.
  SetRegView 32

!ifdef HAVE_64BIT_BUILD

  ; Running Firefox 64 bit on Windows 64 bit
  ClearErrors
  ReadRegDWORD $2 HKLM "Software\Mozilla\${AppName}\32to64DidMigrate" "$1"
  ; If there were no errors then the system was updated from Firefox 32 bit to
  ; Firefox 64 bit and if the value is already 1 then the registry value has
  ; already been updated in the HKLM registry.
  ${IfNot} ${Errors}
  ${AndIf} $2 != 1
    ClearErrors
    WriteRegDWORD HKLM "Software\Mozilla\${AppName}\32to64DidMigrate" "$1" 1
    ${If} ${Errors}
      ; There was an error writing to HKLM so just write it to HKCU
      WriteRegDWORD HKCU "Software\Mozilla\${AppName}\32to64DidMigrate" "$1" 1
    ${Else}
      ; This will delete the value from HKCU if it exists
      DeleteRegValue HKCU "Software\Mozilla\${AppName}\32to64DidMigrate" "$1"
    ${EndIf}
  ${EndIf}

  ClearErrors
  ReadRegDWORD $2 HKCU "Software\Mozilla\${AppName}\32to64DidMigrate" "$1"
  ; If there were no errors then the system was updated from Firefox 32 bit to
  ; Firefox 64 bit and if the value is already 1 then the registry value has
  ; already been updated in the HKCU registry.
  ${IfNot} ${Errors}
  ${AndIf} $2 != 1
    WriteRegDWORD HKCU "Software\Mozilla\${AppName}\32to64DidMigrate" "$1" 1
  ${EndIf}

!else

  ; Running Firefox 32 bit
  ${If} ${RunningX64}
  ${OrIf} ${IsNativeARM64}
    ; Running Firefox 32 bit on a Windows 64 bit system
    ClearErrors
    ReadRegDWORD $2 HKLM "Software\Mozilla\${AppName}\32to64DidMigrate" "$1"
    ; If there were errors the value doesn't exist yet.
    ${If} ${Errors}
      ClearErrors
      WriteRegDWORD HKLM "Software\Mozilla\${AppName}\32to64DidMigrate" "$1" 0
      ; If there were errors write the value in HKCU.
      ${If} ${Errors}
        WriteRegDWORD HKCU "Software\Mozilla\${AppName}\32to64DidMigrate" "$1" 0
      ${EndIf}
    ${EndIf}
  ${EndIf}

!endif

  ClearErrors
  SetRegView lastused
!macroend
!define Set32to64DidMigrateReg "!insertmacro Set32to64DidMigrateReg"

; The IconHandler reference for FirefoxHTML can end up in an inconsistent state
; due to changes not being detected by the IconHandler for side by side
; installs (see bug 268512). The symptoms can be either an incorrect icon or no
; icon being displayed for files associated with Firefox (does not use SHCTX).
!macro FixShellIconHandler RegKey
  ; Find the correct key to update, either FirefoxHTML or FirefoxHTML-[PathHash]
  StrCpy $3 "FirefoxHTML-$AppUserModelID"
  ClearErrors
  ReadRegStr $0 ${RegKey} "Software\Classes\$3\DefaultIcon" ""
  ${If} ${Errors}
    StrCpy $3 "FirefoxHTML"
  ${EndIf}

  ClearErrors
  ReadRegStr $1 ${RegKey} "Software\Classes\$3\ShellEx\IconHandler" ""
  ${Unless} ${Errors}
    ReadRegStr $1 ${RegKey} "Software\Classes\$3\DefaultIcon" ""
    ${GetLongPath} "$INSTDIR\${FileMainEXE}" $2
    ${If} "$1" != "$2,1"
      WriteRegStr ${RegKey} "Software\Classes\$3\DefaultIcon" "" "$2,1"
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
  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion}$3 (${ARCH} ${AB_CD})\Main"
  ${WriteRegStr2} $TmpVal "$0" "Install Directory" "$8" 0
  ${WriteRegStr2} $TmpVal "$0" "PathToExe" "$8\${FileMainEXE}" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion}$3 (${ARCH} ${AB_CD})\Uninstall"
  ${WriteRegStr2} $TmpVal "$0" "Description" "${BrandFullNameInternal} ${AppVersion}$3 (${ARCH} ${AB_CD})" 0

  StrCpy $0 "Software\Mozilla\${BrandFullNameInternal}\${AppVersion}$3 (${ARCH} ${AB_CD})"
  ${WriteRegStr2} $TmpVal  "$0" "" "${AppVersion}$3 (${ARCH} ${AB_CD})" 0
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
  ${WriteRegStr2} $TmpVal "$0" "CurrentVersion" "${AppVersion}$3 (${ARCH} ${AB_CD})" 0
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
    ${WriteRegStr2} $1 "$0" "DisplayName" "${BrandFullNameInternal}$3 (${ARCH} ${AB_CD})" 0
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

; Due to a bug when associating some file handlers, only SHCTX was checked for
; some file types such as ".pdf". SHCTX is set to HKCU or HKLM depending on
; whether the installer has write access to HKLM. The bug would happen when
; HCKU was checked and didn't exist since programs aren't required to set the
; HKCU Software\Classes keys when associating handlers. The fix uses the merged
; view in HKCR to check for existance of an existing association. This macro
; cleans affected installations by removing the HKLM and HKCU value if it is set
; to FirefoxHTML when there is a value for PersistentHandler or by removing the
; HKCU value when the HKLM value has a value other than an empty string.
!macro FixBadFileAssociation FILE_TYPE
  ; Only delete the default value in case the key has values for OpenWithList,
  ; OpenWithProgids, PersistentHandler, etc.
  ReadRegStr $0 HKCU "Software\Classes\${FILE_TYPE}" ""
  ${WordFind} "$0" "-" "+1{" $0
  ReadRegStr $1 HKLM "Software\Classes\${FILE_TYPE}" ""
  ${WordFind} "$1" "-" "+1{" $1
  ReadRegStr $2 HKCR "${FILE_TYPE}\PersistentHandler" ""
  ${If} "$2" != ""
    ; Since there is a persistent handler remove FirefoxHTML as the default
    ; value from both HKCU and HKLM if it set to FirefoxHTML.
    ${If} "$0" == "FirefoxHTML"
      DeleteRegValue HKCU "Software\Classes\${FILE_TYPE}" ""
    ${EndIf}
    ${If} "$1" == "FirefoxHTML"
      DeleteRegValue HKLM "Software\Classes\${FILE_TYPE}" ""
    ${EndIf}
  ${ElseIf} "$0" == "FirefoxHTML"
    ; Since HKCU is set to FirefoxHTML remove FirefoxHTML as the default value
    ; from HKCU if HKLM is set to a value other than an empty string.
    ${If} "$1" != ""
      DeleteRegValue HKCU "Software\Classes\${FILE_TYPE}" ""
    ${EndIf}
  ${EndIf}
!macroend
!define FixBadFileAssociation "!insertmacro FixBadFileAssociation"

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

  ; Remove possibly badly associated file types
  ${FixBadFileAssociation} ".pdf"
  ${FixBadFileAssociation} ".oga"
  ${FixBadFileAssociation} ".ogg"
  ${FixBadFileAssociation} ".ogv"
  ${FixBadFileAssociation} ".pdf"
  ${FixBadFileAssociation} ".webm"
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

  ${IsHandlerForInstallDir} "FirefoxHTML-$AppUserModelID" $R9
  ${If} "$R9" == "true"
    ; An empty string is used for the 5th param because FirefoxHTML is not a
    ; protocol handler.
    ${AddDisabledDDEHandlerValues} "FirefoxHTML-$AppUserModelID" "$2" "$8,1" \
                                   "${AppRegName} HTML Document" ""
  ${Else}
    ${IsHandlerForInstallDir} "FirefoxHTML" $R9
    ${If} "$R9" == "true"
      ${AddDisabledDDEHandlerValues} "FirefoxHTML" "$2" "$8,1" \
                                     "${AppRegName} HTML Document" ""
    ${EndIf}
  ${EndIf}

  ${IsHandlerForInstallDir} "FirefoxURL-$AppUserModelID" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "FirefoxURL-$AppUserModelID" "$2" "$8,1" \
                                   "${AppRegName} URL" "true"
  ${Else}
    ${IsHandlerForInstallDir} "FirefoxURL" $R9
    ${If} "$R9" == "true"
      ${AddDisabledDDEHandlerValues} "FirefoxURL" "$2" "$8,1" \
                                     "${AppRegName} URL" "true"
    ${EndIf}
  ${EndIf}

  ; An empty string is used for the 4th & 5th params because the following
  ; protocol handlers already have a display name and the additional keys
  ; required for a protocol handler.

  ${IsHandlerForInstallDir} "ftp" $R9
  ${If} "$R9" == "true"
    ; In the past, we supported ftp, so we need to delete any registration.
    ${AddDisabledDDEHandlerValues} "ftp" "$2" "$8,1" "" "delete"
  ${EndIf}

  ${IsHandlerForInstallDir} "http" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "http" "$2" "$8,1" "" ""
  ${EndIf}

  ${IsHandlerForInstallDir} "https" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "https" "$2" "$8,1" "" ""
  ${EndIf}

  ${IsHandlerForInstallDir} "mailto" $R9
  ${If} "$R9" == "true"
    ${AddDisabledDDEHandlerValues} "mailto" "$2" "$8,1" "" ""
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
    ${OrIf} ${IsNativeARM64}
      SetRegView 64
    ${EndIf}

    ; PrefetchProcessName was originally used to experiment with deleting
    ; Windows prefetch as a speed optimization.  It is no longer used though.
    DeleteRegValue HKLM "$R0" "prefetchProcessName"

    ; Setting the Attempted value will ensure that a new Maintenance Service
    ; install will never be attempted again after this from updates.  The value
    ; is used only to see if updates should attempt new service installs.
    WriteRegDWORD HKLM "Software\Mozilla\MaintenanceService" "Attempted" 1

    ; These values associate the allowed certificates for the current
    ; installation.
    WriteRegStr HKLM "$R0\0" "name" "${CERTIFICATE_NAME}"
    WriteRegStr HKLM "$R0\0" "issuer" "${CERTIFICATE_ISSUER}"
    ; These values associate the allowed certificates for the previous
    ;  installation, so that we can update from it cleanly using the
    ;  old updater.exe (which will still have this signature).
    WriteRegStr HKLM "$R0\1" "name" "${CERTIFICATE_NAME_PREVIOUS}"
    WriteRegStr HKLM "$R0\1" "issuer" "${CERTIFICATE_ISSUER_PREVIOUS}"
    ${If} ${RunningX64}
    ${OrIf} ${IsNativeARM64}
      SetRegView lastused
    ${EndIf}
    ClearErrors
  ${EndIf}
  ; Restore the previously used value back
  Pop $R0
!macroend
!define AddMaintCertKeys "!insertmacro AddMaintCertKeys"
!endif

!macro RegisterAccessibleHandler
  ${RegisterDLL} "$INSTDIR\AccessibleHandler.dll"
!macroend
!define RegisterAccessibleHandler "!insertmacro RegisterAccessibleHandler"

!macro RegisterAccessibleMarshal
  ${RegisterDLL} "$INSTDIR\AccessibleMarshal.dll"
!macroend
!define RegisterAccessibleMarshal "!insertmacro RegisterAccessibleMarshal"

; Removes various registry entries for reasons noted below (does not use SHCTX).
!macro RemoveDeprecatedKeys
  StrCpy $0 "SOFTWARE\Classes"
  ; Remove support for launching chrome urls from the shell during install or
  ; update if the DefaultIcon is from firefox.exe (Bug 301073).
  ${RegCleanAppHandler} "chrome"

  ; Remove protocol handler registry keys added by the MS shim
  DeleteRegKey HKLM "Software\Classes\Firefox.URL"
  DeleteRegKey HKCU "Software\Classes\Firefox.URL"
!macroend
!define RemoveDeprecatedKeys "!insertmacro RemoveDeprecatedKeys"

; Removes various directories and files for reasons noted below.
!macro RemoveDeprecatedFiles
  ; Remove the toplevel chrome.manifest added by bug 1295542.
  ${If} ${FileExists} "$INSTDIR\chrome.manifest"
    Delete "$INSTDIR\chrome.manifest"
  ${EndIf}

  ; Remove talkback if it is present (remove after bug 386760 is fixed)
  ${If} ${FileExists} "$INSTDIR\extensions\talkback@mozilla.org"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\talkback@mozilla.org"
  ${EndIf}

  ; Remove the Java Console extension (bug 1165156)
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0031-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0031-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0034-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0034-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0039-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0039-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0045-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0016-0000-0045-ABCDEFFEDCBA}"
  ${EndIf}
  ${If} ${FileExists} "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0000-ABCDEFFEDCBA}"
    RmDir /r /REBOOTOK "$INSTDIR\extensions\{CAFEEFAC-0017-0000-0000-ABCDEFFEDCBA}"
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
      WriteRegDWORD HKCU \
        "Software\Mozilla\${AppName}\Installer\$AppUserModelID" \
        "WasPinnedToTaskbar" 1
      ${If} ${AtLeastWin7}
        ; If we didn't run the stub installer, AddTaskbarSC will be empty.
        ; We determine whether to pin based on whether we're the default
        ; browser, or if we're on win8 or later, we always pin.
        ${If} $AddTaskbarSC == ""
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
        ${ElseIf} $AddTaskbarSC == "1"
          ${PinToTaskBar}
        ${EndIf}
      ${EndIf}
    ${ElseIf} ${AtLeastWin10}
      ${GetInstallerRegistryPref} "Software\Mozilla\${AppName}" \
        "installer.taskbarpin.win10.enabled" $2
      ${If} $2 == "true"
        ; On Windows 10, we may have previously tried to make a taskbar pin
        ; and failed because the API we tried to use was blocked by the OS.
        ; We have an option that works in more cases now, so we're going to try
        ; again, but also record that we've done so by writing a particular
        ; registry value, so that we don't continue to do this repeatedly.
        ClearErrors
        ReadRegDWORD $2 HKCU \
            "Software\Mozilla\${AppName}\Installer\$AppUserModelID" \
            "WasPinnedToTaskbar"
        ${If} ${Errors}
          WriteRegDWORD HKCU \
            "Software\Mozilla\${AppName}\Installer\$AppUserModelID" \
            "WasPinnedToTaskbar" 1
          ${If} $AddTaskbarSC != "0"
            ${PinToTaskBar}
          ${EndIf}
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

            ${If} ${AtMostWin2012R2}
              ; Pin the shortcut to the TaskBar. 5386 is the shell32.dll
              ; resource id for the "Pin to Taskbar" string.
              InvokeShellVerb::DoIt "$SMPROGRAMS" "$1" "5386"
            ${Else}
              ; In Windows 10 the "Pin to Taskbar" resource was removed, so we
              ; can't access the verb that way anymore. We have a create a
              ; command key using the GUID that's assigned to this action and
              ; then invoke that as a verb.
              ReadRegStr $R9 HKLM \
                "Software\Microsoft\Windows\CurrentVersion\Explorer\CommandStore\shell\Windows.taskbarpin" \
                "ExplorerCommandHandler"
              WriteRegStr HKCU "Software\Classes\*\shell\${AppRegName}-$AppUserModelID" "ExplorerCommandHandler" $R9
              InvokeShellVerb::DoIt "$SMPROGRAMS" "$1" "${AppRegName}-$AppUserModelID"
              DeleteRegKey HKCU "Software\Classes\*\shell\${AppRegName}-$AppUserModelID"
            ${EndIf}

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
    ${LogStartMenuShortcut} "${BrandShortName}.lnk"
    ${LogQuickLaunchShortcut} "${BrandShortName}.lnk"
    ${LogDesktopShortcut} "${BrandShortName}.lnk"
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
  Push "AccessibleHandler.dll"
  Push "AccessibleMarshal.dll"
  Push "IA2Marshal.dll"
  Push "freebl3.dll"
  Push "nssckbi.dll"
  Push "nspr4.dll"
  Push "nssdbm3.dll"
  Push "mozsqlite3.dll"
  Push "xpcom.dll"
  Push "crashreporter.exe"
  Push "default-browser-agent.exe"
  Push "minidump-analyzer.exe"
  Push "pingsender.exe"
  Push "updater.exe"
  Push "mozwer.dll"
  Push "${FileMainEXE}"
!macroend
!define PushFilesToCheck "!insertmacro PushFilesToCheck"


; Pushes the string "true" to the top of the stack if the Firewall service is
; running and pushes the string "false" to the top of the stack if it isn't.
!define SC_MANAGER_ALL_ACCESS 0x3F
!define SERVICE_QUERY_CONFIG 0x0001
!define SERVICE_QUERY_STATUS 0x0004
!define SERVICE_RUNNING 0x4

!macro IsFirewallSvcRunning
  Push $R9
  Push $R8
  Push $R7
  Push $R6
  Push "false"

  System::Call 'advapi32::OpenSCManagerW(n, n, i ${SC_MANAGER_ALL_ACCESS}) i.R6'
  ${If} $R6 != 0
    ; MpsSvc is the Firewall service.
    ; When opening the service with SERVICE_QUERY_CONFIG the return value will
    ; be 0 if the service is not installed.
    System::Call 'advapi32::OpenServiceW(i R6, t "MpsSvc", i ${SERVICE_QUERY_CONFIG}) i.R7'
    ${If} $R7 != 0
      System::Call 'advapi32::CloseServiceHandle(i R7) n'
      ; Open the service with SERVICE_QUERY_STATUS so its status can be queried.
      System::Call 'advapi32::OpenServiceW(i R6, t "MpsSvc", i ${SERVICE_QUERY_STATUS}) i.R7'
    ${EndIf}
    ; Did the calls to OpenServiceW succeed?
    ${If} $R7 != 0
      System::Call '*(i,i,i,i,i,i,i) i.R9'
      ; Query the current status of the service.
      System::Call 'advapi32::QueryServiceStatus(i R7, i $R9) i'
      System::Call '*$R9(i, i.R8)'
      System::Free $R9
      System::Call 'advapi32::CloseServiceHandle(i R7) n'
      IntFmt $R8 "0x%X" $R8
      ${If} $R8 == ${SERVICE_RUNNING}
        Pop $R9
        Push "true"
      ${EndIf}
    ${EndIf}
    System::Call 'advapi32::CloseServiceHandle(i R6) n'
  ${EndIf}

  Exch 1
  Pop $R6
  Exch 1
  Pop $R7
  Exch 1
  Pop $R8
  Exch 1
  Pop $R9
!macroend
!define IsFirewallSvcRunning "!insertmacro IsFirewallSvcRunning"
!define un.IsFirewallSvcRunning "!insertmacro IsFirewallSvcRunning"

; Sets this installation as the default browser by setting the registry keys
; under HKEY_CURRENT_USER via registry calls and using the AppAssocReg NSIS
; plugin. This is a function instead of a macro so it is
; easily called from an elevated instance of the binary. Since this can be
; called by an elevated instance logging is not performed in this function.
Function SetAsDefaultAppUserHKCU
  ; See if we're using path hash suffixed registry keys for this install
  ${GetLongPath} "$INSTDIR\${FileMainEXE}" $8
  ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
  ClearErrors
  ReadRegStr $0 HKCU "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${If} ${Errors}
  ${OrIf} ${AtMostWin2008R2}
    ClearErrors
    ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
  ${EndIf}
  StrCpy $0 $0 -2
  ${If} $0 != $8
    ${If} $AppUserModelID == ""
      ${InitHashAppModelId} "$INSTDIR" "Software\Mozilla\${AppName}\TaskBarIDs"
    ${EndIf}
    StrCpy $R9 "${AppRegName}-$AppUserModelID"
  ${EndIf}

  ; Set ourselves as the user's selected StartMenuInternet browser, but only
  ; if we have StartMenuInternet registry keys that are for this install.
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
        ; On Windows >= 8, this function cannot do anything to actually set
        ; the default browser, it can only set up the registry entries to
        ; allow the user to do so. Getting here means that those entries already
        ; exist for this installation, we just found them, so there is nothing
        ; more to be done.
        ${If} ${AtLeastWin8}
          Return
        ${Else}
          WriteRegStr HKCU "Software\Clients\StartMenuInternet" "" "$R9"
        ${EndIf}
      ${EndIf}
    ${EndIf}
  ${EndUnless}

  SetShellVarContext current  ; Set SHCTX to the current user (e.g. HKCU)

  ; It's unlikely that we didn't find a StartMenuInternet key above, but it is
  ; possible; it likely would mean this copy of the application was extracted
  ; directly from a ZIP file and the installer was never run.
  ${If} ${AtLeastWin8}
    ${SetStartMenuInternet} "HKCU"
    ${FixShellIconHandler} "HKCU"
    ${FixClassKeys} ; Does not use SHCTX
  ${EndIf}

  ${SetHandlers}

  ; Only register as the handler if the app registry name
  ; exists under the RegisteredApplications registry key. The protocol and
  ; file handlers set previously at the user level will associate this install
  ; as the default browser.
  ClearErrors
  ReadRegStr $0 HKLM "Software\RegisteredApplications" "$R9"
  ${Unless} ${Errors}
    ; This is all protected by a user choice hash in Windows 8 so it won't
    ; help, but it also won't hurt.
    AppAssocReg::SetAppAsDefault "$R9" ".htm" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" ".html" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" ".shtml" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" ".webp" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" ".avif" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" ".xht" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" ".xhtml" "file"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" "http" "protocol"
    Pop $0
    AppAssocReg::SetAppAsDefault "$R9" "https" "protocol"
    Pop $0
  ${EndUnless}
  ${RemoveDeprecatedKeys}
  ${MigrateTaskBarShortcut}
FunctionEnd

; Helper for updating the shortcut application model IDs.
Function FixShortcutAppModelIDs
  ${If} ${AtLeastWin7}
  ${AndIf} "$AppUserModelID" != ""
    ${UpdateShortcutAppModelIDs} "$INSTDIR\${FileMainEXE}" "$AppUserModelID" $0
  ${EndIf}
FunctionEnd

; Helper for adding Firewall exceptions during install and after app update.
Function AddFirewallEntries
  ${IsFirewallSvcRunning}
  Pop $0
  ${If} "$0" == "true"
    liteFirewallW::AddRule "$INSTDIR\${FileMainEXE}" "${BrandShortName} ($INSTDIR)"
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

  ; Check if this install location registered as a StartMenuInternet client
  ClearErrors
  ReadRegStr $0 HKCU "Software\Clients\StartMenuInternet\${AppRegName}-$AppUserModelID\DefaultIcon" ""
  ${If} ${Errors}
  ${OrIf} ${AtMostWin2008R2}
    ClearErrors
    ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\${AppRegName}-$AppUserModelID\DefaultIcon" ""
  ${EndIf}
  ${If} ${Errors}
    ${StrFilter} "${FileMainEXE}" "+" "" "" $R9
    ClearErrors
    ReadRegStr $0 HKCU "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
    ${If} ${Errors}
    ${OrIf} ${AtMostWin2008R2}
      ClearErrors
      ReadRegStr $0 HKLM "Software\Clients\StartMenuInternet\$R9\DefaultIcon" ""
    ${EndIf}
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

  ; The code after ElevateUAC won't be executed when the user:
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

!ifdef MOZ_LAUNCHER_PROCESS
!macro ResetLauncherProcessDefaults
  # By deleting these values, we remove remnants of any force-disable settings
  # that may have been set during the SHIELD study in 67. Note that this setting
  # was only intended to distinguish between test and control groups for the
  # purposes of the study, not as a user preference.
  DeleteRegValue HKCU ${MOZ_LAUNCHER_SUBKEY} "$INSTDIR\${FileMainEXE}|Launcher"
  DeleteRegValue HKCU ${MOZ_LAUNCHER_SUBKEY} "$INSTDIR\${FileMainEXE}|Browser"
!macroend
!define ResetLauncherProcessDefaults "!insertmacro ResetLauncherProcessDefaults"
!endif
