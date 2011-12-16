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

#include "Main.h"
#include "graphite2/Segment.h"

namespace graphite2 {

class NoLimit		//relies on the processor.processChar() failing, such as because of a terminating nul character
{
public:
    NoLimit(gr_encform enc2, const void* pStart2) : m_enc(enc2), m_pStart(pStart2) {}
    gr_encform enc() const { return m_enc; }
    const void* pStart() const { return m_pStart; }

    bool inBuffer(const void* /*pCharLastSurrogatePart*/, uint32 /*val*/) const { return true; }
    bool needMoreChars(const void* /*pCharStart*/, size_t /*nProcessed*/) const { return true; }
    
private:
    gr_encform m_enc;
    const void* m_pStart;
};


class CharacterCountLimit
{
public:
    CharacterCountLimit(gr_encform enc2, const void* pStart2, size_t numchars) : m_numchars(numchars), m_enc(enc2), m_pStart(pStart2) {}
    gr_encform enc() const { return m_enc; }
    const void* pStart() const { return m_pStart; }

    bool inBuffer (const void* /*pCharLastSurrogatePart*/, uint32 val) const { return (val != 0); }
    bool needMoreChars (const void* /*pCharStart*/, size_t nProcessed) const { return nProcessed<m_numchars; }
    
private:
    size_t m_numchars;
    gr_encform m_enc;
    const void* m_pStart;
};


class BufferLimit
{
public:
    BufferLimit(gr_encform enc2, const void* pStart2, const void* pEnd/*as in stl i.e. don't use end*/) : m_enc(enc2), m_pStart(pStart2) {
	size_t nFullTokens = (static_cast<const char*>(pEnd)-static_cast<const char *>(m_pStart))/int(m_enc); //rounds off partial tokens
	m_pEnd = static_cast<const char *>(m_pStart) + (nFullTokens*int(m_enc));
    }
    gr_encform enc() const { return m_enc; }
    const void* pStart() const { return m_pStart; }
  
    bool inBuffer (const void* pCharLastSurrogatePart, uint32 /*val*/) const { return pCharLastSurrogatePart<m_pEnd; }	//also called on charstart by needMoreChars()

    bool needMoreChars (const void* pCharStart, size_t /*nProcessed*/) const { return inBuffer(pCharStart, 1); }
     
private:
    const void* m_pEnd;
    gr_encform m_enc;
    const void* m_pStart;
};


class IgnoreErrors
{
public:
    //for all of the ignore* methods is the parameter is false, the return result must be true
    static bool ignoreUnicodeOutOfRangeErrors(bool /*isBad*/) { return true; }
    static bool ignoreBadSurrogatesErrors(bool /*isBad*/) { return true; }

    static bool handleError(const void* /*pPositionOfError*/) { return true;}
};


class BreakOnError
{
public:
    BreakOnError() : m_pErrorPos(NULL) {}
    
    //for all of the ignore* methods is the parameter is false, the return result must be true
    static bool ignoreUnicodeOutOfRangeErrors(bool isBad) { return !isBad; }
    static bool ignoreBadSurrogatesErrors(bool isBad) { return !isBad; }

    bool handleError(const void* pPositionOfError) { m_pErrorPos=pPositionOfError; return false;}

public:
    const void* m_pErrorPos;
};





/*
  const int utf8_extrabytes_lut[16] = {0,0,0,0,0,0,0,0,        // 1 byte
                                          3,3,3,3,  // errors since trailing byte, catch later
                                          1,1,            // 2 bytes
                                          2,                 // 3 bytes
                                          3};                // 4 bytes
   quicker to implement directly:
*/

inline unsigned int utf8_extrabytes(const unsigned int topNibble) { return (0xE5FF0000>>(2*topNibble))&0x3; }

inline unsigned int utf8_mask(const unsigned int seq_extra) { return ((0xFEC0>>(4*seq_extra))&0xF)<<4; }

class Utf8Consumer
{
public:
    Utf8Consumer(const uint8* pCharStart2) : m_pCharStart(pCharStart2) {}
    
