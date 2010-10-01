/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *   Mark Steele <mwsteele@gmail.com>
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

#include <stdarg.h>

#include "WebGLContext.h"

#include "prprf.h"

#include "nsIConsoleService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPrefBranch.h"
#include "nsServiceManagerUtils.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIVariant.h"

#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMDataContainerEvent.h"

#include "nsContentUtils.h"

#if 0
#include "nsIContentURIGrouper.h"
#include "nsIContentPrefService.h"
#endif

using namespace mozilla;

PRBool
WebGLContext::SafeToCreateCanvas3DContext(nsHTMLCanvasElement *canvasElement)
{
    nsresult rv;

    // first see if we're a chrome context
    PRBool is_caller_chrome = PR_FALSE;
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = ssm->SubjectPrincipalIsSystem(&is_caller_chrome);
    if (NS_SUCCEEDED(rv) && is_caller_chrome)
        return PR_TRUE;

    // not chrome? check pref.

    // first check our global pref
    nsCOMPtr<nsIPrefBranch> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRBool allSites = PR_FALSE;
    rv = prefService->GetBoolPref("webgl.enabled_for_all_sites", &allSites);
    if (NS_SUCCEEDED(rv) && allSites) {
        // the all-sites pref was set, we're good to go
        return PR_TRUE;
    }

#if 0
    // otherwise we'll check content prefs
    nsCOMPtr<nsIContentPrefService> cpsvc = do_GetService("@mozilla.org/content-pref/service;1", &rv);
    if (NS_FAILED(rv)) {
        LogMessage("Canvas 3D: Failed to get Content Pref service, can't verify that canvas3d is ok for this site!");
        return PR_FALSE;
    }

    // grab our content URI
    nsCOMPtr<nsIURI> contentURI;

    nsCOMPtr<nsIPrincipal> principal;
    rv = ssm->GetSubjectPrincipal(getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    if (!principal) {
        // seriously? no script executing, but not the system principal?
        return PR_FALSE;
    }
    rv = principal->GetURI(getter_AddRefs(contentURI));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // our pref is 'webgl.enabled'
    nsCOMPtr<nsIVariant> val;
    rv = cpsvc->GetPref(contentURI, NS_LITERAL_STRING("webgl.enabled"), getter_AddRefs(val));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRInt32 iv;
    rv = val->GetAsInt32(&iv);
    if (NS_SUCCEEDED(rv)) {
        // 1 means "yes, allowed"
        if (iv == 1)
            return PR_TRUE;

        // -1 means "no, don't ask me again"
        if (iv == -1)
            return PR_FALSE;

        // otherwise, we'll throw an event and maybe ask the user
    }

    // grab the document that we can use to create the event
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(canvasElement);
    nsCOMPtr<nsIDOMDocument> domDoc;
    rv = node->GetOwnerDocument(getter_AddRefs(domDoc));

    /*
    // figure out where to throw the event.  we just go for the outermost
    // document.  ideally, I want to throw the event to the <browser> if one exists,
    // otherwise the topmost document, but that's more work than I want to deal with.
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    while (doc->GetParentDocument())
        doc = doc->GetParentDocument();
    */

    // set up the event
    nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(domDoc);
    NS_ENSURE_TRUE(docEvent, PR_FALSE);

    nsCOMPtr<nsIDOMEvent> eventBase;
    rv = docEvent->CreateEvent(NS_LITERAL_STRING("DataContainerEvent"), getter_AddRefs(eventBase));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = eventBase->InitEvent(NS_LITERAL_STRING("Canvas3DContextRequest"), PR_TRUE, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIDOMDataContainerEvent> event = do_QueryInterface(eventBase);
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(eventBase);
    NS_ENSURE_TRUE(event && privateEvent, PR_FALSE);

    // mark it as trusted, so that it'll bubble upwards into chrome
    privateEvent->SetTrusted(PR_TRUE);

    // set some extra data on the event
    nsCOMPtr<nsIContentURIGrouper> grouper = do_GetService("@mozilla.org/content-pref/hostname-grouper;1", &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsAutoString group;
    rv = grouper->Group(contentURI, group);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIWritableVariant> groupVariant = do_CreateInstance(NS_VARIANT_CONTRACTID);
    nsCOMPtr<nsIWritableVariant> uriVariant = do_CreateInstance(NS_VARIANT_CONTRACTID);

    groupVariant->SetAsAString(group);
    uriVariant->SetAsISupports(contentURI);

    rv = event->SetData(NS_LITERAL_STRING("group"), groupVariant);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    rv = event->SetData(NS_LITERAL_STRING("uri"), uriVariant);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    // our target...
    nsCOMPtr<nsIDOMEventTarget> targ = do_QueryInterface(canvasElement);

    // and go.
    PRBool defaultActionEnabled;
    targ->DispatchEvent(event, &defaultActionEnabled);
#endif

    return PR_FALSE;
}

void
WebGLContext::LogMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    LogMessage(fmt, ap);

    va_end(ap);
}

void
WebGLContext::LogMessage(const char *fmt, va_list ap)
{
    char buf[1024];

    nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (console) {
        PR_vsnprintf(buf, 1024, fmt, ap);
        console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(buf)).get());
        fprintf(stderr, "%s\n", buf);
    }
}

void
WebGLContext::LogMessage(bool display, const char *fmt, ...)
{
    if (!display)
        return;

    va_list ap;
    va_start(ap, fmt);

    LogMessage(fmt, ap);

    va_end(ap);
}

void
WebGLContext::LogMessageIfVerbose(const char *fmt, ...)
{
    if (!mVerbose)
        return;

    va_list ap;
    va_start(ap, fmt);

    LogMessage(fmt, ap);

    va_end(ap);
}

nsresult
WebGLContext::SynthesizeGLError(WebGLenum err)
{
    // If there is already a pending error, don't overwrite it;
    // but if there isn't, then we need to check for a gl error
    // that may have occurred before this one and use that code
    // instead.

    if (mSynthesizedGLError == LOCAL_GL_NO_ERROR) {
        MakeContextCurrent();

        mSynthesizedGLError = gl->fGetError();

        if (mSynthesizedGLError == LOCAL_GL_NO_ERROR)
            mSynthesizedGLError = err;
    }

    return NS_OK;
}

nsresult
WebGLContext::SynthesizeGLError(WebGLenum err, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    if (fmt)
        LogMessage(mVerbose, fmt, va);
    va_end(va);

    return SynthesizeGLError(err);
}

nsresult
WebGLContext::ErrorInvalidEnum(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    if (fmt)
        LogMessage(mVerbose, fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_ENUM);
}

nsresult
WebGLContext::ErrorInvalidOperation(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    if (fmt)
        LogMessage(mVerbose, fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_OPERATION);
}

nsresult
WebGLContext::ErrorInvalidValue(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    if (fmt)
        LogMessage(mVerbose, fmt, va);
    va_end(va);

    return SynthesizeGLError(LOCAL_GL_INVALID_VALUE);
}
