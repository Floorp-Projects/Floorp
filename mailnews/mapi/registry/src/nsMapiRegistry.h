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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Srilatha Moturi <srilatha@netscape.com>
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

#ifndef nsmapiregistry_h____
#define nsmapiregistry_h____

#include "nsIMapiRegistry.h"

#ifndef MAX_BUF
#define MAX_BUF 4096
#endif

/* c5be14ba-4e0a-4eec-a1b8-04363761d63c */
#define NS_IMAPIREGISTRY_CID \
 { 0xc5be14ba, 0x4e0a, 0x4eec, {0xa1, 0xb8, 0x04, 0x36, 0x37, 0x61, 0xd6, 0x3c} }
#define NS_IMAPIREGISTRY_CONTRACTID    "@mozilla.org/mapiregistry;1"
#define NS_IMAPIREGISTRY_CLASSNAME "Mozilla MAPI Registry"

class nsMapiRegistry : public nsIMapiRegistry {
public:
    // ctor/dtor
    nsMapiRegistry();
    virtual ~nsMapiRegistry();

    // Declare all interface methods we must implement.
    NS_DECL_ISUPPORTS
    NS_DECL_NSIMAPIREGISTRY

protected:
    
    PRBool m_DefaultMailClient;
    PRBool m_ShowDialog;

private:
    // Special member to handle initialization.
    PRBool mHaveBeenSet;
}; // nsMapiRegistry

#endif // nsmapiregistry_h____
