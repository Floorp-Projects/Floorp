				CCK Read Me


What are all of these files?
-------------------------

bdate.bat - Sets the environment var, BuildID, to the value given it by the PERL script date.pl.
The BuildID var is used to name the repository folder.

CCKBuild.bat - Build automation file for this whole build processs.  Paths, in the script will have
to updated to work on a machine other than mine.  I plan to move this to PERL to better script the
build process for portability.

CCKBuild.pl - The PERL build script for CCK.  This must also have PERLBuild.bat and PERLUpload.bat
in the same folder to work.

date.pl - PERL script that creates a the date that is used to name the repository folder.  Called
by CCKBuild.bat.

PERLUpload.bat - Creates repoitory folders,  moves the wizardmachine.exe and associated ini's to the 
repository folders.   Called by CCKBuild.pl.

PERLBuild.bat - Issues the commands to set the Env vars and start the build.   Called by CCKBuild.pl.

ReadMe.txt - Um, uh, well....  DUH!

WizardMachine.mak - Make file for WizardMachine.  Details below.....

WizardMachine.dep - The dependancy file for WizardMachine.mak.  Put both WizardMachine.mak 
and WizardMachine.dep in the mozilla/cck/driver folder to build the WizardMachine project(They
should already be there). 
To build this project issue the commands:

NMAKE /f "WizardMachine.mak" CFG="WizardMachine - Win32 Debug"
or
NMAKE /f "WizardMachine.mak" CFG="WizardMachine - Win32 Release"

The commands above should be executed in the same folder as the WizardMachine.mak and .dep 
files. When complete, you should end up with nice shiny new .exe, .obj's, .pch and .res files in a
"release" or "debug" folder, depending on the command issued from above.





Doc Owner:

Frank (petitta@netscape.com)
X6378