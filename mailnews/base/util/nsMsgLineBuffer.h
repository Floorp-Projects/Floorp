/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef _nsMsgLineBuffer_H
#define _nsMsgLineBuffer_H

#include "msgCore.h"    // precompiled header...

// I can't believe I have to have this stupid class, but I can't find
// anything suitable (nsStrImpl might be, when its done). nsIByteBuffer
// would do, if I had a stream for input, which I don't.

class nsByteArray 
{
public:
	nsByteArray();
	virtual ~nsByteArray();
	PRUint32	GetSize() {return m_bufferSize;}
	PRUint32	GetBufferPos() {return m_bufferPos;}
	nsresult	GrowBuffer(PRUint32 desired_size, PRUint32 quantum = 1024);
	nsresult	AppendString(const char *string);
	nsresult	AppendBuffer(const char *buffer, PRUint32 length);
	void		ResetWritePos() {m_bufferPos = 0;}
	char		*GetBuffer() {return m_buffer;}
protected:
	char		*m_buffer;
	PRUint32	m_bufferSize;
	PRUint32	m_bufferPos;	// write Pos in m_buffer - where the next byte should go.
};


class nsMsgLineBufferHandler : public nsByteArray
{
public:
	virtual PRInt32 HandleLine(char *line, PRUint32 line_length) = 0;
};

class nsMsgLineBuffer : nsByteArray
{
public:
	nsMsgLineBuffer(nsMsgLineBufferHandler *handler, PRBool convertNewlinesP);

	virtual ~nsMsgLineBuffer();
	PRInt32	BufferInput(const char *net_buffer, PRInt32 net_buffer_size);
	// Not sure why anyone cares, by NNTPHost seems to want to know the buf pos.
	PRUint32	GetBufferPos() {return m_bufferPos;}

	virtual PRInt32 HandleLine(char *line, PRUint32 line_length);
	// flush last line, though it won't be CRLF terminated.
	virtual PRInt32 FlushLastLine();
protected:
	nsMsgLineBuffer(PRBool convertNewlinesP);

	PRInt32 ConvertAndSendBuffer();

	nsMsgLineBufferHandler *m_handler;
	PRBool		m_convertNewlinesP;
};


#endif
