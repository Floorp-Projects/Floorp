***** BEGIN LICENSE BLOCK *****
Version: MPL 1.1/GPL 2.0/LGPL 2.1

The contents of this file are subject to the Mozilla Public License Version 
1.1 (the "License"); you may not use this file except in compliance with 
the License. You may obtain a copy of the License at 
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS" basis,
WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
for the specific language governing rights and limitations under the
License.

The Original Code is is mozilla.org code.

The Initial Developer of the Original Code is 
Netscape Communications Corporation.
Portions created by the Initial Developer are Copyright (C) 1998
the Initial Developer. All Rights Reserved.

Contributor(s):
  Chak Nanga <chak@netscape.com>
  David Epstein <depstein@netscape.com>

Alternatively, the contents of this file may be used under the terms of
either the GNU General Public License Version 2 or later (the "GPL"), or
the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
in which case the provisions of the GPL or the LGPL are applicable instead
of those above. If you wish to allow use of your version of this file only
under the terms of either the GPL or the LGPL, and not to allow others to
use your version of this file under the terms of the MPL, indicate your
decision by deleting the provisions above and replace them with the notice
and other provisions required by the GPL or the LGPL. If you do not delete
the provisions above, a recipient may use your version of this file under
the terms of any one of the MPL, the GPL or the LGPL.

***** END LICENSE BLOCK *****

TestEmbed is a C++ test app based upon mfcEmbed. It includes
individual test cases for Embedding interface methods. Interfaces
covered include nsIFile & nsILocalFile, nsISHistory, nsIWebNavigation,
nsIWebProgressListener, nsIClipboardCommands, nsIRequest, nsIDOMWindow,
nsIDirectoryService, nsIProfile, nsIObserverService, nsIWebBrowser,
nsIWebProgress, nsIWebBrowserFind, nsIEditingSession, nsICommandManager,
nsICommandParams, nsIStreamListener, nsIURIContentListener,
nsISHistoryListener, nsIWebBrowserChrome, and nsIEmbeddingSiteWindow.
.
Individual tests also exist for methods found in nsIGlobalHistory
and nsIProfile.

In addition to the mfcEmbed menus, it contains 5 menus: Tests,
Interfaces, Tools, Verified Bugs, and Clipboard Commands.

Executed test cases print output to a logfile called TestOutput.txt.
This will be created in the C:\temp folder.

See build instructions below.
	
General Overview:
-----------------

1. The TestEmbedApp creates BrowserFrames

2. BrowserFrame creates the toolbar, statusbar, URLbar 
   BrowserView etc.
   BrowserFrames implement the IBrowserFrameGlue interface
   using which new BrowserFrames can be created, statusbar
   updated etc. 

3. BrowserView creates the embeddable browser instance and
   manages user interaction such as URL navigation etc.
   BrowserView connects the BrowserImpl with the BrowserFrame
   via the IBrowserFrameGlue interface

4. BrowserImpl implements the set of required/optional Gecko
   embedding interfaces
   

Start by first looking at TestEmbed.cpp

Files:

StdAfx.h	 
	- Includes the required Mozilla header files

TestEmbed.cpp 
	- CWinApp derived class
	- Creates the browser's main frame window etc
	  using the CreateNewBrowserFrame() and loads
	  the HomePage
	- Makes the required NS_InitEmbedding() and the
	  NS_TermEmbedding() calls in the app's InitInstance()
	  and ExitInstance() functions
	- Keeps track of the list of new BrowserFrames created
	  which it cleans up properly in ExitInstance()

BrowserFrm.cpp 
	- This is the browser's Frame window i.e. the one with the
	  "chrome" - with the toolbar, urlbar etc.
	- Creates the toolbar, URLbar, statusbar, progressbar 
	  also the browser's view window. 

BrowserFrameGlue.cpp
	- Implements the IBrowserFrameGlue interface. This interface
	  contains platform specific implementations of functions to
	  update the statusbar, creating new browser frames etc. Embedded
	  browser's callbacks use this interface when needed

BrowserView.cpp 
	- Creates the embedded browser object and handles most aspects
	of the embedded browser interactions - like URL navigation,
	clipboard interactions etc
	- Also has code to keep the menu/toolbar/statusbar UI items up 
	to date
	- It's the view which conntects the BrowserFrame to the BrowserImpl
	  (see below) by passing it the pointer to IBrowserFrameGlue

