/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "stdio.h"
#include "nscore.h"
#include "plstr.h"
#include "prtypes.h"
//#include "mimecth.h"
#include "nsMimeContentTypeHandler.h"

/* 
 * This function will be used by the factory to generate an 
 * mime object class object....
 */
nsresult NS_NewMimeContentTypeHandler(nsIMimeContentTypeHandler ** aInstancePtrResult)
{
	/* note this new macro for assertions...they can take 
     a string describing the assertion */
	NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
	if (nsnull != aInstancePtrResult)
	{
		nsMimeContentTypeHandler *obj = new nsMimeContentTypeHandler();
		if (obj)
			return obj->QueryInterface(NS_GET_IID(nsIMimeContentTypeHandler), (void**) aInstancePtrResult);
		else
			return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
	}
	else
		return NS_ERROR_NULL_POINTER; /* aInstancePtrResult was NULL....*/
}

/* 
 * The following macros actually implement addref, release and 
 * query interface for our component. 
 */
NS_IMPL_ISUPPORTS1(nsMimeContentTypeHandler, nsIMimeContentTypeHandler)

/*
 * nsIMimeEmitter definitions....
 */
nsMimeContentTypeHandler::nsMimeContentTypeHandler()
{
  /* the following macro is used to initialize the ref counting data */
  NS_INIT_REFCNT();

}

nsMimeContentTypeHandler::~nsMimeContentTypeHandler(void)
{
}

extern "C" char             *MIME_GetContentType(void);
extern "C" MimeObjectClass  *MIME_CreateContentTypeHandlerClass(const char *content_type, 
                                                                contentTypeHandlerInitStruct *initStruct);

// Get the content type if necessary
nsresult
nsMimeContentTypeHandler::GetContentType(char **contentType)
{
  *contentType = MIME_GetContentType();
  return NS_OK;
}

// Set the output stream for processed data.
nsresult
nsMimeContentTypeHandler::CreateContentTypeHandlerClass(const char *content_type, 
                                                contentTypeHandlerInitStruct *initStruct, 
                                                MimeObjectClass **objClass)
{
  *objClass = MIME_CreateContentTypeHandlerClass(content_type, initStruct);
	if (!*objClass)
    return NS_ERROR_OUT_OF_MEMORY; /* we couldn't allocate the object */
  else
    return NS_OK;
}
