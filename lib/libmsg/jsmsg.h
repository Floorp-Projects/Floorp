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
/*   jsmsg.h -- javascript reflection of mail messages for filters.
 */

#ifndef _JSMSG_H_
#define _JSMSG_H_

class ParseNewMailState;

extern MSG_SearchError JSMailFilter_execute(ParseNewMailState *state,
											MSG_Filter* filter,
											MailMessageHdr* msgHdr,
											MailDB* mailDB,
											XP_Bool* pMoved);

extern MSG_SearchError JSNewsFilter_execute(MSG_Filter* filter, 
											DBMessageHdr* msgHdr, 
											NewsGroupDB* newsDB);

/* runs the garbage collector on the filter context. Probably a good
   idea to call on completion of GetNewMail */
extern void JSFilter_cleanup();

#endif /* _JSMSG_H_ */
