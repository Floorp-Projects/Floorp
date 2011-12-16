/*  GRAPHITE2 LICENSING

    Copyright 2010, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street, 
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the 
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/

#include <cstring>
#include <cstdarg>
#include "Main.h"
#include "XmlTraceLog.h"


using namespace graphite2;

#ifndef DISABLE_TRACING

/*static*/ XmlTraceLog XmlTraceLog::sm_NullLog(NULL, NULL, GRLOG_NONE);
XmlTraceLog * XmlTraceLog::sLog = &sm_NullLog;

XmlTraceLog::XmlTraceLog(FILE * file, const char * ns, GrLogMask logMask)
                         : m_file(file), m_depth(0), m_mask(logMask)
{
    if (!m_file) return;
    int deep = 0;
#ifdef ENABLE_DEEP_TRACING
    deep = 1;
#endif
    fprintf(m_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<%s xmlns=\"%s\" mask=\"%x\" deep=\"%d\">",
            xmlTraceLogElements[ElementTopLevel].mName, ns, logMask, deep);
    m_elementStack[m_depth++] = ElementTopLevel;
    m_elementEmpty = true;
    m_inElement = false;
    m_lastNodeText = false;
}

XmlTraceLog::~XmlTraceLog()
{
    if (m_file && m_file != stdout && m_file != stderr)
    {
        assert(m_depth == 1);
        while (m_depth > 0)
        {
            closeElement(m_elementStack[m_depth-1]);
        }
        fclose(m_file);
        m_file = NULL;
    }
}

void XmlTraceLog::addSingleElement(XmlTraceLogElement eId, const int value)
{
    if (!m_file) return;
    if (m_inElement)
    {
        if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
            fprintf(m_file, ">");
    }
    if (xmlTraceLogElements[eId].mFlags & m_mask)
    {
        if (!m_lastNodeText)
        {
            fprintf(m_file, "\n");
            for (size_t i = 0; i < m_depth; i++)
            {
                fprintf(m_file, " ");
            }
        }
        fprintf(m_file, "<%s val=\"%d\"/>", xmlTraceLogElements[eId].mName, value);
    }
    m_inElement = false;
    m_lastNodeText = false;
}
    
void XmlTraceLog::writeElementArray(XmlTraceLogElement eId, XmlTraceLogAttribute aId, int16 values [], size_t length)
{
    if (!m_file) return;
    if (xmlTraceLogElements[eId].mFlags & m_mask)
    {
        if(m_inElement && xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
        {
            fprintf(m_file, ">");
            m_inElement = false;
        }
        // line break after 5 columns
        for (size_t i = 0; i < length; i++)
        {
            if (i % 5 == 0)
            {
                fprintf(m_file, "\n");
                for (size_t j = 0; j < m_depth; j++)
                {
                    fprintf(m_file, " ");
                }
            }
            fprintf(m_file, "<%s index=\"%d\" %s=\"%d\"/>", xmlTraceLogElements[eId].mName, int(i),
                xmlTraceLogAttributes[aId], (int)values[i]);
        }
    }
}

void XmlTraceLog::writeText(const char * utf8)
{
    if (!m_file) return;
    if (m_inElement)
    {
        if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
        {
            fprintf(m_file, ">");
        }
        m_inElement = false;
    }
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        escapeIfNeeded(utf8);
    }
    m_lastNodeText = true;
}

void XmlTraceLog::writeUnicode(const uint32 code)
{
    if (!m_file) return;
    if (m_inElement)
    {
        if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
        {
            fprintf(m_file, ">");
        }
        m_inElement = false;
    }
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, "&#x%02x;", code);
    }
    m_lastNodeText = true;
}

void XmlTraceLog::escapeIfNeeded(const char * data)
{
    size_t length = strlen(data);
    for (size_t i = 0; i < length; i++)
    {
        switch (data[i])
        {
            case '<':
                fprintf(m_file, "&lt;");
                break;
            case '>':
                fprintf(m_file, "&gt;");
                break;
            case '&':
                fprintf(m_file, "&amp;");
                break;
            case '"':
                fprintf(m_file, "&#34;");
                break;
            default:
                fprintf(m_file, "%c", data[i]);
        }
    }
}

static const int MAX_MSG_LEN = 1024;

void XmlTraceLog::error(const char * msg, ...)
{
    if (!m_file) return;
    openElement(ElementError);
    va_list args;
    va_start(args, msg);
    char buffer[MAX_MSG_LEN];
#ifndef NDEBUG
    int len =
#endif
        vsnprintf(buffer, MAX_MSG_LEN, msg, args);
    assert(len + 1 < MAX_MSG_LEN);
    writeText(buffer);
    va_end(args);
    closeElement(ElementError);
}

void XmlTraceLog::warning(const char * msg, ...)
{
    if (!m_file) return;
    openElement(ElementWarning);
    va_list args;
    va_start(args, msg);
    char buffer[MAX_MSG_LEN];
#ifndef NDEBUG
    int len =
#endif
        vsnprintf(buffer, MAX_MSG_LEN, msg, args);
    assert(len + 1 < MAX_MSG_LEN);
    writeText(buffer);
    va_end(args);
    closeElement(ElementWarning);
}

#endif		//!DISABLE_TRACING
