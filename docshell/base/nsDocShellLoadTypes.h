/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
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

#ifndef nsDocShellLoadTypes_h_
#define nsDocShellLoadTypes_h_

#ifdef MOZILLA_INTERNAL_API

#include "nsIDocShell.h"
#include "nsIWebNavigation.h"

/**
 * Load flag for error pages. This uses one of the reserved flag
 * values from nsIWebNavigation.
 */
#define LOAD_FLAGS_ERROR_PAGE 0x0001U

#define MAKE_LOAD_TYPE(type, flags) ((type) | ((flags) << 16))
#define LOAD_TYPE_HAS_FLAGS(type, flags) ((type) & ((flags) << 16))

/**
 * These are flags that confuse ConvertLoadTypeToDocShellLoadInfo and should
 * not be passed to MAKE_LOAD_TYPE.  In particular this includes all flags
 * above 0xffff (e.g. LOAD_FLAGS_BYPASS_CLASSIFIER), since MAKE_LOAD_TYPE would
 * just shift them out anyway.
 */
#define EXTRA_LOAD_FLAGS (LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP | \
                          LOAD_FLAGS_FIRST_LOAD              | \
                          LOAD_FLAGS_ALLOW_POPUPS            | \
                          0xffff0000)



/* load types are legal combinations of load commands and flags 
 *  
 * NOTE:
 *  Remember to update the IsValidLoadType function below if you change this
 *  enum to ensure bad flag combinations will be rejected.
 */
enum LoadType {
    LOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_NORMAL_REPLACE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
    LOAD_NORMAL_EXTERNAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_FROM_EXTERNAL),
    LOAD_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_HISTORY, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_NORMAL_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
    LOAD_NORMAL_BYPASS_PROXY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_NORMAL_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_RELOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_RELOAD_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
    LOAD_RELOAD_BYPASS_PROXY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_RELOAD_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_LINK = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_LINK),
    LOAD_REFRESH = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_REFRESH),
    LOAD_RELOAD_CHARSET_CHANGE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE),
    LOAD_BYPASS_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY),
    LOAD_STOP_CONTENT = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_STOP_CONTENT),
    LOAD_STOP_CONTENT_AND_REPLACE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_STOP_CONTENT | nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
    /**
     * Load type for an error page. These loads are never triggered by users of
     * Docshell. Instead, Docshell triggers the load itself when a
     * consumer-triggered load failed.
     */
    LOAD_ERROR_PAGE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, LOAD_FLAGS_ERROR_PAGE)

    // NOTE: Adding a new value? Remember to update IsValidLoadType!
};
static inline PRBool IsValidLoadType(PRUint32 aLoadType)
{
    switch (aLoadType)
    {
    case LOAD_NORMAL:
    case LOAD_NORMAL_REPLACE:
    case LOAD_NORMAL_EXTERNAL:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_HISTORY:
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_LINK:
    case LOAD_REFRESH:
    case LOAD_RELOAD_CHARSET_CHANGE:
    case LOAD_BYPASS_HISTORY:
    case LOAD_STOP_CONTENT:
    case LOAD_STOP_CONTENT_AND_REPLACE:
    case LOAD_ERROR_PAGE:
        return PR_TRUE;
    }
    return PR_FALSE;
}

#endif // MOZILLA_INTERNAL_API
#endif 
