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


/*
    aPath       -> the mozilla bin directory. If nsnull, the default is used
    aProvider   -> the application directory service provider. If nsnull, the
                   default (nsAppFileLocationProvider) is constructed and used.
*/
extern nsresult NS_InitEmbedding(nsILocalFile *mozBinDirectory,
                                 nsIDirectoryServiceProvider *appFileLocProvider);
extern nsresult NS_TermEmbedding();


#endif

