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
 * This interface allows any module to access the encoder/decoder 
 * routines for RFC822 headers. This will allow any mail/news module
 * to call on these routines.
 */
#ifndef nsMailHeaders_h_
#define nsMailHeaders_h_

/* 
 * These are the defines for standard header field names.
 */
#define HEADER_BCC							            "BCC"
#define HEADER_CC							              "CC"
#define HEADER_CONTENT_BASE					        "Content-Base"
#define HEADER_CONTENT_LOCATION					    "Content-Location"
#define HEADER_CONTENT_ID					          "Content-ID"
#define HEADER_CONTENT_DESCRIPTION			    "Content-Description"
#define HEADER_CONTENT_DISPOSITION			    "Content-Disposition"
#define HEADER_CONTENT_ENCODING				      "Content-Encoding"
#define HEADER_CONTENT_LENGTH				        "Content-Length"
#define HEADER_CONTENT_NAME					        "Content-Name"
#define HEADER_CONTENT_TRANSFER_ENCODING	  "Content-Transfer-Encoding"
#define HEADER_CONTENT_TYPE					        "Content-Type"
#define HEADER_DATE							            "Date"
#define HEADER_DISTRIBUTION					        "Distribution"
#define HEADER_FCC							            "FCC"
#define HEADER_FOLLOWUP_TO					        "Followup-To"
#define HEADER_FROM							            "From"
#define HEADER_LINES						            "Lines"
#define HEADER_MESSAGE_ID					          "Message-ID"
#define HEADER_MIME_VERSION					        "MIME-Version"
#define HEADER_NEWSGROUPS					          "Newsgroups"
#define HEADER_ORGANIZATION					        "Organization"
#define HEADER_REFERENCES				          	"References"
#define HEADER_REPLY_TO				          		"Reply-To"
#define HEADER_RESENT_COMMENTS	      			"Resent-Comments"
#define HEADER_RESENT_DATE			        		"Resent-Date"
#define HEADER_RESENT_FROM			        		"Resent-From"
#define HEADER_RESENT_MESSAGE_ID	      		"Resent-Message-ID"
#define HEADER_RESENT_SENDER			        	"Resent-Sender"
#define HEADER_RESENT_TO				          	"Resent-To"
#define HEADER_RESENT_CC				          	"Resent-CC"
#define HEADER_SENDER					            	"Sender"
#define HEADER_SUBJECT				          		"Subject"
#define HEADER_TO						              	"To"
#define HEADER_X_MAILER			          			"X-Mailer"
#define HEADER_X_NEWSREADER				        	"X-Newsreader"
#define HEADER_X_POSTING_SOFTWARE		      	"X-Posting-Software"
#define HEADER_X_MOZILLA_STATUS	 		      	"X-Mozilla-Status"
#define HEADER_X_MOZILLA_NEWSHOST		      	"X-Mozilla-News-Host"
#define HEADER_X_MOZILLA_DRAFT_INFO		      "X-Mozilla-Draft-Info"
#define HEADER_X_UIDL					            	"X-UIDL"
#define HEADER_XREF						            	"XREF"
#define HEADER_X_SUN_CHARSET	        			"X-Sun-Charset"
#define HEADER_X_SUN_CONTENT_LENGTH			    "X-Sun-Content-Length"
#define HEADER_X_SUN_CONTENT_LINES		      "X-Sun-Content-Lines"
#define HEADER_X_SUN_DATA_DESCRIPTION	    	"X-Sun-Data-Description"
#define HEADER_X_SUN_DATA_NAME			      	"X-Sun-Data-Name"
#define HEADER_X_SUN_DATA_TYPE			      	"X-Sun-Data-Type"
#define HEADER_X_SUN_ENCODING_INFO		    	"X-Sun-Encoding-Info"
#define HEADER_X_PRIORITY                   "X-Priority"

#define HEADER_PARM_CHARSET                 "charset"     
#define HEADER_PARM_START                   "start"     
#define HEADER_PARM_BOUNDARY				        "BOUNDARY"
#define HEADER_PARM_FILENAME				        "FILENAME"
#define HEADER_PARM_NAME					          "NAME"
#define HEADER_PARM_TYPE					          "TYPE"

#define HEADER_X_MOZILLA_PART_URL           "X-Mozilla-PartURL"

#endif /* nsMailHeaders_h_ */
