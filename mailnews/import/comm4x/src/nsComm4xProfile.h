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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef nsComm4xProfile_h__
#define nsComm4xProfile_h__

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsIComm4xProfile.h"
#include "nsXPIDLString.h"
#include "nsString.h"

/* c66c1060-2bdc-11d6-92a0-0010a4b26cda */
#define NS_ICOMM4XPROFILE_CID \
 { 0xc66c1060, 0x2bdc, 0x11d6, {0x92, 0xa0, 0x0, 0x10, 0xa4, 0xb2, 0x6c, 0xda} }
#define NS_ICOMM4XPROFILE_CONTRACTID    "@mozilla.org/comm4xProfile;1"
#define NS_ICOMM4XPROFILE_CLASSNAME "Communicator 4.x Profile"

class nsComm4xProfile : public nsIComm4xProfile
{
public:
    nsComm4xProfile();
    virtual ~nsComm4xProfile();
    nsresult Get4xProfileInfo(const char *registryName);
    nsresult GetPrefValue(nsILocalFile *filePath, const char * prefName, const char * prefEnd, PRUnichar ** value);

    NS_DECL_ISUPPORTS
    NS_DECL_NSICOMM4XPROFILE

};

#endif /* nsComm4xProfile_h__ */

