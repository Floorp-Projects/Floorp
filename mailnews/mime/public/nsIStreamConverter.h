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
#ifndef nsIStreamConverter_h_
#define nsIStreamConverter_h_

#include "nsIStreamListener.h" 
#include "nsIOutputStream.h" 
#include "nsIURI.h" 

// {C9CDF8E5-95FA-11d2-8807-00805F5A1FB8} 
#define NS_ISTREAM_CONVERTER_IID \
   { 0xc9cdf8e5, 0x95fa, 0x11d2,    \
   { 0x88, 0x7, 0x0, 0x80, 0x5f, 0x5a, 0x1f, 0xb8 } }

// {588595CB-2012-11d3-8EF0-00A024A7D144}
#define NS_STREAM_CONVERTER_CID  \
    { 0x588595cb, 0x2012, 0x11d3,   \
    { 0x8e, 0xf0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

// Message delivery modes
typedef enum
{
  nsMimeMessageSplitDisplay,    // the wrapper HTML output to produce the split header/body display
  nsMimeMessageHeaderDisplay,   // the split header - header display
  nsMimeMessageBodyDisplay,     // the split header - body display
  nsMimeMessageQuoting,         // all HTML quoted output
  nsMimeMessageRaw,             // the raw RFC822 data (view source?)
  nsMimeUnknown                 // Don't know the format, figure it out from the URL/headers
} nsMimeOutputType;

class nsIStreamConverter : public nsIStreamListener { 
public: 
  static const nsIID& GetIID() { static nsIID iid = NS_ISTREAM_CONVERTER_IID; return iid; }

    // 
    // This is the output stream where the stream converter will write processed data after 
    // conversion. 
    // 
    NS_IMETHOD SetOutputStream(nsIOutputStream *aOutStream, nsIURI *aURI, nsMimeOutputType aType,
                               nsMimeOutputType *aOutFormat, char **aOutputContentType) = 0; 

    // 
    // This is the type of output operation that is being requested by libmime. The types
    // of output are specified by nsIMimeOutputType enum
    // 
    NS_IMETHOD SetOutputType(nsMimeOutputType aType) = 0; 

    // 
    // The output listener can be set to allow for the flexibility of having the stream converter 
    // directly notify the listener of the output stream for any processed/converter data. If 
    // this output listener is not set, the data will be written into the output stream but it is 
    // the responsibility of the client of the stream converter to handle the resulting data. 
    // 
    NS_IMETHOD SetOutputListener(nsIStreamListener *aOutListener) = 0; 

    // 
    // This is needed by libmime for MHTML link processing...this is the URI associated
    // with this input stream
    // 
    NS_IMETHOD SetStreamURI(nsIURI *aURI) = 0; 
}; 

#endif /* nsIStreamConverter_h_ */
