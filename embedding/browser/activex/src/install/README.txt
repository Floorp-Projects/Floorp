Instructions for producing the ActiveX control installer

1. Build Mozilla
2. Build the embedding dist
3. Grab the latest Nullsoft 2.0+ Installer from http://nsis.sourceforge.net/
4. Run build.pl to create a file maninfest and local settings
5. Run MakeNSISW over control.nsi

Notes.

You need NSIS 2.0. I used NSIS 2.0b3 but others will probably do as well.

The script will evolve but presently it assumes the dist dir is $MOZ_SRC\dist\Embed.

