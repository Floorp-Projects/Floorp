 ====================================================================
 Netscape JavaScript Debugger 1.1
 ====================================================================
 
 Netscape JavaScript Debugger is subject to the terms detailed in the
 license agreement accompanying it. 

 "Getting Started with the JavaScript Debugger" describes how to use
 the debugger. This document is available online at the following URL:
 
      http://developer.netscape.com/library/documentation/jsdebug/index.htm 

 In addition, a PDF version of the document is available for download
 at the following URL:
 
      http://developer.netscape.com/library/documentation/jsdebug/jsd.pdf 

 Release Notes (detailing known problems and workarounds) are
 available online at the following URL:
 
      http://home.netscape.com/eng/Tools/JSDebugger/relnotes/relnotes11.html
 
 
 ====================================================================
 System Requirements
 ====================================================================
 
 Netscape JavaScript Debugger is available on the following operating
 systems: 
       Windows 95/NT
       Macintosh PowerPC
       Solaris 2.4, 2.5, 2.6
       SunOS 4.1.3
       Irix 5.3, 6.2, 6.3

 The recommended memory configurations are as follows:
       Windows 95/NT: 16MB 
       Macintosh PPC: 16MB 
       Unix:          64MB
 
 
 ====================================================================
 Installation Requirements
 ====================================================================
 
 * Before installing Netscape JavaScript Debugger, you must install
   and run Netscape Communicator version 4.02 or later.
 
 * For UNIX users: Before installing Netscape JavaScript Debugger, you
   must set the following environment variables and restart Communicator:

   MOZILLA_FIVE_HOME
   Set to the directory in which Communicator is installed. (You may
   have done this when installing Communicator.)  If you installed
   Communicator in /communicator_installation_directory, you could use
   a call such as:

      setenv  MOZILLA_FIVE_HOME /communicator_installation_directory

   LD_LIBRARY_PATH
   Add MOZILLA_FIVE_HOME to this path. If you already have this variable
   set, you can add MOZILLA_FIVE_HOME with a call such as the following:

      setenv LD_LIBRARY_PATH ${LD_LIBRARY_PATH}:${MOZILLA_FIVE_HOME}

   If you do not already have this variable set, you can set it with a
   call such as the following:

      setenv LD_LIBRARY_PATH ${MOZILLA_FIVE_HOME}

 * IMPORTANT NOTE to Visual JavaScript PR1 users: Due to a CLASSPATH
   conflict, you must uninstall Visual JavaScript PR1 *before*
   starting Netscape JavaScript Debugger. 
 
   If you do not do this, the debugger displays a message box 
   notifying you to uninstall Visual JavaScript PR1, and then quits.
 
   To uninstall an application under Windows 95/NT:
   
   1. Click the Start menu and choose Settings|Control Panel. 
 
   2. Open the Add/Remove Programs control panel and specify Visual 
      JavaScript 1.0 PR1.

 ====================================================================
 What Gets Installed
 ====================================================================
 
 Netscape JavaScript Debugger is installed using the SoftUpdate
 mechanism. By default, the debugger files are placed where you
 installed Netscape Communicator. For example, assume you installed
 Communicator for Win95/NT in:

       c:\program files\netscape\communicator\program

 In this case, the debugger files would be installed in:

       c:\program files\netscape\communicator\program\jsdebug
 
 The following files and directories are installed in the jsdebug
 directory or folder:
 
      jsdebugger.html (open this page in Navigator to start the debugger)
      jsdeb11.jar 
      sounds\*.au 
      images\*.gif 
      samples\*.*
 
 ====================================================================
 What To Do First
 ====================================================================
 
 The first time you run the debugger, Navigator displays several
 security dialogs that describe special privileges that the debugger
 requires to run. You *must* grant those privileges in order for the
 debugger to function.
 
 NOTE: To prevent these dialogs from appearing the next time you run 
 the debugger, check the "Remember this decision" box.
 
 For convenience, you should add the Debugger location to your bookmarks,
 or to your personal toolbar in Navigator. 
 
 ====================================================================
 	Copyright (c) 1997, Netscape Communications Corporation
