
You are reading: README.TXT
---------------------------

This file gives an overview of CCK Wizard for Win32

It includes the following:
  Conditions for use of CCK Wizard for Win32
  System Requirements
  Brief description of CCK Wizard and architecture
  Listing of files in this Kit
  How to build and run the Wizard
  Info-ZIP License


System requirements
===================
To run CCK Wizard for Win32, you will need a computer running Windows NT
Workstation 3.5 or later, Windows 95 or Windows 98.

CCK Wizard for Win32 has been built using Microsoft Visual C++ version 4.2.
The source release of CCK Wizard includes the source files, inifiles and bitmaps.
Currently there is no general makefile to build the source, so you will need to
have VC++ 4.2 or greater installed on your machine.

CCK Wizard does not assume that you have Communicator installed on your machine.


Brief description of CCK Wizard and architecture
================================================
CCK Wizard allows users to create customized versions of Netscape Communicator. The
application has a main driver routine, called WizardMachine, that runs it. The UI
of the application is dynamically generated from iniFiles. The WizardMachine builds 
wizard pages based on the iniFiles it receives. 

The UI presents the most common customizations that users usually make to Communicator.
If the user wishes to make extended customizations, they can do so by changing the
iniFiles. As the application is completely iniFile driven, it can pick up any further
modifications added to the iniFiles.

The hierarchy of the directories and files are listed below. Understanding this structure 
is important to run WizardMachine with a given set of iniFiles.


Listing of files in this Kit
============================
mozilla/cck
    	    |
	      aswiz
    	    | 
	      cckwiz 
		    |
		      ase
                          |
			    NCIFiles
		    |
		      bitmaps
		    |
  		      ConfigEditor
		    |
		      customizations
		    |
  		      docs
		    |
		      iniFiles
		    |
  		      InstallBuilder
		    |
		      shell	
	    |
	      docs
            |
              driver (Wizard Machine code)
		    |
		      res
	    |
              muc 	


This document explains the details of things required and need to be done in order to run 
CCK Wizard. 


How to build and run the Wizard
===============================
cvs co mozilla/cck
Change dir into cck/driver
Open WizardMachine.mdp in VC++ 4.2 or greater.
(If VC++ asks that it will convert the mdp file into a newer version, go ahead)
Build the application.

To run the application from VC++:
Give -i cckwiz\iniFiles\cck.ini as program arguments in the settings.

To run the application from command prompt:
WizardMachine -i cckwiz\iniFiles\cck.ini


Info-ZIP License
=============================

This is version 2000-Apr-09 of the Info-ZIP copyright and license.
The definitive version of this document should be available at
ftp://ftp.info-zip.org/pub/infozip/license.html indefinitely.


Copyright (c) 1990-2000 Info-ZIP.  All rights reserved.

For the purposes of this copyright and license, "Info-ZIP" is defined as
the following set of individuals:

   Mark Adler, John Bush, Karl Davis, Harald Denker, Jean-Michel Dubois,
   Jean-loup Gailly, Hunter Goatley, Ian Gorman, Chris Herborth, Dirk Haase,
   Greg Hartwig, Robert Heath, Jonathan Hudson, Paul Kienitz, David Kirschbaum,
   Johnny Lee, Onno van der Linden, Igor Mandrichenko, Steve P. Miller,
   Sergio Monesi, Keith Owens, George Petrov, Greg Roelofs, Kai Uwe Rommel,
   Steve Salisbury, Dave Smith, Christian Spieler, Antoine Verheijen,
   Paul von Behren, Rich Wales, Mike White

This software is provided "as is," without warranty of any kind, express
or implied.  In no event shall Info-ZIP or its contributors be held liable
for any direct, indirect, incidental, special or consequential damages
arising out of the use of or inability to use this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. Redistributions of source code must retain the above copyright notice,
       definition, disclaimer, and this list of conditions.

    2. Redistributions in binary form must reproduce the above copyright
       notice, definition, disclaimer, and this list of conditions in
       documentation and/or other materials provided with the distribution.

    3. Altered versions--including, but not limited to, ports to new operating
       systems, existing ports with new graphical interfaces, and dynamic,
       shared, or static library versions--must be plainly marked as such
       and must not be misrepresented as being the original source.  Such
       altered versions also must not be misrepresented as being Info-ZIP
       releases--including, but not limited to, labeling of the altered
       versions with the names "Info-ZIP" (or any variation thereof, including,
       but not limited to, different capitalizations), "Pocket UnZip," "WiZ"
       or "MacZip" without the explicit permission of Info-ZIP.  Such altered
       versions are further prohibited from misrepresentative use of the
       Zip-Bugs or Info-ZIP e-mail addresses or of the Info-ZIP URL(s).

    4. Info-ZIP retains the right to use the names "Info-ZIP," "Zip," "UnZip,"
       "WiZ," "Pocket UnZip," "Pocket Zip," and "MacZip" for its own source and
       binary releases.


