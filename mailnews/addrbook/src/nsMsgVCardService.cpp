/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is vCard bridge code.
 *
 * The Initial Developer of the Original Code is
 * Seth Spitzer <sspitzer@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsMsgVCardService.h"
#include "prmem.h"
#include "plstr.h"
    
NS_IMPL_ISUPPORTS1(nsMsgVCardService, nsIMsgVCardService)

nsMsgVCardService::nsMsgVCardService()
{
}

nsMsgVCardService::~nsMsgVCardService()
{
}

NS_IMETHODIMP_(void) nsMsgVCardService::CleanVObject(VObject * o)
{
    cleanVObject(o);
}

NS_IMETHODIMP_(VObject *) nsMsgVCardService::NextVObjectInList(VObject * o)
{
    return nextVObjectInList(o);
}

NS_IMETHODIMP_(VObject *) nsMsgVCardService::Parse_MIME(const char *input, PRUint32 len)
{
    return parse_MIME(input, (unsigned long)len);
}

NS_IMETHODIMP_(char *) nsMsgVCardService::FakeCString(VObject * o)
{
    return fakeCString(vObjectUStringZValue(o));
}

NS_IMETHODIMP_(VObject *) nsMsgVCardService::IsAPropertyOf(VObject * o, const char *id)
{
    return isAPropertyOf(o,id);
}

NS_IMETHODIMP_(char *) nsMsgVCardService::WriteMemoryVObjects(const char *s, PRInt32 *len, VObject * list, PRBool expandSpaces)
{
    return writeMemoryVObjects((char *)s, len, list, expandSpaces);
}

NS_IMETHODIMP_(VObject *) nsMsgVCardService::NextVObject(VObjectIterator * i)
{
    return nextVObject(i);
}

NS_IMETHODIMP_(void) nsMsgVCardService::InitPropIterator(VObjectIterator * i, VObject * o)
{
    initPropIterator(i,o);
}

NS_IMETHODIMP_(PRInt32) nsMsgVCardService::MoreIteration(VObjectIterator * i)
{
    return ((PRInt32)moreIteration(i));
}

NS_IMETHODIMP_(const char *) nsMsgVCardService::VObjectName(VObject * o)
{
    return vObjectName(o);
}

NS_IMETHODIMP_(char *) nsMsgVCardService::VObjectAnyValue(VObject * o)
{
    char *retval = (char *)PR_MALLOC(strlen((char *)vObjectAnyValue(o)) + 1);
    if (retval)
        PL_strcpy(retval, (char *) vObjectAnyValue(o));
    return retval;
}
