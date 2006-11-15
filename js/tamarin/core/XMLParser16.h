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
 * by the Initial Developer are Copyright (C)[ 2004-2006 ] Adobe Systems Incorporated. All Rights 
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

#ifndef __avmplus_XMLParser16__
#define __avmplus_XMLParser16__


namespace avmplus
{
	/**
	 * XMLTag represents a single XML tag.  It is output by the
	 * XMLParser class, the XML parser.
	 */
	class XMLTag
	{
	public:
		XMLTag(MMgc::GC *gc) : attributes(gc, 0)
		{
			reset();
		}

		void reset()
		{
			text = NULL;
			nodeType = kNoType;
			empty = false;
			attributes.clear();
		}

		enum TagType {
			kNoType                = 0,
			// These match the W3C XML DOM SPEC
			// http://www.w3.org/TR/2000/REC-DOM-Level-2-Core-20001113/core.html
			// (Search for ELEMENT_NODE)
			kElementType           = 1,
			kTextNodeType          = 3,
			kCDataSection          = 4,
			kProcessingInstruction = 7,
			kComment			   = 8,
			kDocTypeDeclaration    = 10,

			// This does not match the W3C XML DOM SPEC
			kXMLDeclaration        = 13
		};

		Stringp text;
		enum TagType nodeType;
		bool empty;
		List<Stringp, LIST_RCObjects> attributes;

		/**
		 * nextAttribute is used to iterate over the
		 * attributes of a XML tag
		 */
		bool nextAttribute(uint32& index,
						   Stringp& name,
						   Stringp& value);
	};

	/**
	 * XMLParser is a XML parser which takes 16-bit wide characters
	 * as input.  The parser operates in "pull" fashion, returning a
	 * single tag or text node on each call to the GetNext method.
	 *
	 * This XML parser is used to support E4X in AVM+.
	 */
	class XMLParser
	{
	public:
		XMLParser(AvmCore *core);
		~XMLParser()
		{
			core = NULL;
			m_source = NULL;
			m_ptr = NULL;
			m_ignoreWhite = false;
			m_condenseWhite = false;
		}

		void parse(Stringp source,
				   bool ignoreWhite = false);

		int getNext(XMLTag& tag);
	
		enum {
			kNoError                           = 0,
			kEndOfDocument                     = -1,
			kUnterminatedCDataSection          = -2,
			kUnterminatedXMLDeclaration        = -3,
			kUnterminatedDocTypeDeclaration    = -4,
			kUnterminatedComment               = -5,
			kMalformedElement                  = -6,
			kOutOfMemory                       = -7,
			kUnterminatedAttributeValue        = -8,
			kUnterminatedElement               = -9,
			kElementNeverBegun                 = -10,
			kUnterminatedProcessingInstruction = -11
		};

		AvmCore* core;
	
		bool getCondenseWhite() const { return m_condenseWhite; }
		void setCondenseWhite(bool flag) { m_condenseWhite = flag; }

	private:
		Stringp unescape(Stringp buffer, const wchar *start, int len, bool bIntern);

		Stringp m_source;
		const wchar *m_ptr;

		bool m_ignoreWhite;
		bool m_condenseWhite;

		void condenseWhitespace(Stringp text);
	};
}

#endif 
