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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

// icalfrdr.cpp
// John Sun
// 3:03 PM February 10 1998

#include <stdlib.h>
#include <stdio.h>

#include "stdafx.h"
#include "jdefines.h"

#include "icalfrdr.h"
#include "icalredr.h"
//---------------------------------------------------------------------

ICalFileReader::ICalFileReader() {}

//---------------------------------------------------------------------

ICalFileReader::ICalFileReader(char * filename, ErrorCode & status)
: m_file(0), m_filename(0)
{
    m_filename = filename;
    m_file = fopen(filename, "r");

    if (m_file == 0) 
    {
#if 0
        if (FALSE) TRACE("Can't open %s\n", filename);
#endif
        status = 1;
    }
    else
    {
        status = ZERO_ERROR;
    }
}
//---------------------------------------------------------------------
ICalFileReader::~ICalFileReader()
{
    if (m_file) { fclose(m_file); m_file = 0; }
}

//---------------------------------------------------------------------

t_int8 ICalFileReader::read(ErrorCode & status)
{
    int c = fgetc(m_file);
    if (c == EOF)
    {
        status = 1;
        return -1;
    }
    else 
        return (t_int8) c;
}
//---------------------------------------------------------------------
// TODO: handle quoted-printable
UnicodeString & 
ICalFileReader::readLine(UnicodeString & aLine, ErrorCode & status)
{
    status = ZERO_ERROR;
    aLine = "";
    char * l = 0;

	if ( 0 != (l = fgets(m_pBuffer,1023,m_file)) )
	{
		t_int32 iLen = strlen(m_pBuffer);
		if (m_pBuffer[iLen-1] == '\n')
			m_pBuffer[iLen-1] = 0;
        {
            aLine = m_pBuffer;
            return aLine;
        }
	}
    status = 1;
    return aLine;
}
//---------------------------------------------------------------------
// NOTE: TODO: make faster profiling?
UnicodeString & 
ICalFileReader::readFullLine(UnicodeString & aLine, ErrorCode & status, t_int32 iTemp)
{
    status = ZERO_ERROR;
    t_int8 i;
    t_bool qp = FALSE;
    
    readLine(aLine, status);
    //if (FALSE) TRACE("rfl(1) %s\r\n", aLine.toCString(""));

    if (FAILURE(status))
    {
        return aLine;
    }

    if (aLine.indexOf("QUOTED-PRINTABLE") >= 0 || aLine.indexOf("quoted-printable") >= 0)
      qp = TRUE;
    if (qp)
    {
      // convert string after ':'
      t_int32 i;
      i = aLine.indexOf(':');
      if (i >= 0)
      {
        UnicodeString u;
        u = aLine.extractBetween(i + 1, aLine.size(), u);
        u = ICalReader::convertQuotedPrintableString(u);
        aLine.replaceBetween(i + 1, aLine.size(), u);
      }
      if (aLine[(TextOffset) aLine.size() - 1] == '=')
        aLine.remove((TextOffset) aLine.size() - 1, 1);
      else
        qp = FALSE;
    }

    UnicodeString aSubLine;
    while (TRUE)
    {
        i = read(status);
        if (i != -1 && i == ' ' || (qp && i != -1))
        {
            if (!qp)
            {
              aLine += readLine(aSubLine, status);
            }
            else 
            {
              aLine += i;
              aSubLine = readLine(aSubLine, status);
              // convert aSubLine;
              aSubLine = ICalReader::convertQuotedPrintableString(aSubLine);
              // remove last '=' if it exists, if it doesn't we're done.
              if (aSubLine[(TextOffset) aSubLine.size() - 1] == '=')
                aSubLine.remove((TextOffset) aSubLine.size() - 1, 1);
              else
                qp = FALSE;

              aLine += aSubLine;
            }
        }
        else if (i == -1)
        {
            return aLine;
        }
        else
        {
            ungetc(i, m_file);
            break;
        }
    }
    //if (FALSE) TRACE("end of rfl: ---%s---\r\n", aLine.toCString(""));
    return aLine;
}
//---------------------------------------------------------------------