BrowserImpl*.cpp 
	- Implements the required and/or optional embedded browser 
	interfaces
	(BrowserImpl.cpp implements the set of interfaces which 
	are required by Gecko of all embedding apps. The other
	interfaces implemented in the BrowserImpl*.cpp files are 
	optional. Included in the BrowserImpl.cpp file are interface tests for 					nsIWebBrowserChrome,nsIEmbeddingSiteWindow, nsIStreamListener, and 				nsIURIContentListener.)

	- Calls on the statusbar/progressbar update functions exposed
	via the IBrowserFrameGlue in response to the nsIWebProgressListener
	interface callbacks. nsIWebProgressListener methods are implemented
	in BrowserImplWebPrgrsLstnr.cpp. nsISHistoryListener methods are
	implemented in BrowserImplHistoryLstnr.cpp

Dialogs.cpp
	- Contains dialog box code for displaying Prompts, getting
	passwords, getting username/passwords etc
	- Contains the CFindDialog class - used for searching text
	in a web page

winEmbedFileLocProvider.cpp, ProfilesDlg.cpp, ProfileMgr.cpp
	- Profile management related code (by Conrad Carlen)

Tests.cpp
	- This is where individual test cases are stored and test interfaces are
	registered. The file is divided into three sections: 1) individual test 
	cases corresponding to the "Tests" menu. 2) tools that are helpful,
	corresponding to the "Tools" menu. These include routines like
	removing all entries from Global History. 3) Registration of
	interface tests with CTests object (bottom part of the file). Example: 

	void CTests::OnInterfacesNsidomwindow()
	{
		CDomWindow oDomWindow(qaWebBrowser) ;
		oDomWindow.OnStartTests(nCommandID);
	}

	The interface tests OnInterfacesNsidomwindow() is associated with menu
	handlers in the message map (in top part of the file). For this example,
	ON_COMMAND(ID_INTERFACES_NSIDOMWINDOW_RUNALLTESTS, OnInterfacesNsidomwindow).
	The test interface object (oDomWindow) is created with appropriate constructor.
	Then the object calls the test interface's OnStartTests() method and passes
	the selected command ID.


QAUtils.cpp
	- This contains routines that are useful to QA. Such tasks as
	printing to a logfile, displaying messages to the screen, formatting
	output data, retrieving the name of an nsI request, and getting a urI 
	are stored here.

makefile.in
	- We define "_AFXDLL" and for the compiler and specify
	"-SUBSYSTEM:windows" for the linker using LCFLAGS and 
	LLFLAGS, respectively
	- We also define "USE_SINGLE_SIGN_ON" to enable the
	single sign-on support
	- Also need REQUIRES for all modules to be included
	- Place all .cpp files for compiling in CPPSRCS section.

testembed.dsp and testembed.dsw
	- These VisualStudio workspace/project files can be used
	to open/build this sample inside of the VC++ IDE

Most interface tests have their own .cpp & .h files (e.g. nsIWebBrow.cpp).

Instructions for building:
	1) Open a dos shell.
	2) cd /mozilla/embedding/qa/testembed	   // testEmbed directory
	3) Copy the makefile.in file and call it makefile. Place in same directory.
	4) Change the top few lines in 'makefile' to indicate local pathway. Example:
		DEPTH		= ../../..
		topsrcdir	= d:/mozilla_src/mozilla
		srcdir		= d:/mozilla_src/mozilla/embedding/qa/testembed
		VPATH		= d:/mozilla_src/mozilla/embedding/qa/testembed
	5) cd components			  // components directory
	6) Copy the makefile.in file and call it makefile. Place in same directory.
	7) Change the top few lines in 'makefile' to indicate local pathway. Example:
		DEPTH		= ../../../..
		topsrcdir    = d:/mozilla_src/mozilla
		srcdir       = d:/mozilla_src/mozilla/embedding/qa/testembed/components
		VPATH        = d:/mozilla_src/mozilla/embedding/qa/testembed/components
	8) make				   	// compile 'makefile.in' in components
	9) cd ..				// return to testembed folder
	10) make				// compile 'makefile.in' in testembed
	11) cd ../../../dist/bin/	   	// navigate to the bin dir
	12) testembed.exe			// run testEmbed.exe

	note: if 'make' doesn't work above, try 'make -f makefile.in'

A few suggestions:
	1) Don't run nsIWebNavigation tests after turning on the Web
	Progress listener (from Tests menu). This will create all types
	of listener msgs in your logfile and display many msgs on the 
	screen.
	2) Best way to use web progress listener is to turn it on (from 
	the Tests menu)then change URL . Change a few more urls and
	monitor the output.
	3) Before running nsISHistory interface tests, load 1-2 urls.
	That will create a session history. The same applies for the
	nsIWebNavigation interfaces tests. One loaded url will enable
	back/forward navigation.
	4) nsIUriContentListener is registered from the "Tests" >
	"Add urIContentListener" menu. There are submenu items for different options. 
