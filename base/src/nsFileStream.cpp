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
 
//	First checked in on 98/12/08 by John R. McMullen.
//  Since nsFileStream.h is entirely templates, common code (such as open())
//  which does not actually depend on the charT, can be placed here.

#include "nsFileStream.h"


#include <string.h>
#include <stdio.h>


//========================================================================================
//          nsInputStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsInputStream::~nsInputStream()
//----------------------------------------------------------------------------------------
{
    mInputStream->Close();
}

//----------------------------------------------------------------------------------------
char nsInputStream::get()
//----------------------------------------------------------------------------------------
{
	char c;
    read(&c, sizeof(c));
    return c;
}

//----------------------------------------------------------------------------------------
static void TidyEndOfLine(char*& cp)
// Assumes that cp is pointing at \n or \r.  Nulls out the character, checks for
// a second terminator (of the opposite persuasion), and returns cp pointing past the
// entire eol construct (one or two characters).
//----------------------------------------------------------------------------------------
{
    char ch = *cp;
    *cp++ = '\0'; // terminate at the newline, then skip past it
    if ((ch == '\n' && *cp == '\r') || (ch == '\r' && *cp == '\n'))
        cp++; // possibly a pair.
}

//----------------------------------------------------------------------------------------
nsInputStream& nsInputStream::operator >> (char& c)
//----------------------------------------------------------------------------------------
{
	c = get();
	return *this;
}

//========================================================================================
//          nsOutputStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputStream::~nsOutputStream()
//----------------------------------------------------------------------------------------
{
    mOutputStream->Close();
}

//----------------------------------------------------------------------------------------
void nsOutputStream::put(char c)
//----------------------------------------------------------------------------------------
{
    write(&c, sizeof(c));
}

//----------------------------------------------------------------------------------------
void nsOutputStream::flush()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (char c)
//----------------------------------------------------------------------------------------
{
	put(c);
	return *this;
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (const char* s)
//----------------------------------------------------------------------------------------
{
	write(s, strlen(s));
	return *this;
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (short val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%d", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (unsigned short val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%ud", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (long val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%ld", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (unsigned long val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%uld", val);
	return (*this << buf);
}

//========================================================================================
//          nsInputFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
PRBool nsInputFileStream::readline(char* s, PRInt32 n)
// This will truncate if the buffer is too small.  Result will always be null-terminated.
//----------------------------------------------------------------------------------------
{
    PRBool bufferLargeEnough = PR_TRUE; // result
    if (!s || !n)
        return PR_TRUE;

    PRIntn position = tell();
    if (position < 0)
        return PR_FALSE;
    PRInt32 bytesRead = read(s, n - 1);
    if (failed())
        return PR_FALSE;
    s[bytesRead] = '\0'; // always terminate at the end of the buffer
    char* tp = strpbrk(s, "\n\r");
    if (tp)
    {
        TidyEndOfLine(tp);
        bytesRead = (tp - s);
    }
    else if (!eof())
        bufferLargeEnough = PR_FALSE;
    position += bytesRead;
    seek(position);
    return bufferLargeEnough;
} // nsInputStream::readline

//========================================================================================
//          nsOutputFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
void nsOutputFileStream::flush()
//----------------------------------------------------------------------------------------
{
	if (mFileOutputStream)
		mFileOutputStream->Flush();
}

//========================================================================================
//        Manipulators
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputStream& nsEndl(nsOutputStream& os)
//----------------------------------------------------------------------------------------
{
    os.put('\n');
    os.flush();
    return os;
} // nsEndl