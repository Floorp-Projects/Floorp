/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Author:
 *   Adam Lock <adamlock@netscape.com>
 *
 * Contributor(s):
 */

#ifndef NSEMBEDAPI_H
#define NSEMBEDAPI_H

#include "nsILocalFile.h"
#include "nsIDirectoryService.h"


/**
 * Function to initialise the Gecko embedding APIs. You *must* call this
 * method before any others!
 *
 *   aPath      -> the mozilla bin directory. If nsnull, the default is used
 *   aProvider  -> the application directory service provider. If nsnull, the
 *                 default (nsAppFileLocationProvider) is constructed and used.
 */
extern nsresult NS_InitEmbedding(nsILocalFile *mozBinDirectory,
                                 nsIDirectoryServiceProvider *appFileLocProvider);


/**
 * Function to call to finish the Gecko embedding APIs.
 */
extern nsresult NS_TermEmbedding();

/*---------------------------------------------------------------------------*/
/* Event processing APIs. The native OS dependencies mean you must be        */
/* building on a supported platform to get the functions below.              */
/*---------------------------------------------------------------------------*/

#undef MOZ_SUPPORTS_EMBEDDING_EVENT_PROCESSING

/* Win32 specific stuff */
#ifdef WIN32
#include "windows.h"
typedef MSG nsEmbedNativeEvent;
#define MOZ_SUPPORTS_EMBEDDING_EVENT_PROCESSING
#endif

/* Mac specific stuff */
/* TODO implementation left as an exercise for the reader */

/* GTK specific stuff */
/* TODO implementation left as an exercise for the reader */


#ifdef MOZ_SUPPORTS_EMBEDDING_EVENT_PROCESSING

/**
 * Function to call during the idle time in your application and/or as each
 * event is processed. This function ensures things such as timers are fired
 * correctly.
 */
extern nsresult NS_DoIdleEmbeddingStuff();


/**
 * Function to call before handling an event. It gives Gecko the chance to
 * handle the event first.
 *
 *   aEvent      -> the native UI event
 *   aWasHandled -> returns with PR_TRUE if the event was handled by Gecko
 */
extern nsresult NS_HandleEmbeddingEvent(nsEmbedNativeEvent &aEvent, PRBool &aWasHandled);

#endif /* MOZ_SUPPORTS_EMBEDDING_EVENT_PROCESSING */

#endif /* NSEMBEDAPI_H */

