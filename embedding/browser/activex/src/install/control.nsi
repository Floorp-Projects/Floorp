!define VERSION "v1.5"
!define DISTDIR "d:\m\source\mozilla\dist\Embed"

Name "Mozilla ActiveX Control ${VERSION}"

InstallDir "$PROGRAMFILES\Mozilla Control ${VERSION}"

OutFile MozillaControl.exe

DirText "This will install the Mozilla ActiveX Control ${VERSION} on your computer. Pick an installation directory."

Section "Mozilla Control (required)"

  SetOutPath $INSTDIR
  File /r "${DISTDIR}\*.*"
  
  RegDLL mozctlx.dll
  
SectionEnd

Section "uninstall"

  UnRegDLL mozctlx.dll
  Delete $INSTDIR\Uninst.exe
  RMDir /r $INSTDIR

SectionEnd
