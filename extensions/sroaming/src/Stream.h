/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/* Uploads/downloads files using HTTP or FTP */

/* This implementation uses Necko to transfer files to/from the server.
   It should be able to support any protocol which has an nsIUploadChannel
   implementation in Necko, after some tweaks (errors msgs and workarounds)
   to the JS part (resources/content/transfer/transfer.js, utility.js and
   transfer.properties). Currently, HTTP and FTP are supported.
*/

#ifndef _STREAM_H_
#define _STREAM_H_

#include "Protocol.h"

class Stream: public Protocol
{
public:
    nsresult Init(Core* controller);
    nsresult Download();
    nsresult Upload();
    nsresult DownUpLoad(PRBool download);

protected:
    // Data
    /**
     * Directory on server, where profile files are.
     * This is already with the concrete username.
     */
    nsCString mRemoteBaseUrl;  
    nsString mPassword;
    /**
     * If we have to ask for a password, this is
     * the default for the "save password" checkbox
     */
    PRBool mSavePassword;     
    nsCOMPtr<nsIURI> mProfileDir;
    nsRegistryKey mRegkeyStream;

protected:
};

#endif