    const uint8* pCharStart() const { return m_pCharStart; }

private:
    template <class ERRORHANDLER>
    bool respondToError(uint32* pRes, ERRORHANDLER* pErrHandler) {       //return value is if should stop parsing
        *pRes = 0xFFFD;
        if (!pErrHandler->handleError(m_pCharStart)) {
            return false;
        }                          
        ++m_pCharStart; 
        return true;
    }
    
public:
    template <class LIMIT, class ERRORHANDLER>
    inline bool consumeChar(const LIMIT& limit, uint32* pRes, ERRORHANDLER* pErrHandler) {			//At start, limit.inBuffer(m_pCharStart) is true. return value is iff character contents does not go past limit
        const unsigned int seq_extra = utf8_extrabytes(*m_pCharStart >> 4);        //length of sequence including *m_pCharStart is 1+seq_extra
        if (!limit.inBuffer(m_pCharStart+(seq_extra), *m_pCharStart)) {
            return false;
        }
    
        *pRes = *m_pCharStart ^ utf8_mask(seq_extra);
        
        if (seq_extra) {
            switch(seq_extra) {    //hopefully the optimizer will implement this as a jump table. If not the above if should cover the majority case.    
                case 3: {	
                    if (pErrHandler->ignoreUnicodeOutOfRangeErrors(*m_pCharStart>=0xF8)) {		//the good case
                        ++m_pCharStart;
                        if (!pErrHandler->ignoreBadSurrogatesErrors((*m_pCharStart&0xC0)!=0x80)) {
                            return respondToError(pRes, pErrHandler);
                        }           
                        
                        *pRes <<= 6; *pRes |= *m_pCharStart & 0x3F;		//drop through
                    }
                    else {
                        return respondToError(pRes, pErrHandler);
                    }		    
                }
                case 2: {
                    ++m_pCharStart;
                    if (!pErrHandler->ignoreBadSurrogatesErrors((*m_pCharStart&0xC0)!=0x80)) {
                        return respondToError(pRes, pErrHandler);
                    }
                }           
                *pRes <<= 6; *pRes |= *m_pCharStart & 0x3F;       //drop through
                case 1: {
                    ++m_pCharStart;
                    if (!pErrHandler->ignoreBadSurrogatesErrors((*m_pCharStart&0xC0)!=0x80)) {
                        return respondToError(pRes, pErrHandler);
                    }
                }           
                *pRes <<= 6; *pRes |= *m_pCharStart & 0x3F;
             }
        }
        ++m_pCharStart; 
        return true;
    }	
  
private:
    const uint8 *m_pCharStart;
};



class Utf16Consumer
{
private:
    static const unsigned int SURROGATE_OFFSET = 0x10000 - 0xDC00;

public:
      Utf16Consumer(const uint16* pCharStart2) : m_pCharStart(pCharStart2) {}
      
      const uint16* pCharStart() const { return m_pCharStart; }
  
private:
    template <class ERRORHANDLER>
    bool respondToError(uint32* pRes, ERRORHANDLER* pErrHandler) {       //return value is if should stop parsing
        *pRes = 0xFFFD;
        if (!pErrHandler->handleError(m_pCharStart)) {
            return false;
        }                          
        ++m_pCharStart; 
        return true;
    }
    
public:
      template <class LIMIT, class ERRORHANDLER>
      inline bool consumeChar(const LIMIT& limit, uint32* pRes, ERRORHANDLER* pErrHandler)			//At start, limit.inBuffer(m_pCharStart) is true. return value is iff character contents does not go past limit
      {
	  *pRes = *m_pCharStart;
      if (0xD800 > *pRes || pErrHandler->ignoreUnicodeOutOfRangeErrors(*pRes >= 0xE000)) {
          ++m_pCharStart;
          return true;
      }
      
      if (!pErrHandler->ignoreBadSurrogatesErrors(*pRes >= 0xDC00)) {        //second surrogate is incorrectly coming first
          return respondToError(pRes, pErrHandler);
      }

      ++m_pCharStart;
	  if (!limit.inBuffer(m_pCharStart, *pRes)) {
	      return false;
	  }

	  uint32 ul = *(m_pCharStart);
	  if (!pErrHandler->ignoreBadSurrogatesErrors(0xDC00 > ul || ul > 0xDFFF)) {
          return respondToError(pRes, pErrHandler);
	  }
	  ++m_pCharStart;
	  *pRes =  (*pRes<<10) + ul + SURROGATE_OFFSET;
	  return true;
      }

private:
      const uint16 *m_pCharStart;
};


class Utf32Consumer
{
public:
      Utf32Consumer(const uint32* pCharStart2) : m_pCharStart(pCharStart2) {}
      
