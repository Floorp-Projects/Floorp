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
#ifndef _nsMsgLineBuffer_H
#define _nsMsgLineBuffer_H

#include "msgCore.h"    // precompiled header...

// I can't believe I have to have this stupid class, but I can't find
// anything suitable (nsStrImpl might be, when its done). nsIByteBuffer
// would do, if I had a stream for input, which I don't.

class NS_MSG_BASE nsByteArray 
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


class NS_MSG_BASE nsMsgLineBufferHandler : public nsByteArray
{
public:
	virtual PRInt32 HandleLine(char *line, PRUint32 line_length) = 0;
};

class NS_MSG_BASE nsMsgLineBuffer : public nsByteArray
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
    void SetLookingForCRLF(PRBool b);

	nsMsgLineBufferHandler *m_handler;
	PRBool		m_convertNewlinesP;
    PRBool      m_lookingForCRLF; 
};

// I'm adding this utility class here for lack of a better place. This utility class is similar to nsMsgLineBuffer
// except it works from an input stream. It is geared towards efficiently parsing new lines out of a stream by storing
// read but unprocessed bytes in a buffer. I envision the primary use of this to be our mail protocols such as imap, news and
// pop which need to process line by line data being returned in the form of a proxied stream from the server.

class nsIInputStream;

class NS_MSG_BASE nsMsgLineStreamBuffer
{
public:
	// aBufferSize -- size of the buffer you want us to use for buffering stream data
	// aEndOfLinetoken -- The delimeter string to be used for determining the end of line. This 
	//				      allows us to parse platform specific end of line endings by making it
	//					  a parameter.
	// aAllocateNewLines -- PR_TRUE if you want calls to ReadNextLine to allocate new memory for the line. 
	//						if false, the char * returned is just a ptr into the buffer. Subsequent calls to
	//						ReadNextLine will alter the data so your ptr only has a life time of a per call.
	// aEatCRLFs  -- PR_TRUE if you don't want to see the CRLFs on the lines returned by ReadNextLine. 
	//				 PR_FALSE if you do want to see them.
	nsMsgLineStreamBuffer(PRUint32 aBufferSize, PRBool aAllocateNewLines, PRBool aEatCRLFs = PR_TRUE); // specify the size of the buffer you want the class to use....
	virtual ~nsMsgLineStreamBuffer();

	// Caller must free the line returned using PR_Free
	// aEndOfLinetoken -- delimeter used to denote the end of a line.
	// aNumBytesInLine -- The number of bytes in the line returned
	// aPauseForMoreData -- There is not enough data in the stream to make a line at this time...
	char * ReadNextLine(nsIInputStream * aInputStream, PRUint32 &anumBytesInLine, PRBool &aPauseForMoreData);
  nsresult GrowBuffer(PRInt32 desiredSize);
protected:
	PRBool m_eatCRLFs;
	PRBool m_allocateNewLines;
	char * m_dataBuffer;
	PRUint32 m_dataBufferSize;
    PRUint32 m_startPos;
    PRUint32 m_numBytesInBuffer;
};


#endif
