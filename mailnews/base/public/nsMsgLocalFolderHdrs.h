/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef _nsMsgLocalFolderHdrs_H
#define _nsMsgLocalFolderHdrs_H

/* The Netscape-specific header fields that we use for storing our
   various bits of state in mail folders.
 */
#define X_MOZILLA_STATUS           "X-Mozilla-Status"
#define X_MOZILLA_STATUS_FORMAT    X_MOZILLA_STATUS ": %04.4x"
#define X_MOZILLA_STATUS_LEN      /*1234567890123456*/      16

#define X_MOZILLA_STATUS2		   "X-Mozilla-Status2"
#define X_MOZILLA_STATUS2_FORMAT   X_MOZILLA_STATUS2 ": %08.8x"
#define X_MOZILLA_STATUS2_LEN	  /*12345678901234567*/		17

#define X_MOZILLA_DRAFT_INFO	   "X-Mozilla-Draft-Info"
#define X_MOZILLA_DRAFT_INFO_LEN  /*12345678901234567890*/  20

#define X_MOZILLA_NEWSHOST         "X-Mozilla-News-Host"
#define X_MOZILLA_NEWSHOST_LEN    /*1234567890123456789*/   19

#define X_UIDL                     "X-UIDL"
#define X_UIDL_LEN                /*123456*/                 6

#define CONTENT_LENGTH             "Content-Length"
#define CONTENT_LENGTH_LEN        /*12345678901234*/        14

/* Provide a common means of detecting empty lines in a message. i.e. to detect the end of headers among other things...*/
#define EMPTY_MESSAGE_LINE(buf) (buf[0] == nsCRT::CR || buf[0] == nsCRT::LF || buf[0] == '\0')

#endif