      const uint32* pCharStart() const { return m_pCharStart; }
  
private:
    template <class ERRORHANDLER>
    bool respondToError(uint32* pRes, ERRORHANDLER* pErrHandler) {       //return value is if should stop parsing
        *pRes = 0xFFFD;
        if (!pErrHandler->handleError(m_pCharStart)) {
            return false;
        }                          
        ++m_pCharStart; 
        return true;
    }

public:
      template <class LIMIT, class ERRORHANDLER>
      inline bool consumeChar(const LIMIT& limit, uint32* pRes, ERRORHANDLER* pErrHandler)			//At start, limit.inBuffer(m_pCharStart) is true. return value is iff character contents does not go past limit
      {
	  *pRes = *m_pCharStart;
      if (pErrHandler->ignoreUnicodeOutOfRangeErrors(!(*pRes<0xD800 || (*pRes>=0xE000 && *pRes<0x110000)))) {
          if (!limit.inBuffer(++m_pCharStart, *pRes))
            return false;
          else
            return true;
      }
      
      return respondToError(pRes, pErrHandler);
      }

private:
      const uint32 *m_pCharStart;
};




/* The following template function assumes that LIMIT and CHARPROCESSOR have the following methods and semantics:

class LIMIT
{
public:
    SegmentHandle::encform enc() const;		//which of the below overloads of inBuffer() and needMoreChars() are called
    const void* pStart() const;			//start of first character to process
  
    bool inBuffer(const uint8* pCharLastSurrogatePart) const;	//whether or not the input is considered to be in the range of the buffer.
    bool inBuffer(const uint16* pCharLastSurrogatePart) const;	//whether or not the input is considered to be in the range of the buffer.

    bool needMoreChars(const uint8* pCharStart, size_t nProcessed) const; //whether or not the input is considered to be in the range of the buffer, and sufficient characters have been processed.
    bool needMoreChars(const uint16* pCharStart, size_t nProcessed) const; //whether or not the input is considered to be in the range of the buffer, and sufficient characters have been processed.
    bool needMoreChars(const uint32* pCharStart, size_t nProcessed) const; //whether or not the input is considered to be in the range of the buffer, and sufficient characters have been processed.
};

class ERRORHANDLER
{
public:
    //for all of the ignore* methods is the parameter is false, the return result must be true
    bool ignoreUnicodeOutOfRangeErrors(bool isBad) const;
    bool ignoreBadSurrogatesErrors(bool isBad) const;

    bool handleError(const void* pPositionOfError);     //returns true iff error handled and should continue
};

class CHARPROCESSOR
{
public:
    bool processChar(uint32 cid);		//return value indicates if should stop processing
    size_t charsProcessed() const;	//number of characters processed. Usually starts from 0 and incremented by processChar(). Passed in to LIMIT::needMoreChars
};

Useful reusable examples of LIMIT are:
NoLimit		//relies on the CHARPROCESSOR.processChar() failing, such as because of a terminating nul character
CharacterCountLimit //doesn't care about where the input buffer may end, but limits the number of unicode characters processed.
BufferLimit	//processes how ever many characters there are until the buffer end. characters straggling the end are not processed.
BufferAndCharacterCountLimit //processes a maximum number of characters there are until the buffer end. characters straggling the end are not processed.

Useful examples of ERRORHANDLER are IgnoreErrors, BreakOnError.
*/

template <class LIMIT, class CHARPROCESSOR, class ERRORHANDLER>
void processUTF(const LIMIT& limit/*when to stop processing*/, CHARPROCESSOR* pProcessor, ERRORHANDLER* pErrHandler)
{
     uint32             cid;
     switch (limit.enc()) {
       case gr_utf8 : {
        const uint8 *pInit = static_cast<const uint8 *>(limit.pStart());
	    Utf8Consumer consumer(pInit);
        for (;limit.needMoreChars(consumer.pCharStart(), pProcessor->charsProcessed());) {
            const uint8 *pCur = consumer.pCharStart();
		    if (!consumer.consumeChar(limit, &cid, pErrHandler))
		        break;
		    if (!pProcessor->processChar(cid, pCur - pInit))
		        break;
        }
        break; }
       case gr_utf16: {
        const uint16* pInit = static_cast<const uint16 *>(limit.pStart());
        Utf16Consumer consumer(pInit);
        for (;limit.needMoreChars(consumer.pCharStart(), pProcessor->charsProcessed());) {
            const uint16 *pCur = consumer.pCharStart();
    		if (!consumer.consumeChar(limit, &cid, pErrHandler))
	    	    break;
		    if (!pProcessor->processChar(cid, pCur - pInit))
		        break;
            }
	    break;
        }
       case gr_utf32 : default: {
        const uint32 *pInit = static_cast<const uint32 *>(limit.pStart());
	    Utf32Consumer consumer(pInit);
        for (;limit.needMoreChars(consumer.pCharStart(), pProcessor->charsProcessed());) {
            const uint32 *pCur = consumer.pCharStart();
		    if (!consumer.consumeChar(limit, &cid, pErrHandler))
		        break;
		    if (!pProcessor->processChar(cid, pCur - pInit))
		        break;
            }
        break;
        }
    }
}

