/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla
 *
 * The Initial Developer of the Original Code is 
 # Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *           Rajiv Dayal <rdayal@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MSG_PALMSYNC_SUPPORT_H_
#define MSG_PALMSYNC_SUPPORT_H_

#include "nsIObserver.h"
#include "nsIPalmSyncSupport.h"
#include "PalmSyncFactory.h"

// {44812571-CE84-11d6-B8A6-00B0D06E5F27}
#define NS_IPALMSYNCSUPPORT_CID \
  { 0x44812571, 0xce84, 0x11d6, \
    { 0xb8, 0xa6, 0x0, 0xb0, 0xd0, 0x6e, 0x5f, 0x27 } }


class nsPalmSyncSupport : public nsIPalmSyncSupport,
                      public nsIObserver
{
    public :
        nsPalmSyncSupport();
        ~nsPalmSyncSupport();

        // Declare all interface methods we must implement.
        NS_DECL_ISUPPORTS
        NS_DECL_NSIOBSERVER
        NS_DECL_NSIPALMSYNCSUPPORT

    private :

        DWORD   m_dwRegister;
        CPalmSyncFactory *m_nsPalmSyncFactory;
};

#endif  // MSG_PALMSYNC_SUPPORT_H_
