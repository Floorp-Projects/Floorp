/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozIXMLTermStream.h: interface to display HTML/XML streams as documents
//                      (unregistered)

#ifndef mozIXMLTermStream_h___
#define mozIXMLTermStream_h___

#include "nscore.h"

#include "nsISupports.h"
#include "nsIDOMDocument.h"
#include "nsIWebShell.h"
#include "nsIPresShell.h"
#include "nsIScriptContext.h"
#include "nsIInputStream.h"

/* {0eb82b40-43a2-11d3-8e76-006008948af5} */
#define MOZIXMLTERMSTREAM_IID_STR "0eb82b40-43a2-11d3-8e76-006008948af5"
#define MOZIXMLTERMSTREAM_IID \
  {0x0eb82b40, 0x43a2, 0x11d3, \
    { 0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5 }}

class mozIXMLTermStream : public nsIInputStream
{
  public:
  NS_DEFINE_STATIC_IID_ACCESSOR(MOZIXMLTERMSTREAM_IID);

  // mozIXMLTermStream interface

  /** Open stream in specified frame, or in current frame if frameName is null
   * @param aDOMWindow parent window
   * @param frameName name of child frame in which to display stream, or null
   *                  to display in parent window
   * @param contentURL URL of stream content
   * @param contentType MIME type of stream content
   * @param maxResizeHeight maximum resize height (0=> do not resize)
   * @return NS_OK on success
   */
  NS_IMETHOD Open(nsIDOMWindowInternal* aDOMWindow,
                  const char* frameName,
                  const char* contentURL,
                  const char* contentType,
                  PRInt32 maxResizeHeight) = 0;

  /** Write Unicode string to stream (blocks until write is completed)
   * @param buf string to write
   * @return NS_OK on success
   */
  NS_IMETHOD Write(const PRUnichar* buf) = 0;
};

#define MOZXMLTERMSTREAM_CID                 \
{ /* 0eb82b41-43a2-11d3-8e76-006008948af5 */ \
   0x0eb82b41, 0x43a2, 0x11d3,               \
{0x8e, 0x76, 0x00, 0x60, 0x08, 0x94, 0x8a, 0xf5} }
extern nsresult
NS_NewXMLTermStream(mozIXMLTermStream** aXMLTermStream);

#endif /* mozIXMLTermStream_h___ */