    class ToUtf8Processor
    {
    public:
        // buffer length should be three times the utf16 length or
        // four times the utf32 length to cover the worst case
        ToUtf8Processor(uint8 * buffer, size_t maxLength) :
            m_count(0), m_byteLength(0), m_maxLength(maxLength), m_buffer(buffer)
        {}
        bool processChar(uint32 cid, size_t /*offset*/)
        {
            // taken from Unicode Book ch3.9
            if (cid <= 0x7F)
                m_buffer[m_byteLength++] = cid;
            else if (cid <= 0x07FF)
            {
                if (m_byteLength + 2 >= m_maxLength)
                    return false;
                m_buffer[m_byteLength++] = 0xC0 + (cid >> 6);
                m_buffer[m_byteLength++] = 0x80 + (cid & 0x3F);
            }
            else if (cid <= 0xFFFF)
            {
                if (m_byteLength + 3 >= m_maxLength)
                    return false;
                m_buffer[m_byteLength++] = 0xE0 + (cid >> 12);
                m_buffer[m_byteLength++] = 0x80 + ((cid & 0x0FC0) >> 6);
                m_buffer[m_byteLength++] = 0x80 +  (cid & 0x003F);
            }
            else if (cid <= 0x10FFFF)
            {
                if (m_byteLength + 4 >= m_maxLength)
                    return false;
                m_buffer[m_byteLength++] = 0xF0 + (cid >> 18);
                m_buffer[m_byteLength++] = 0x80 + ((cid & 0x3F000) >> 12);
                m_buffer[m_byteLength++] = 0x80 + ((cid & 0x00FC0) >> 6);
                m_buffer[m_byteLength++] = 0x80 +  (cid & 0x0003F);
            }
            else
            {
                // ignore
            }
            m_count++;
            if (m_byteLength >= m_maxLength)
                return false;
            return true;
        }
        size_t charsProcessed() const { return m_count; }
        size_t bytesProcessed() const { return m_byteLength; }
    private:
        size_t m_count;
        size_t m_byteLength;
        size_t m_maxLength;
        uint8 * m_buffer;
    };

    class ToUtf16Processor
    {
    public:
        // buffer length should be twice the utf32 length
        // to cover the worst case
        ToUtf16Processor(uint16 * buffer, size_t maxLength) :
            m_count(0), m_uint16Length(0), m_maxLength(maxLength), m_buffer(buffer)
        {}
        bool processChar(uint32 cid, size_t /*offset*/)
        {
            // taken from Unicode Book ch3.9
            if (cid <= 0xD800)
                m_buffer[m_uint16Length++] = cid;
            else if (cid < 0xE000)
            {
                // skip for now
            }
            else if (cid >= 0xE000 && cid <= 0xFFFF)
                m_buffer[m_uint16Length++] = cid;
            else if (cid <= 0x10FFFF)
            {
                if (m_uint16Length + 2 >= m_maxLength)
                    return false;
                m_buffer[m_uint16Length++] = 0xD800 + ((cid & 0xFC00) >> 10) + ((cid >> 16) - 1);
                m_buffer[m_uint16Length++] = 0xDC00 + ((cid & 0x03FF) >> 12);
            }
            else
            {
                // ignore
            }
            m_count++;
            if (m_uint16Length == m_maxLength)
                return false;
            return true;
        }
        size_t charsProcessed() const { return m_count; }
        size_t uint16Processed() const { return m_uint16Length; }
    private:
        size_t m_count;
        size_t m_uint16Length;
        size_t m_maxLength;
        uint16 * m_buffer;
    };

    class ToUtf32Processor
    {
    public:
        ToUtf32Processor(uint32 * buffer, size_t maxLength) :
            m_count(0), m_maxLength(maxLength), m_buffer(buffer) {}
        bool processChar(uint32 cid, size_t /*offset*/)
        {
            m_buffer[m_count++] = cid;
            if (m_count == m_maxLength)
                return false;
            return true;
        }
        size_t charsProcessed() const { return m_count; }
    private:
        size_t m_count;
        size_t m_maxLength;
        uint32 * m_buffer;
    };

} // namespace graphite2
