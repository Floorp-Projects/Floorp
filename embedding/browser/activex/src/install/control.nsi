!include "local.nsh"

!define PRODUCT "Mozilla ActiveX Control ${VERSION}"

Name "Mozilla ActiveX Control ${VERSION}"

InstallDir "$PROGRAMFILES\Mozilla Control ${VERSION}"

OutFile MozillaControl.exe

DirText "This will install the Mozilla ActiveX Control ${VERSION} on your computer. Pick an installation directory."

Section "Mozilla Control (required)"

  !include "files.nsh"

  ; TODO install MSVC++ redistributable DLLs - msvcrt.dll & msvcp60.dll
  
  RegDLL "$INSTDIR\mozctlx.dll"
  
  WriteUninstaller Uninst.exe

SectionEnd

Section "uninstall"

  UnRegDLL "$INSTDIR\mozctlx.dll"
  Delete $INSTDIR\Uninst.exe
  RMDir /r $INSTDIR

SectionEnd
