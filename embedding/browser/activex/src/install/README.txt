Instructions for producing the ActiveX control installer

1. Build Mozilla
2. Build the embedding dist
3. Grab the latest Nullsoft 2.0+ Installer from http://nsis.sourceforge.net/
4. Run build.pl to create a file manifest and local settings and generate the
   installer.

Notes.

You need NSIS 2.0. I used NSIS 2.0b3 but others will probably do as well.

The build script will evolve but presently it assumes the dist dir is $MOZ_SRC\dist\Embed and there
are a number of manual steps that NEED to be done.

1. Edit control.nsi to set the version information and paths at the top
2. Copy client-win into embedding/config
3. Run make in embedding/config
4. Run build.pl

Other things todo:

1. Code sign installer
2. Add MPL / GPL licence to installer
3. Add readme
4. Add sample apps for C++, VB & C#

