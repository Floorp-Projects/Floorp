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

#ifndef ipcStringList_h__
#define ipcStringList_h__

#include <string.h>
#include "plstr.h"
#include "xpcom-config.h"
#include "ipcList.h"

//-----------------------------------------------------------------------------
// string node
//-----------------------------------------------------------------------------

class ipcStringNode
{
public:
    ipcStringNode() {}

    const char *Value() const { return mData; }

    PRBool Equals(const char *val) const { return strcmp(mData, val) == 0; }
    PRBool EqualsIgnoreCase(const char *val) const { return PL_strcasecmp(mData, val) == 0; }

    class ipcStringNode *mNext;
private:
    void *operator new(size_t size, const char *str) CPP_THROW_NEW;

    // this is actually bigger
    char mData[1];

    friend class ipcStringList;
};

//-----------------------------------------------------------------------------
// singly-linked list of strings
//-----------------------------------------------------------------------------

class ipcStringList : public ipcList<ipcStringNode>
{
public:
    typedef ipcList<ipcStringNode> Super;

    void Prepend(const char *str)
    {
        Super::Prepend(new (str) ipcStringNode());
    }

    void Append(const char *str)
    {
        Super::Append(new (str) ipcStringNode());
    }

    const ipcStringNode *Find(const char *str) const
    {
        return FindNode(mHead, str);
    }

    void FindAndDelete(const char *str)
    {
        ipcStringNode *node = FindNodeBefore(mHead, str);
        if (node)
            DeleteAfter(node);
        else
            DeleteFirst();
    }

private:
    static ipcStringNode *FindNode      (ipcStringNode *head, const char *str);
    static ipcStringNode *FindNodeBefore(ipcStringNode *head, const char *str);
};

#endif // !ipcStringList_h__
