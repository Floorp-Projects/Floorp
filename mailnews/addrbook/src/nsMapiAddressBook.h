/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Created by Cyrille Moureaux <Cyrille.Moureaux@sun.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsMapiAddressBook_h___
#define nsMapiAddressBook_h___

#include "nsAbWinHelper.h"
 
class nsMapiAddressBook : public nsAbWinHelper
{
public :
    nsMapiAddressBook(void) ;
    virtual ~nsMapiAddressBook(void) ;

protected :
    // Class members to handle the library/entry points
    static HMODULE mLibrary ;
    static PRInt32 mLibUsage ;
    static LPMAPIINITIALIZE mMAPIInitialize ;
    static LPMAPIUNINITIALIZE mMAPIUninitialize ;
    static LPMAPIALLOCATEBUFFER mMAPIAllocateBuffer ;
    static LPMAPIFREEBUFFER mMAPIFreeBuffer ;
    static LPMAPILOGONEX mMAPILogonEx ;
    // Shared session and address book used by all instances.
    // For reasons best left unknown, MAPI doesn't seem to like
    // having different threads playing with supposedly different
    // sessions and address books. They ll end up fighting over
    // the same resources, with hangups and GPF resulting. Not nice.
    // So it seems that if everybody (as long as some client is 
    // still alive) is using the same sessions and address books,
    // MAPI feels better. And who are we to get in the way of MAPI
    // happiness? Thus the following class members:
    static BOOL mInitialized ;
    static BOOL mLogonDone ;
    static LPMAPISESSION mRootSession ;
    static LPADRBOOK mRootBook ;

    // Load the MAPI environment
    BOOL Initialize(void) ;
    // Allocation of a buffer for transmission to interfaces
    virtual void AllocateBuffer(ULONG aByteCount, LPVOID *aBuffer) ;
    // Destruction of a buffer provided by the interfaces
    virtual void FreeBuffer(LPVOID aBuffer) ;
    // Library management 
    static BOOL LoadMapiLibrary(void) ;
    static void FreeMapiLibrary(void) ;

private :
} ;

#endif // nsMapiAddressBook_h___

