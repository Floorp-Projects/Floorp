/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#ifndef MSG_OfflineNewsArt_H
#define MSG_OfflineNewsArt_H

class MSG_Pane;
class NewsMessageHdr;
class NewsGroupDB;

class MSG_OfflineNewsArtState 
{
public:
					MSG_OfflineNewsArtState(MSG_Pane *pane, const char *group,
										uint32 messageNumber);
	virtual			~MSG_OfflineNewsArtState();
	virtual int		Process(char *outputBuffer, int outputBufSize);
	virtual int		Interrupt();
protected:
	MSG_Pane		*m_pane;
	int32			m_bytesReadSoFar;
	int32			m_articleLength;
	NewsMessageHdr	*m_newsHeader;
	NewsGroupDB		*m_newsDB;
};

#endif
