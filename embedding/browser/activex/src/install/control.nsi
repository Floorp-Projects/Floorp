!include "local.nsh"

!define PRODUCT "Mozilla ActiveX Control ${VERSION}"

Name "${PRODUCT}"

InstallDir "$PROGRAMFILES\${PRODUCT}"

OutFile MozillaControl${MAJOR_VERSION}${MINOR_VERSION}.exe

DirText "This will install the Mozilla ActiveX Control ${VERSION} on your computer. Pick an installation directory."

Section "Mozilla Control (required)"

  !include "files.nsh"

  ; TODO install MSVC++ redistributable DLLs - msvcrt.dll & msvcp60.dll
  
  RegDLL "$INSTDIR\mozctlx.dll"
  
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}" "DisplayName" "${PRODUCT}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}" "VersionMajor" "${MAJOR_VERSION}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}" "VersionMinor" "${MINOR_VERSION}"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\{PRODUCT}" "NoModify" "1"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}" "NoRepair" "1"
  WriteUninstaller Uninst.exe

SectionEnd

Section "uninstall"

  UnRegDLL "$INSTDIR\mozctlx.dll"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT}"
  Delete $INSTDIR\Uninst.exe
  RMDir /r $INSTDIR

SectionEnd
