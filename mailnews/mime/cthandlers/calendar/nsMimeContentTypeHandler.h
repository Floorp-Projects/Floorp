/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
 
/*
 * This interface is implemented by content type handlers that will be
 * called upon by libmime to process various attachments types. The primary
 * purpose of these handlers will be to represent the attached data in a
 * viewable HTML format that is useful for the user
 *
 * Note: These will all register by their content type prefixed by the
 *       following:  mimecth:text/vcard
 * 
 *       libmime will then use nsComponentManager::ProgIDToCLSID() to 
 *       locate the appropriate Content Type handler
 */
#ifndef nsMimeContentTypeHandler_h_
#define nsMimeContentTypeHandler_h_

#include "prtypes.h"
#include "nsIMimeContentTypeHandler.h"

class nsMimeContentTypeHandler : public nsIMimeContentTypeHandler {
public: 
    nsMimeContentTypeHandler ();
    virtual       ~nsMimeContentTypeHandler (void);

    /* this macro defines QueryInterface, AddRef and Release for this class */
    NS_DECL_ISUPPORTS 

    NS_IMETHOD    GetContentType(char **contentType);

    NS_IMETHOD    CreateContentTypeHandlerClass(const char *content_type, 
                                                contentTypeHandlerInitStruct *initStruct, 
                                                MimeObjectClass **objClass);
}; 

/* this function will be used by the factory to generate an class access object....*/
extern nsresult NS_NewMimeContentTypeHandler(nsIMimeContentTypeHandler **aInstancePtrResult);


#endif /* nsMimeContentTypeHandler_h_ */
