
You are reading: README.TXT
---------------------------

This file gives an overview of CCK Wizard for Win32

It includes the following:
  Conditions for use of CCK Wizard for Win32
  System Requirements
  Brief description of CCK Wizard and architecture
  Listing of files in this Kit
  How to build and run the Wizard
  Where to get more information


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


Where to get more information
===============================


