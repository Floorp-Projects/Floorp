/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *  Conrad Carlen <ccarlen@netscape.com>
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

#ifndef __CBrowserShellMsgDefs__
#define __CBrowserShellMsgDefs__

#pragma once

// Messages sent by CBrowserShell

#ifndef EMBED_MSG_BASE_ID
#define EMBED_MSG_BASE_ID 1000
#endif

enum {
    msg_OnNetStartChange      = EMBED_MSG_BASE_ID + 0,
    msg_OnNetStopChange 	  = EMBED_MSG_BASE_ID + 1,
    msg_OnProgressChange      = EMBED_MSG_BASE_ID + 2,
    msg_OnLocationChange      = EMBED_MSG_BASE_ID + 3,
    msg_OnStatusChange        = EMBED_MSG_BASE_ID + 4,
    msg_OnSecurityChange      = EMBED_MSG_BASE_ID + 5,
    
    msg_OnChromeStatusChange  = EMBED_MSG_BASE_ID + 6
};


/**
 *  CBrowserShell and CBrowserChrome broadcast changes using LBroadcaster::BroadcastMessage()
 *  A pointer to one of the following types is passed as the ioParam.
 */

// msg_OnNetStartChange
struct MsgNetStartInfo
{
    MsgNetStartInfo(CBrowserShell* broadcaster) :
        mBroadcaster(broadcaster)
        { }
    
    CBrowserShell *mBroadcaster;      
};

// msg_OnNetStopChange
struct MsgNetStopInfo
{
    MsgNetStopInfo(CBrowserShell* broadcaster) :
        mBroadcaster(broadcaster)
        { }
    
    CBrowserShell *mBroadcaster;      
};

// msg_OnProgressChange
struct MsgOnProgressChangeInfo
{
    MsgOnProgressChangeInfo(CBrowserShell* broadcaster, PRInt32 curProgress, PRInt32 maxProgress) :
        mBroadcaster(broadcaster), mCurProgress(curProgress), mMaxProgress(maxProgress)
        { }
    
    CBrowserShell *mBroadcaster;      
    PRInt32 mCurProgress, mMaxProgress;
};

// msg_OnLocationChange
struct MsgLocationChangeInfo
{
    MsgLocationChangeInfo(CBrowserShell* broadcaster,
                          const char* urlSpec) :
        mBroadcaster(broadcaster), mURLSpec(urlSpec)
        { }
    
    CBrowserShell *mBroadcaster;
    const char *mURLSpec;     
};

// msg_OnStatusChange
struct MsgStatusChangeInfo
{
    MsgStatusChangeInfo(CBrowserShell* broadcaster,
                        nsresult status, const PRUnichar *message) :
        mBroadcaster(broadcaster),
        mStatus(status), mMessage(message)
        { }
    
    CBrowserShell *mBroadcaster;
    nsresult mStatus;
    const PRUnichar *mMessage;     
};

// msg_OnSecurityChange
struct MsgSecurityChangeInfo
{
    MsgSecurityChangeInfo(CBrowserShell* broadcaster,
                          PRUint32 state) :
        mBroadcaster(broadcaster),
        mState(state)
        { }
    
    CBrowserShell *mBroadcaster;
    PRUint32 mState;
}; 

// msg_OnChromeStatusChange
// See nsIWebBrowserChrome::SetStatus
struct MsgChromeStatusChangeInfo
{
    MsgChromeStatusChangeInfo(CBrowserShell* broadcaster,
                              PRUint32 statusType,
                              const PRUnichar* status) :
        mBroadcaster(broadcaster),
        mStatusType(statusType), mStatus(status)
        { }
                     
    CBrowserShell *mBroadcaster;
    PRUint32  mStatusType;
    const PRUnichar *mStatus;
};

#endif // __CBrowserShellMsgDefs__
