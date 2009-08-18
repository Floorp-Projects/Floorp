/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "NPPInstanceParent.h"
#include "NPBrowserStreamParent.h"

namespace mozilla {
namespace plugins {

NPBrowserStreamProtocolParent*
NPPInstanceParent::NPBrowserStreamConstructor(const nsCString& url, const uint32_t& length,
                                              const uint32_t& lastmodified,
                                              const nsCString& headers,
                                              const nsCString& mimeType,
                                              const bool& seekable,
                                              NPError* rv, uint16_t *stype)
{
    NS_RUNTIMEABORT("Not reachable");
    return NULL;
}

nsresult
NPPInstanceParent::AnswerNPBrowserStreamDestructor(NPBrowserStreamProtocolParent* stream,
                                                   const NPError& reason,
                                                   const bool& artificial)
{
    if (!artificial) {
        static_cast<NPBrowserStreamParent*>(stream)->NPN_DestroyStream(reason);
    }
    return NS_OK;
}

nsresult
NPPInstanceParent::NPBrowserStreamDestructor(NPBrowserStreamProtocolParent* stream,
                                             const NPError& reason,
                                             const bool& artificial)
{
    delete stream;
    return NS_OK;
}

NPError
NPPInstanceParent::NPP_SetWindow(NPWindow* aWindow)
{
    _MOZ_LOG(__FUNCTION__);
    NS_ENSURE_TRUE(aWindow, NPERR_GENERIC_ERROR);

    NPError prv;
    nsresult rv = CallNPP_SetWindow(*aWindow, &prv);
    if (NS_OK != rv)
        return NPERR_GENERIC_ERROR;
    return prv;
}

NPError
NPPInstanceParent::NPP_GetValue(NPPVariable variable, void *ret_value)
{
    _MOZ_LOG(__FUNCTION__);

    // FIXME/cjones: HACK ALERT! should forward to child
    switch(variable) {
#ifdef OS_LINUX
    case NPPVpluginNeedsXEmbed:
        (*(PRBool*)ret_value) = PR_TRUE;
        return NPERR_NO_ERROR;
#endif
    default:
        return NPERR_GENERIC_ERROR;
    }

    NS_NOTREACHED("Don't get here!");
    return NPERR_GENERIC_ERROR;
}

NPError
NPPInstanceParent::NPP_NewStream(NPMIMEType type, NPStream* stream,
                                 NPBool seekable, uint16_t* stype)
{
    _MOZ_LOG(__FUNCTION__);
        
    NPError err;
    CallNPBrowserStreamConstructor(new NPBrowserStreamParent(this, stream),
                                   nsCString(stream->url),
                                   stream->end,
                                   stream->lastmodified,
                                   nsCString(stream->headers),
                                   nsCString(type), seekable, &err, stype);
    return err;
}    

NPError
NPPInstanceParent::NPP_DestroyStream(NPStream* stream, NPReason reason)
{
    NPBrowserStreamParent* sp =
        static_cast<NPBrowserStreamParent*>(stream->pdata);
    if (sp->mNPP != this)
        NS_RUNTIMEABORT("Mismatched plugin data");

    return CallNPBrowserStreamDestructor(sp, reason, false);
}

NPObjectProtocolParent*
NPPInstanceParent::NPObjectConstructor(NPError* _retval)
{
    NS_NOTYETIMPLEMENTED("NPPInstanceParent::NPObjectConstructor");
    return nsnull;
}

nsresult
NPPInstanceParent::NPObjectDestructor(NPObjectProtocolParent* aObject,
                                      NPError* _retval)
{
    NS_NOTYETIMPLEMENTED("NPPInstanceParent::NPObjectDestructor");
    return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace plugins
} // namespace mozilla
