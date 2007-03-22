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

The Original Code is mozilla.org code.

The Initial Developer of the Original Code is
Netscape Communications Corporation.
Portions created by the Initial Developer are Copyright (C) 2___
the Initial Developer. All Rights Reserved.

Contributor(s):
        Chak Nanga <chak@netscape.com> 

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

This sample shows how to embed Mozilla from within 
an MFC Windows application

Mainly demonstrates the use of the following interfaces:

	nsIWebBrowserChrome
	nsIEmbeddingSiteWindow
	nsIWebProgressListener
	nsIContextMenuListener
	nsIPrompt
	nsIWebBrowserFind
	
General Overview:
-----------------

1. The MfcEmbedApp creates BrowserFrames

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
   

Start by first looking at MfcEmbed.cpp

Files:

StdAfx.h	 
	- Includes the required Mozilla header files

MfcEmbed.cpp 
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
	optional)

	- Calls on the statusbar/progressbar update functions exposed
	via the IBrowserFrameGlue in response to the nsIProgressListener
	interface callbacks

Dialogs.cpp
	- Contains dialog box code for displaying Prompts, getting
	passwords, getting username/passwords etc
	- Contains the CFindDialog class - used for searching text
	in a web page

winEmbedFileLocProvider.cpp, ProfilesDlg.cpp, ProfileMgr.cpp
	- Profile management related code (by Conrad Carlen)

MfcEmbed.htm
	- This is a simple test harness for excercising some of the
	implemented interfaces . For ex, the nsIWebBrowserChrome.
	- Open the file in mfcemed by typing the following in it's
	location bar. For ex: 

			file:///c:/tmp/mfcembed.htm
	- Read/Click on the links on that page more info
	- This test page is just a start and will add more test
	case to it over time

 
makefile.win
	- We define "_AFXDLL" and for the compiler and specify
	"-SUBSYSTEM:windows" for the linker using LCFLAGS and 
	LLFLAGS, respectively
	- We also define "USE_SINGLE_SIGN_ON" to enable the
	single sign-on support

mfcembed.dsp and mfcembed.dsw
	- These VisualStudio workspace/project files can be used
	to open/build this sample inside of the VC++ IDE
