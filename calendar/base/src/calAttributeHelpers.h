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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

#ifndef CALATTRIBUTEHELPERS_H_
#define CALATTRIBUTEHELPERS_H_

#ifndef UPDATE_LAST_MODIFIED
#define UPDATE_LAST_MODIFIED /**/
#endif

/**
 ** A few helpers for declaring simple attribute getters and setters in
 ** calItemBase derivatives
 **/

// helpers for string types
#define CAL_STRINGTYPE_ATTR_GETTER(cname,mtype,name) \
NS_IMETHODIMP \
cname::Get##name (mtype &_retval) { \
    _retval.Assign(m##name); \
    return NS_OK; \
}

#define CAL_STRINGTYPE_ATTR_SETTER(cname,mtype,name) \
NS_IMETHODIMP \
cname::Set##name (const mtype &aValue) { \
    if (mImmutable) return NS_ERROR_FAILURE; \
    m##name.Assign(aValue); \
    UPDATE_LAST_MODIFIED; \
    return NS_OK; \
}

#define CAL_STRINGTYPE_ATTR(cname,mtype,name) \
    CAL_STRINGTYPE_ATTR_GETTER(cname,mtype,name) \
    CAL_STRINGTYPE_ATTR_SETTER(cname,mtype,name)

// helpers for value types
#define CAL_VALUETYPE_ATTR_GETTER(cname,mtype,name) \
NS_IMETHODIMP \
cname::Get##name (mtype *_retval) { \
    *_retval = m##name; \
    return NS_OK; \
}

#define CAL_VALUETYPE_ATTR_SETTER(cname,mtype,name) \
NS_IMETHODIMP \
cname::Set##name (mtype aValue) { \
    if (mImmutable) return NS_ERROR_FAILURE; \
    if (m##name != aValue) { \
        m##name = aValue; \
        UPDATE_LAST_MODIFIED; \
    } \
    return NS_OK; \
}

#define CAL_VALUETYPE_ATTR(cname,mtype,name) \
    CAL_VALUETYPE_ATTR_GETTER(cname,mtype,name) \
    CAL_VALUETYPE_ATTR_SETTER(cname,mtype,name)

// helpers for interface types
#define CAL_ISUPPORTS_ATTR_GETTER(cname,mtype,name) \
NS_IMETHODIMP \
cname::Get##name (mtype **_retval) { \
    NS_IF_ADDREF (*_retval = m##name); \
    return NS_OK; \
}

#define CAL_ISUPPORTS_ATTR_SETTER(cname,mtype,name) \
NS_IMETHODIMP \
cname::Set##name (mtype *aValue) { \
    if (mImmutable) return NS_ERROR_FAILURE; \
    if (m##name != aValue) { \
        m##name = aValue; \
        UPDATE_LAST_MODIFIED; \
    } \
    return NS_OK; \
}

#define CAL_ISUPPORTS_ATTR(cname,mtype,name) \
    CAL_ISUPPORTS_ATTR_GETTER(cname,mtype,name) \
    CAL_ISUPPORTS_ATTR_SETTER(cname,mtype,name)


#endif
