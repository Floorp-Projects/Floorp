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

#include "ipcStringList.h"

void *
ipcStringNode::operator new(size_t size, const char *str) CPP_THROW_NEW
{
    int len = strlen(str);

    size += len;

    ipcStringNode *node = (ipcStringNode *) ::operator new(size);
    if (!node)
        return NULL;

    node->mNext = NULL;
    memcpy(node->mData, str, len);
    node->mData[len] = '\0';

    return node;
}

ipcStringNode *
ipcStringList::FindNode(ipcStringNode *node, const char *str)
{
    while (node) {
        if (node->Equals(str))
            return node;
        node = node->mNext;
    }
    return NULL;
}

ipcStringNode *
ipcStringList::FindNodeBefore(ipcStringNode *node, const char *str)
{
    ipcStringNode *prev = NULL;
    while (node) {
        if (node->Equals(str))
            return prev;
        prev = node;
        node = node->mNext;
    }
    return NULL;
}
