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

