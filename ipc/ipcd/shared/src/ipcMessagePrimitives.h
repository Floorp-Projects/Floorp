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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#ifndef ipcMessagePrimitives_h__
#define ipcMessagePrimitives_h__

#include "ipcMessage.h"

class ipcMessage_DWORD : public ipcMessage
{
public:
    ipcMessage_DWORD(const nsID &target, PRUint32 first)
    {
        Init(target, (char *) &first, sizeof(first));
    }

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }
};

class ipcMessage_DWORD_DWORD : public ipcMessage
{
public:
    ipcMessage_DWORD_DWORD(const nsID &target, PRUint32 first, PRUint32 second)
    {
        PRUint32 data[2] = { first, second };
        Init(target, (char *) data, sizeof(data));
    }

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    PRUint32 Second() const
    {
        return ((PRUint32 *) Data())[1];
    }
};

class ipcMessage_DWORD_DWORD_DWORD : public ipcMessage
{
public:
    ipcMessage_DWORD_DWORD_DWORD(const nsID &target, PRUint32 first, PRUint32 second, PRUint32 third)
    {
        PRUint32 data[3] = { first, second, third };
        Init(target, (char *) data, sizeof(data));
    }

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    PRUint32 Second() const
    {
        return ((PRUint32 *) Data())[1];
    }

    PRUint32 Third() const
    {
        return ((PRUint32 *) Data())[2];
    }
};

class ipcMessage_DWORD_DWORD_DWORD_DWORD : public ipcMessage
{
public:
    ipcMessage_DWORD_DWORD_DWORD_DWORD(const nsID &target, PRUint32 first, PRUint32 second, PRUint32 third, PRUint32 fourth)
    {
        PRUint32 data[4] = { first, second, third, fourth };
        Init(target, (char *) data, sizeof(data));
    }

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    PRUint32 Second() const
    {
        return ((PRUint32 *) Data())[1];
    }

    PRUint32 Third() const
    {
        return ((PRUint32 *) Data())[2];
    }

    PRUint32 Fourth() const
    {
        return ((PRUint32 *) Data())[3];
    }
};

class ipcMessage_DWORD_STR : public ipcMessage
{
public:
    ipcMessage_DWORD_STR(const nsID &target, PRUint32 first, const char *second) NS_HIDDEN;

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    const char *Second() const
    {
        return Data() + sizeof(PRUint32);
    }
};

class ipcMessage_DWORD_DWORD_STR : public ipcMessage
{
public:
    ipcMessage_DWORD_DWORD_STR(const nsID &target, PRUint32 first, PRUint32 second, const char *third) NS_HIDDEN;

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    PRUint32 Second() const
    {
        return ((PRUint32 *) Data())[1];
    }

    const char *Third() const
    {
        return Data() + 2 * sizeof(PRUint32);
    }
};

class ipcMessage_DWORD_ID : public ipcMessage
{
public:
    ipcMessage_DWORD_ID(const nsID &target, PRUint32 first, const nsID &second) NS_HIDDEN;

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    const nsID &Second() const
    {
        return * (const nsID *) (Data() + sizeof(PRUint32));
    }
};

class ipcMessage_DWORD_DWORD_ID : public ipcMessage
{
public:
    ipcMessage_DWORD_DWORD_ID(const nsID &target, PRUint32 first, PRUint32 second, const nsID &third) NS_HIDDEN;

    PRUint32 First() const
    {
        return ((PRUint32 *) Data())[0];
    }

    PRUint32 Second() const
    {
        return ((PRUint32 *) Data())[1];
    }

    const nsID &Third() const
    {
        return * (const nsID *) (Data() + 2 * sizeof(PRUint32));
    }
};

#endif // !ipcMessagePrimitives_h__
