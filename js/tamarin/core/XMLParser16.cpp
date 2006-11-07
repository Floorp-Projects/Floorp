/* ***** BEGIN LICENSE BLOCK ***** 
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1 
 *
 * The contents of this file are subject to the Mozilla Public License Version 1.1 (the 
 * "License"); you may not use this file except in compliance with the License. You may obtain 
 * a copy of the License at http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis, WITHOUT 
 * WARRANTY OF ANY KIND, either express or implied. See the License for the specific 
 * language governing rights and limitations under the License. 
 * 
 * The Original Code is [Open Source Virtual Machine.] 
 * 
 * The Initial Developer of the Original Code is Adobe System Incorporated.  Portions created 
 * by the Initial Developer are Copyright (C)[ 1993-2006 ] Adobe Systems Incorporated. All Rights 
 * Reserved. 
 * 
 * Contributor(s): Adobe AS3 Team
 * 
 * Alternatively, the contents of this file may be used under the terms of either the GNU 
 * General Public License Version 2 or later (the "GPL"), or the GNU Lesser General Public 
 * License Version 2.1 or later (the "LGPL"), in which case the provisions of the GPL or the 
 * LGPL are applicable instead of those above. If you wish to allow use of your version of this 
 * file only under the terms of either the GPL or the LGPL, and not to allow others to use your 
 * version of this file under the terms of the MPL, indicate your decision by deleting provisions 
 * above and replace them with the notice and other provisions required by the GPL or the 
 * LGPL. If you do not delete the provisions above, a recipient may use your version of this file 
 * under the terms of any one of the MPL, the GPL or the LGPL. 
 * 
 ***** END LICENSE BLOCK ***** */

#include "avmplus.h"

namespace avmplus
{
	wchar *stripPrefix(const wchar *str, const char *pre)
		// If str begins with pre, return the first char after in str
	{
		if ((!str) || (!pre))
			return 0;
		for (;;) {
			// Map to uppercase
			wchar s = *str;
			if ( s >= 'a' && s <= 'z' )
				s -= 'a' - 'A';
			unsigned char p = *pre;
			if ( p >= 'a' && p <= 'z' )
				p -= 'a' - 'A';

			// See if the characters are not equal or we hit the end of the strings
			if ( s != p || !s || !p ) 
				break;

			*pre++; *str++;
		}
		return *pre == 0 ? const_cast<wchar*>(str) : 0;
	}

	//
	// XMLParser
	//	

	// !!@ I'm not sure what this was supposed to do originally but I've rewritten it
	// to remove the leading and trailing white space for text elements.  
	// "     5     4     3     " becomes "5     4     3"
	// This is to simulate the E4X XML parser
	void XMLParser::condenseWhitespace(Stringp text)
	{
		AvmAssert (!text->isInterned());
		wchar *str = text->lockBuffer();
		int len = text->length();

		wchar *dst = str;
		wchar *src = str;
		bool leadingWhite = true;
		wchar *lastChar = 0;

		while (len--) {
			if (String::isSpace(*src)) {
				if (!leadingWhite) {
					*dst++ = *src;
				}
				src++;
			} else {
				leadingWhite = false; // first non-space char, no more 
				lastChar = dst;
				*dst++ = *src++;
			}
		}

		if (lastChar)
			lastChar[1] = 0;

		*dst = 0;

        text->unlockBuffer((lastChar ? (lastChar + 1) : dst)-str);
	}

