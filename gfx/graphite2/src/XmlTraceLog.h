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
#pragma once
// XmlTraceLog - produces a log in XML format

#include <cstdio>
#include <graphite2/Types.h>
#include <graphite2/XmlLog.h>
#include <cassert>
#include "Main.h"
#include "XmlTraceLogTags.h"

namespace graphite2 {

#ifndef DISABLE_TRACING

class XmlTraceLog
{
    friend bool ::graphite_start_logging(FILE * logFile, GrLogMask mask);
    friend void ::graphite_stop_logging();
public:
    ~XmlTraceLog();
    bool active() { return (m_file != NULL); };
    void openElement(XmlTraceLogElement eId);
    void closeElement(XmlTraceLogElement eId);
    template<class T> void addArrayElement(XmlTraceLogElement eId, const T *start, int num);
    //void addArrayElement(XmlTraceLogElement eId, const byte *start, int num);
    void addSingleElement(XmlTraceLogElement eId, const int value);
    void addAttribute(XmlTraceLogAttribute aId, const char * value);
    void addAttribute(XmlTraceLogAttribute aId, float value);
    void addAttribute(XmlTraceLogAttribute aId, int value);
    void addAttribute(XmlTraceLogAttribute aId, unsigned int value);
    void addAttribute(XmlTraceLogAttribute aId, long value);
    void addAttribute(XmlTraceLogAttribute aId, unsigned long value);
    void addAttribute(XmlTraceLogAttribute aId, unsigned long long value);
    void addAttributeFixed(XmlTraceLogAttribute aId, uint32 value);
    void writeText(const char * utf8);
    void writeUnicode(const uint32 code);
    void writeElementArray(XmlTraceLogElement eId, XmlTraceLogAttribute aId, int16 * values, size_t length);
    void error(const char * msg, ...);
    void warning(const char * msg, ...);
    static XmlTraceLog & get()
    {
        return *sLog;
    }
    CLASS_NEW_DELETE
private:
    static XmlTraceLog sm_NullLog;
    static XmlTraceLog * sLog;
    XmlTraceLog(FILE * file, const char * ns, GrLogMask logMask);
private:
    void escapeIfNeeded(const char * text);
    enum {
        MAX_ELEMENT_DEPTH = 256
    };
    FILE * m_file;
    bool m_inElement;
    bool m_elementEmpty;
    bool m_lastNodeText;
    uint32 m_depth;
    GrLogMask m_mask;
    XmlTraceLogElement m_elementStack[MAX_ELEMENT_DEPTH];
};


inline void XmlTraceLog::openElement(XmlTraceLogElement eId)
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
        fprintf(m_file, "<%s", xmlTraceLogElements[eId].mName);
    }
    m_elementStack[m_depth++] = eId;
    m_inElement = true;
    m_lastNodeText = false;
}


inline void XmlTraceLog::closeElement(XmlTraceLogElement eId)
{
    if (!m_file) return;
    assert(m_depth > 0);
    assert(eId == m_elementStack[m_depth-1]);
    --m_depth;
    if (xmlTraceLogElements[eId].mFlags & m_mask)
    {
        if (m_inElement)
        {
            fprintf(m_file, "/>");
        }
        else
        {
            if (!m_lastNodeText)
            {
                fprintf(m_file, "\n");
                for (size_t i = 0; i < m_depth; i++)
                    fprintf(m_file, " ");
            }
            fprintf(m_file, "</%s>", xmlTraceLogElements[eId].mName);
        }
#ifdef ENABLE_DEEP_TRACING
        fflush(m_file);
#endif        
    }
    m_inElement = false;
    m_lastNodeText = false;
}


inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, const char * value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"", xmlTraceLogAttributes[aId]);
        escapeIfNeeded(value);
        fprintf(m_file, "\"");
    }
}


inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, float value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"%f\"", xmlTraceLogAttributes[aId], value);
    }
}


inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, int value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"%d\"", xmlTraceLogAttributes[aId], value);
    }
}


inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, unsigned int value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"%u\"", xmlTraceLogAttributes[aId], value);
    }
}

inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, long value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"%ld\"", xmlTraceLogAttributes[aId], value);
    }
}

inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, unsigned long value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"%lu\"", xmlTraceLogAttributes[aId], value);
    }
}

inline void XmlTraceLog::addAttribute(XmlTraceLogAttribute aId, unsigned long long value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        fprintf(m_file, " %s=\"%llu\"", xmlTraceLogAttributes[aId], value);
    }
}

inline void XmlTraceLog::addAttributeFixed(XmlTraceLogAttribute aId, uint32 value)
{
    if (!m_file) return;
    assert(m_inElement);
    if (xmlTraceLogElements[m_elementStack[m_depth-1]].mFlags & m_mask)
    {
        uint32 whole = (value >> 16);
        float fraction = static_cast<float>(value & 0xFFFF) / static_cast<float>(0x1FFFE);
        float fixed = whole + fraction;
        fprintf(m_file, " %s=\"%f\"", xmlTraceLogAttributes[aId], fixed);
    }
}

template<class T> void XmlTraceLog::addArrayElement(XmlTraceLogElement eId, const T *start, int num)
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
        fprintf(m_file, "<%s>\n", xmlTraceLogElements[eId].mName);
        while (num-- > 0)
        {
            for (size_t i = 0; i < m_depth + 1; i++) fprintf(m_file, " ");
            fprintf(m_file, "<val>%d</val>\n", (uint32)*start++);
        }
        for (size_t i = 0; i < m_depth; i++) fprintf(m_file, " ");
            fprintf(m_file, "</%s>", xmlTraceLogElements[eId].mName);
    }
    m_inElement = false;
    m_lastNodeText = false;
}

#endif // !DISABLE_TRACING

} // namespace graphite2
