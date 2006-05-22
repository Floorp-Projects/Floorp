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

# With current versions of flashplayer the location of the plugin is found via
# the registry so the following has been commented out since it should no longer
# be necessary.
  ; Check if Netscape Navigator (pre 6.0) is installed and if the
  ; flash player is installed in Netscape's plugin folder. If it is,
  ; try to copy the flashplayer.xpt file into our plugins folder to
  ; make ensure that flash is scriptable if we're using it from
  ; Netscape's plugins folder.
/*
  StrCpy $TmpVal "Software\Netscape\Netscape Navigator"
  ReadRegStr $0 HKLM $TmpVal "CurrentVersion"
  ${Unless} ${Errors}
    StrCpy $TmpVal "$TmpVal\$0\Main"
    ReadRegStr $0 HKLM $TmpVal "Plugins Directory"
    ${Unless} ${Errors}
      GetFullPathName $TmpVal $0
      ${Unless} ${Errors}
        StrCpy $1 $TmpVal "" -1
        ${If} $1 == "\"
          StrCpy $TmpVal "$TmpVal" -1
        ${EndIf}
        GetFullPathName $TmpVal "$TmpVal\flashplayer.xpt"
        ${Unless} ${Errors}
          CopyFiles /SILENT $TmpVal "$INSTDIR\plugins"
          ${LogUninstall} "\plugins\flashplayer.xpt"
          ${If} ${Errors}
            ${LogMsg} "** ERROR Copying File: $TmpVal **"
          ${Else}
            ${LogMsg} "Source     : $TmpVal"
            ${LogMsg} "Destination: $INSTDIR\plugins\flashplayer.xpt"
          ${EndIf}
        ${EndUnless}
      ${EndUnless}
    ${EndUnless}
  ${EndUnless}
*/
