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
 * Netscape Communications Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *       Rajiv Dayal <rdayal@netscape.com>
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

#ifndef _MOZABCONDUIT_SYNC_H_
#define _MOZABCONDUIT_SYNC_H_

#include <Cpalmrec.h>
#include <Pgenerr.h>

#include "MozABHHManager.h"
#include "MozABPCManager.h"

class CMozABConduitSync
{
public:
    // OpenConduit creates a new object using this constructor
	CMozABConduitSync(CSyncProperties& rProps);
    // destructor	
    ~CMozABConduitSync();

    // OpenConduit calls this function to begin the sync process
    long Perform();

    friend DWORD WINAPI DoFastSync(LPVOID lpParameter);

protected:
    // default constructor
    CMozABConduitSync();
    
    // initialization method
    long GetRemoteDBInfo(int iIndex);

    // synchronization methods
    long PerformFastSync();
    long PerformSlowSync();
    long CopyHHtoPC();
    long CopyPCtoHH();

private:
    CSyncProperties m_rSyncProperties;

    MozABHHManager *m_dbHH;
    MozABPCManager *m_dbPC;
    CDbList        *m_remoteDB; 
    short           m_TotRemoteDBs;
    CSystemInfo     m_SystemInfo;
    CONDHANDLE      m_ConduitHandle;
};

#endif
