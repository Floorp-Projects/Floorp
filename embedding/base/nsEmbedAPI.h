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
class nsIDirectoryServiceProvider;


/*
    mozBinDirectory -> the directory which contains the mozilla parts (chrome, res, etc)
                       If nsnull is used, the dir of the current process will be used.
    appFileLocProvider -> Provides file locations. If nsnull is passed, the default
                       provider will be constructed and used. If the param is non-null,
                       it must be AddRefed already and this function takes ownership. 
*/

extern nsresult NS_InitEmbedding(nsILocalFile *mozBinDirectory,
                                 nsIDirectoryServiceProvider *appFileLocProvider);
  
extern nsresult NS_TermEmbedding();

#endif

