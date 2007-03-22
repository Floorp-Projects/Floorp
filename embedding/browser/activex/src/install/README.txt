Instructions for producing the ActiveX control installer

1. Set your MOZCONFIG to point to the mozconfig.txt here
2. Build Mozilla
4. Optionally cd into mozilla/embedding/browser/activex/src/plugin and
   "make MOZ_USE_ACTIVEX_PLUGIN=1"
3. Copy client-win from here to mozilla/embedding/config
5. Build the embedding dist
6. Grab the latest Nullsoft 2.0+ Installer from http://nsis.sourceforge.net/
7. Edit control.nsi to set the version information and paths at the top
9. Run build.pl to create a file manifest and local settings and generate the
   installer.

Notes.

You need NSIS 2.0. I used NSIS 2.0b3 but others will probably do as well.

The build script will evolve but presently it assumes the dist dir is $MOZ_SRC\dist\Embed and there
are a number of manual steps that NEED to be done.

Other things todo:

1. Code sign installer
2. Add MPL / GPL licence to installer
3. Add readme
4. Add sample apps for C++, VB & C#
5. Automatically grab version number from dist\include\mozilla-config.h
6. Automatically run all the steps at the top of this file
