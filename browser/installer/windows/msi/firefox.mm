;--- Include MAKEMSI support ------------------------------------------------
#include "firefox.mmh"

<$AbortIf Condition=^not VersionNT OR (VersionNT < 400)^ Message=^Can only be installed on WinNT, Windows 2000, XP or greater.^>

<$DirectoryTree Key="INSTALLDIR" DIR="[ProgramFilesFolder]\Mozilla Firefox Installer" CHANGE="\" PrimaryFolder="Y">

<$Feature "F_FirefoxMain" Title="Browser" Description="Firefox installer files" Attributes="FavorLocal">
        
    <$Icon KEY="@Firefoxicon" "..\branding\firefox.ico">

    <$Files "..\install\setup\*.*" SubDir="TREE" DestDir="INSTALLDIR" EXLIST="NotThese">
    <$Files "..\install\xpi\*.*" SubDir="TREE" DestDir="INSTALLDIR" EXLIST="NotThese">
       
    #(
        <$ExeCA
	    EXE="[INSTALLDIR]setup.exe"
	    Args="-ms -nosplash -setDefaultBrowser"
	    Seq="6401" ;; Run this between PublishProduct and InstallFinalize
	    Type="AnyRC"
	>
    #)
<$/Feature>

; fix dialog picture and make it pretty
<$Table "Control">
    #(
        <$Row
                   @Where="Control = 'Bitmap'"
                    @OK="=8"
               Attributes="1048577"
        >
    #)
<$/Table>
