/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/*
 * This file is part of the Java-vendor-neutral implementation of LiveConnect
 *
 * It contains the public XP-COM based interface for java to javascript communication.
 * This interface allows java and javascript to exchange security contexts to allow
 * access to restricted resources on either side.
 *
 */

#ifndef nsISecurityContext_h___
#define nsISecurityContext_h___

#include "nsISupports.h"

class nsISecurityContext : public nsISupports {
public:
    /**
     * Get the security context to be used in LiveConnect.
     * This is used for JavaScript <--> Java.
     *
     * @param target        -- Possible target.
     * @param action        -- Possible action on the target.
     * @return              -- NS_OK if the target and action is permitted on the security context.
     *                      -- NS_FALSE otherwise.
     */
    NS_IMETHOD Implies(const char* target, const char* action, PRBool *bAllowedAccess) = 0;
};


// Supported targets in Netscape Navigator 4.0 (Communicator): 

#define nsSecurityTarget_UniversalThreadAccess "UniversalThreadAccess"       // allows manipulation of threads not belonging to the applet  
#define nsSecurityTarget_UniversalExecAccess "UniversalExecAccess"           // allows executing sub-processes  
#define nsSecurityTarget_UniversalExitAccess "UniversalExitAccess"           // allows terminating the browser session  
#define nsSecurityTarget_UniversalLinkAccess "UniversalLinkAccess"           // allows linking to dymanically loaded libraries (DLLs)  
#define nsSecurityTarget_UniversalPropertyWrite "UniversalPropertyWrite"     // allows reading and writing of all system properties (vs. restrictions on applets)  
#define nsSecurityTarget_UniversalPropertyRead "UniversalPropertyRead"       // allows reading of all system properties (vs. restrictions on applets)  
#define nsSecurityTarget_UniversalFileRead "UniversalFileRead"               // allows reading any file in the local filesystem  
#define nsSecurityTarget_UniversalFileWrite "UniversalFileWrite"             // allows writing any file in the local filesystem  
#define nsSecurityTarget_UniversalFileDelete "UniversalFileDelete"           // allows deleting of any file in the local filesystem  
#define nsSecurityTarget_UniversalFdRead "UniversalFdRead"                   // allows reading from any arbitrary file descriptor  
#define nsSecurityTarget_UniversalFdWrite "UniversalFdWrite"                 // allows writing to any arbitrary file descriptor  
#define nsSecurityTarget_UniversalListen "UniversalListen"                   // allows establishing the server-side of a network connection  
#define nsSecurityTarget_UniversalAccept "UniversalAccept"                   // allows waiting on a network connection  
#define nsSecurityTarget_UniversalConnect "UniversalConnect"                 // allows establishing the client-side of a network connection  
#define nsSecurityTarget_UniversalMulticast "UniversalMulticast"             // allows establishing IP multicast a network connection  
#define nsSecurityTarget_UniversalTopLevelWindow "UniversalTopLevelWindow"   // allows top-level windows to be created by the applet writer.  
#define nsSecurityTarget_UniversalPackageAccess "UniversalPackageAccess"     // allows access to java packages  
#define nsSecurityTarget_UniversalPackageDefinition "UniversalPackageDefinition" // allows access to define packages  
#define nsSecurityTarget_UniversalSetFactory "UniversalSetFactory"           // allows access to set a networking-related object factory  
#define nsSecurityTarget_UniversalMemberAccess "UniversalMemberAccess"       // allows access to members of a class  
#define nsSecurityTarget_UniversalPrintJobAccess "UniversalPrintJobAccess"   // allows access to initiate a print job request  
#define nsSecurityTarget_UniversalSystemClipboardAccess "UniversalSystemClipboardAccess" // allows access to System Clipboard  
#define nsSecurityTarget_UniversalAwtEventQueueAccess "UniversalAwtEventQueueAccess"     // allows access to Awt's EventQueue  
#define nsSecurityTarget_UniversalSecurityProvider "UniversalSecurityProvider" // allows access to certain operations to a given provider, for example, only a given provider (e.g. Netscape) is able to retrieve the Netscape provider properties  
#define nsSecurityTarget_UniversalBrowserRead "UniversalBrowserRead"           // allows access to browser data  
#define nsSecurityTarget_UniversalBrowserWrite "UniversalBrowserWrite"         // allows modification of browser data  
#define nsSecurityTarget_UniversalSendMail "UniversalSendMail"              // allows sending mail  
#define nsSecurityTarget_SuperUser "SuperUser"                              // enables all privileges  
#define nsSecurityTarget_30Capabilities "30Capabilities"                    // enables all privileges that are available in Navigator 3.0.  
#define nsSecurityTarget_UniversalFileAccess "UniversalFileAccess"          // enables read, write and delete of any file in the local filesystem  
#define nsSecurityTarget_TerminalEmulator "TerminalEmulator"                // enables socket connections, property read and to link dynamic libraries.  


#define NS_ISECURITYCONTEXT_IID                          \
{ /* {209B1120-4C41-11d2-A1CB-00805F8F694D} */         \
    0x209b1120,                                      \
    0x4c41,                                          \
    0x11d2,                                          \
    { 0xa1, 0xcb, 0x0, 0x80, 0x5f, 0x8f, 0x69, 0x4d } \
}

#endif // nsISecurityContext_h___