	int XMLParser::getNext(XMLTag& tag)
	{
		tag.reset();

		// If there's nothing left, exit.
		if (!m_ptr || !*m_ptr) {
			return XMLParser::kEndOfDocument;
		}

		// R41
		// If the ignore whitespace flag is on, don't produce
		// all-whitespace text nodes.
		if (m_ignoreWhite) {
			const wchar *ptr = m_ptr;
			while (String::isSpace(*ptr)) {
				ptr++;
			}
			if (*ptr == '<' || !*ptr) {
				// If we reached the end of the document,
				// or we reached a tag, skip all the
				// whitesapce, because it would turn into
				// an empty text node.
				m_ptr = ptr;
			}
			// If there's nothing left, exit.
			// But only do it for Flash 6 because we want
			// to exactly preserve Flash 5 behavior.
			if (!*m_ptr) {
				return XMLParser::kEndOfDocument;
			}
		}
		// end R41

		// If it starts with <, it's an XML element.
		// If it doesn't, it must be a text element.
		if (*m_ptr != '<') {
			// Treat it as text.  Scan up to the next < or until EOF.
			const wchar *start = m_ptr;
			while (*m_ptr && *m_ptr != '<') {
				m_ptr++;
			}
			tag.text = unescape(m_source, start, m_ptr-start, false);

			// Condense whitespace if desired
			if (m_ignoreWhite && m_condenseWhite) {
				condenseWhitespace(tag.text);
			}

			tag.nodeType = XMLTag::kTextNodeType;
			return XMLParser::kNoError;
		}

		// Is this a <?xml> declaration?
		wchar *temp;
		if ((temp = stripPrefix(m_ptr, "<?xml ")) != NULL) {
			// Scan forward for "?>"
			const wchar *start = m_ptr;
			m_ptr = temp;
			while (*m_ptr) {
				if (m_ptr[0] == '?' && m_ptr[1] == '>') 
				{
					// We have the end of the XML declaration
					// !!@ changed to not return <?...?> parts
					tag.text = new (core->GetGC()) String(start + 2, m_ptr - start - 2);
					m_ptr += 2;
					tag.nodeType = XMLTag::kXMLDeclaration;
					return XMLParser::kNoError;
				}
				else
				{
					m_ptr++;
				}
			}
			return XMLParser::kUnterminatedXMLDeclaration;
		}

		// Is this a <!DOCTYPE> declaration?
		if ((temp = stripPrefix(m_ptr, "<!DOCTYPE")) != NULL) {
			// Scan forward for '>'.
			const wchar *start = m_ptr;
			m_ptr = temp;
			int depth = 0;
			while (*m_ptr) {
				if (*m_ptr == '<') {
					depth++;
				}
				if (*m_ptr == '>') {
					if (!depth) {
						// We've reached the end of the DOCTYPE.
						m_ptr++;
						tag.text = new (core->GetGC()) String(start, m_ptr-start);
						tag.nodeType = XMLTag::kDocTypeDeclaration;
						return XMLParser::kNoError;
					}
					depth--;
				}
				m_ptr++;
			}
			return XMLParser::kUnterminatedDocTypeDeclaration;
		}

		// Is this a CDATA section?
		wchar *cdata;
		if ((cdata = stripPrefix(m_ptr, "<![CDATA[")) != NULL) {
			// Scan forward for "]]>"
			m_ptr = cdata;
			while (*m_ptr) {
				if (m_ptr[0] == ']' && m_ptr[1] == ']' && m_ptr[2] == '>') {
					// We have the end of the CDATA section.
					tag.text = new (core->GetGC()) String(cdata, m_ptr-cdata);
					tag.nodeType = XMLTag::kCDataSection;
					m_ptr += 3;
					return XMLParser::kNoError;
				}
				m_ptr++;
			}
			return XMLParser::kUnterminatedCDataSection;
		}

		// Is this a processing instruction?
		wchar *pi;
		if ((pi = stripPrefix(m_ptr, "<?")) != NULL) {
			// Scan forward for "?>"
			m_ptr = pi;
			while (*m_ptr) {
				if (m_ptr[0] == '?' && m_ptr[1] == '>') {
					// We have the end of the processing instruction.
					tag.text = new (core->GetGC()) String(pi, m_ptr - pi);
					tag.nodeType = XMLTag::kProcessingInstruction;
					m_ptr += 2;
					return XMLParser::kNoError;
				}
				m_ptr++;
			}
			return XMLParser::kUnterminatedProcessingInstruction;
		}

		// Advance past the "<"
		m_ptr++;

		// Is this a comment?  Return a comment tag->
		const wchar *comment;
		if (m_ptr[0] == '!' && m_ptr[1] == '-' && m_ptr[2] == '-') {
			// Skip up to '-->'.
			m_ptr += 3;
			comment = m_ptr;
			while (*m_ptr) {
				if (m_ptr[0] == '-' && m_ptr[1] == '-' && m_ptr[2] == '>') 
				{
					tag.text = new (core->GetGC()) String(comment, m_ptr-comment);
					tag.nodeType = XMLTag::kComment;
					m_ptr += 3;
					return XMLParser::kNoError;
				}
				m_ptr++;
			}
			// Got to the end of the buffer without finding a new tag->
			return XMLParser::kUnterminatedComment;
		}


		// Extract the tag name.  Scan up to ">" or whitespace.
		const wchar *tagStart = m_ptr;
		while (!String::isSpace(*m_ptr) && *m_ptr != '>') {
			if (*m_ptr == '/' && *(m_ptr+1) == '>') {
				// Found close of an empty element.
				// Exit!
				break;
			}
			if (!*m_ptr) {
				// Premature end!
				return XMLParser::kMalformedElement;
			}
			m_ptr++;
		}

		// Give up if tag name is empty
		if (m_ptr == tagStart) {
			return XMLParser::kMalformedElement;
		}

		tag.text = unescape(m_source, tagStart, m_ptr-tagStart, true);

		tag.nodeType = XMLTag::kElementType;

		// Extract attributes.
		for (;;) {
			if (!*m_ptr) {
				// Premature end!
				return XMLParser::kMalformedElement;
			}

			// Skip any whitespace.
			while (String::isSpace(*m_ptr)) {
				m_ptr++;
			}

			if (*m_ptr == '>') {
				break;
			}

			if (*m_ptr == '/' && *(m_ptr+1) == '>') {
				// Found close of an empty element.
				// Exit!
				tag.empty = true;
				m_ptr++;
				break;
			}

			// Extract the attribute name.
			const wchar *nameStart = m_ptr;
			while (!String::isSpace(*m_ptr) && *m_ptr != '=' && *m_ptr != '>') {
				if (!*m_ptr) {
					// Premature end!
					return XMLParser::kMalformedElement;
				}
				m_ptr++;
			}
			if (m_ptr == nameStart) {
				// Empty attribute name?
				return XMLParser::kMalformedElement;
			}

			Stringp attributeName = unescape(m_source, nameStart, m_ptr-nameStart, true);

			while (String::isSpace(*m_ptr)) {
				m_ptr++;
			}
			if (*m_ptr != '=') {
				// No '=' sign, no attribute value, error!
				return XMLParser::kMalformedElement;
			} else {
				// Skip over whitespace.
				while (String::isSpace(*++m_ptr))
					;
				const wchar *attrStart = m_ptr;
				// Extract the attribute value.
				if (*m_ptr != '"' && *m_ptr != '\'') {
					// Error; no opening quote for attribute value.
					return XMLParser::kMalformedElement;
				}
				wchar delimiter = *m_ptr;
				// Extract up to the next quote.
				attrStart++;
				while (*++m_ptr != delimiter) {
					if (*m_ptr == '<') {
						// '<' is not permitted in an attribute value
						// Changed this from kMalformedElement to kUnterminatedAttributeValue for bug 117058(105422)
						return XMLParser::kUnterminatedAttributeValue;
					}
					if (!*m_ptr) {
						// If at end of file, 
						// we have an unterminated attribute value on our hands.
						return XMLParser::kUnterminatedAttributeValue;
					}
				}
				const wchar *attrEnd = m_ptr;
				m_ptr++;

				Stringp attributeValue = unescape(m_source, attrStart, attrEnd-attrStart, false);

				AvmAssert (attributeName->isInterned());
				tag.attributes.add(attributeName);
				tag.attributes.add(attributeValue);
			}
		}

		// Advance past the end > of this element.
		if (*m_ptr == '>') {
			m_ptr++;
		}

		return XMLParser::kNoError;
	}

