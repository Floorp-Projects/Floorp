/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _nsMsgComposeBE_H_
#define _nsMsgComposeBE_H_

#include "rosetta.h"

//
// Composition back end declarations...
//

typedef enum
{
  nsMsgDeliverNow,
  nsMsgQueueForLater,
  nsMsgSave,
  nsMsgSaveAs,
  nsMsgSaveAsDraft,
  nsMsgSaveAsTemplate
} nsMsgDeliverMode;

struct nsMsgAttachmentData
{
  char *url;			        // The URL to attach. This should be 0 to signify "end of list".

  char *desired_type;	    // The type to which this document should be
						              // converted.  Legal values are NULL, TEXT_PLAIN
						              // and APPLICATION_POSTSCRIPT (which are macros
                          // defined in net.h); other values are ignored.

  char *real_type;		    // The type of the URL if known, otherwise NULL. For example, if 
                          // you were attaching a temp file which was known to contain HTML data, 
                          // you would pass in TEXT_HTML as the real_type, to override whatever type 
                          // the name of the tmp file might otherwise indicate.

  char *real_encoding;	  // Goes along with real_type 

  char *real_name;		    // The original name of this document, which will eventually show up in the 
                          // Content-Disposition header. For example, if you had copied a document to a 
                          // tmp file, this would be the original, human-readable name of the document.

  char *description;	    // If you put a string here, it will show up as the Content-Description header.  
                          // This can be any explanatory text; it's not a file name.						 

  char *x_mac_type, *x_mac_creator; // Mac-specific data that should show up as optional parameters
						                        // to the content-type header.
};


//
// When we have downloaded a URL to a tmp file for attaching, this
// represents everything we learned about it (and did to it) in the
// process. 
//
typedef struct nsMsgAttachedFile
{
  char *orig_url;		  // Where it came from on the network (or even elsewhere on the local disk.)

  char *file_name;		// The tmp file in which the (possibly converted) data now resides.
  
  char *type;			    // The type of the data in file_name (not necessarily the same as the type of orig_url.)

  char *encoding;		  // Likewise, the encoding of the tmp file. This will be set only if the original 
                      // document had an encoding already; we don't do base64 encoding and so forth until 
                      // it's time to assemble a full MIME message of all parts.


  char *description;	// For Content-Description header 
  char *x_mac_type;   // mac-specific info 
  char *x_mac_creator;// mac-specific info 
  char *real_name;		// The real name of the file. 

  /* Some statistics about the data that was written to the file, so that when
	 it comes time to compose a MIME message, we can make an informed decision
	 about what Content-Transfer-Encoding would be best for this attachment.
	 (If it's encoded already, we ignore this information and ship it as-is.)
   */
  uint32 size;
  uint32 unprintable_count;
  uint32 highbit_count;
  uint32 ctl_count;
  uint32 null_count;
  uint32 max_line_length;
  
  HG68452

} nsMsgAttachedFile;

#endif /* _nsMsgComposeBE_H_ */

