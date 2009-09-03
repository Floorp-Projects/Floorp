/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
#include "WebGLContext.h"

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
// this is a toolkit service? oops.
#include "nsIContentURIGrouper.h"
#include "nsIContentPrefService.h"
#endif

using namespace mozilla;

PRBool
WebGLContext::SafeToCreateCanvas3DContext(nsICanvasElement *canvasElement)
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
    rv = prefService->GetBoolPref("layout.canvas3d.enabled_for_all_sites", &allSites);
    if (NS_SUCCEEDED(rv) && allSites) {
        // the all-sites pref was set, we're good to go
        return PR_TRUE;
    }

    // XXX fixme
    // content prefs is a toolkit component, so it's not available when we're compiling.. oops.

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

    // our pref is 'canvas3d.enabled'
    nsCOMPtr<nsIVariant> val;
    rv = cpsvc->GetPref(contentURI, NS_LITERAL_STRING("canvas3d.enabled"), getter_AddRefs(val));
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
  char buf[256];

  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (console) {
    vsnprintf(buf, 256, fmt, ap);
    console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(buf)).get());
    fprintf(stderr, "%s\n", buf);
  }

  va_end(ap);
}

nsresult
WebGLContext::ErrorMessage(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  char buf[256];

  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (console) {
    vsnprintf(buf, 256, fmt, ap);
    console->LogStringMessage(NS_ConvertUTF8toUTF16(nsDependentCString(buf)).get());
    fprintf(stderr, "%s\n", buf);
  }

  va_end(ap);

  return NS_ERROR_FAILURE;
}