	Stringp XMLParser::unescape(Stringp text, const wchar *startChar, int len, bool bIntern)
	{
		bool bUseSubString = true;
		for (int i = 0; i < len; i++)
		{
			if (startChar[i] == '&')
			{
				bUseSubString = false;
				break;
			}
		}

		if (bUseSubString)
		{
			if (bIntern)
			{
				return core->internAlloc (startChar, len);
			}
			else
			{
				MMgc::GC* gc = MMgc::GC::GetGC(text);
				int start = startChar - text->c_str();
				AvmAssert (start < text->length());
				return new (gc) String (text, start, len);
			}
		}

		MMgc::GC* gc = MMgc::GC::GetGC(text);
		Stringp news = new (gc) String (startChar, len);
		wchar *buffer = news->lockBuffer();
	
		// Remove XML &#xx; escape entities, and &lt; &gt; &amp; &apos;
		wchar *dst = buffer;
		wchar *src = buffer;

		while (*src) {
			if (*src == '&') {
				bool success = false;
				// Scan forward to the ';'
				wchar *endPtr = src;
				while (*endPtr && *endPtr != ';') {
					endPtr++;
				}
				if (*endPtr) {
					*endPtr = 0;
					int len = endPtr-src-1;

					if (*(src+1) == '#') {
						// Parse a &#xx; decimal sequence.  Or a &#xDD hex sequence
						double value = MathUtils::parseInt(src+2, len-1);
						if (MathUtils::isNaN(value)) {
							if (len > 2 && src[2] == 'x') {
								// Handle xFF hex encoded tags, too				
								value = MathUtils::parseInt(src+3, len-2, 16);
							}
						}
						if (!MathUtils::isNaN(value)) {
							*dst++ = (wchar) (int) value;
							success = true;
						}
					} else if (len <= 4) // Our xmlEntities are only 4 characters or less
					{
						Atom entityAtom = core->internAlloc(src+1, len)->atom();
						Atom result = core->xmlEntities->get(entityAtom);
						if (result != undefinedAtom) {
							*dst++ = (wchar)(result>>3);
							success = true;
						}
					}
					*endPtr = ';';
				}
				if (success) {
					// If successful, advance past the sequence
					src = endPtr+1;
				} else {
					// Otherwise copy the sequence literally
					*dst++ = *src++;
				}
			} else {
				*dst++ = *src++;
			}
		}
		*dst = 0;

		news->unlockBuffer(dst-buffer);
		return (bIntern) ? core->internString (news) : news;
	}

	XMLParser::XMLParser(AvmCore *core)
	{
		this->core = core;

		if (!core->xmlEntities)
		{
			// Lazy creation of the XML entities table.
			core->xmlEntities = new (core->GetGC()) Hashtable(core->GetGC());

			const char *entities = "&amp\0\"quot\0'apos\0<lt\0>gt\0\xA0nbsp\0";
		
			while (*entities)
			{
				core->xmlEntities->add(core->constant(entities+1),
							   (void*)core->intToAtom(*entities));
				while (*entities++) {
					// do nothing
				}
			}
		}
	}

	void XMLParser::parse(Stringp source,
						  bool ignoreWhite /*=false*/ )
	{
		m_source = source;
		m_ptr = m_source->c_str();
		m_ignoreWhite = ignoreWhite;
	}

	bool XMLTag::nextAttribute(uint32& index,
							   Stringp& name,
							   Stringp& value)
	{
		if (index >= attributes.size()) {
			return false;
		}
		name  = attributes.get(index++);
		value = attributes.get(index++);
		return true;
	}

} // namespace
