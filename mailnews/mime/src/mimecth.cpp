/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "mimecth.h"

/*
 * These calls are necessary to expose the object class heirarchy 
 * to externally developed content type handlers.
 */
MimeInlineTextClass *
MIME_GetmimeInlineTextClass(void)
{
  return &mimeInlineTextClass;
}

MimeLeafClass *
MIME_GetmimeLeafClass(void)
{
  return &mimeLeafClass;
}

MimeObjectClass *
MIME_GetmimeObjectClass(void)
{
  return &mimeObjectClass;
}

MimeContainerClass *
MIME_GetmimeContainerClass(void)
{
  return &mimeContainerClass;
}

MimeMultipartClass *
MIME_GetmimeMultipartClass(void)
{
  return &mimeMultipartClass;
}

MimeMultipartSignedClass *
MIME_GetmimeMultipartSignedClass(void)
{
  return &mimeMultipartSignedClass;
}
