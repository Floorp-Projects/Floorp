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

#ifndef __avmplus_AbcParser__
#define __avmplus_AbcParser__


namespace avmplus
{
	/**
	 * Parser for reading .abc (Actionscript Byte Code) files.
	 */
	class AbcParser
	{
	public:
		AbcParser(AvmCore* core, ScriptBuffer code, 
			Toplevel* toplevel,
			Domain* domain,
			AbstractFunction *nativeMethods[],
			NativeClassInfo *nativeClasses[],
			NativeScriptInfo *nativeScripts[]);

		~AbcParser();

		/**
		 * parse an .abc file
		 * @param code
		 * @return
		 */
		static PoolObject* decodeAbc(AvmCore* core, ScriptBuffer code, 
			Toplevel* toplevel,
			Domain* domain,
			AbstractFunction *nativeMethods[],
			NativeClassInfo *nativeClasses[],
			NativeScriptInfo *nativeScripts[]);

	protected:
		PoolObject* parse();
		AbstractFunction* resolveMethodInfo(uint32 index) const;

		#ifdef AVMPLUS_VERBOSE
		void parseTypeName(const byte* &p, Multiname& m) const;
		#endif

		Namespace* parseNsRef(const byte* &pc) const;
		Stringp resolveUtf8(uint32 index) const;
		Stringp parseName(const byte* &pc) const;
		Atom resolveQName(const byte* &pc, Multiname &m) const;
		int computeInstanceSize(int class_id, Traits* base) const;
		void parseMethodInfos();
		void parseMetadataInfos();
		bool parseInstanceInfos();
		void parseClassInfos();
		bool parseScriptInfos();
		void parseMethodBodies();
		void parseCpool();
		Traits* parseTraits(Traits* base, Namespace* ns, Stringp name, AbstractFunction* script, int interfaceDelta, Namespace* protectedNamespace = NULL);
		
		/**
		 * add script to VM-wide table
		 */
		void addNamedScript(Namespace* ns, Stringp name, AbstractFunction* script);

		/**
		 * Adds traits to the VM-wide traits table, for types
		 * that can be accessed from multiple abc's.
		 * @param name The name of the class
		 * @param ns The namespace of the class
		 * @param itraits The instance traits of the class
		 */
		void addNamedTraits(Namespace* ns, Stringp name, Traits* itraits);

		sint64 readS64(const byte* &p) const
		{
#ifdef SAFE_PARSE
			// check to see if we are trying to read past the file end.
			if (p < abcStart || p+7 >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE
			unsigned first  = p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
			unsigned second = p[4] | p[5]<<8 | p[6]<<16 | p[7]<<24;
			p += 8;
			return first | ((sint64)second)<<32;
		}

        /**
         * Reads in 2 bytes and turns them into a 16 bit number.  Always reads in 2 bytes.  Currently
         * only used for version number of the ABC file and for version 11 support.
         */
		int readU16(const byte* p) const
		{
#ifdef SAFE_PARSE
			if (p < abcStart || p+1 >= abcEnd)
				toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE
			return p[0] | p[1]<<8;
		}

        /**
         * Read in a 32 bit number that is encoded with a variable number of bytes.  The value can 
         * take up 1-5 bytes depending on its value.  0-127 takes 1 byte, 128-16383 takes 2 bytes, etc.
         * The scheme is that if the current byte has the high bit set, then the following byte is also
         * part of the value.  
         *
         * Returns the value, and the 2nd argument is set to the number of bytes that were read to get that
         * value.
         */
		int readS32(const byte *&p) const
		{
#ifdef SAFE_PARSE
			// We have added kBufferPadding bytes to the end of the main swf buffer.
			// Why?  Here we can read from 1 to 5 bytes.  If we were to
			// put the required safety checks at each byte read, we would slow
			// parsing of the file down.  With this buffer, only one check at the
			// top of this function is necessary. (we will read on into our own memory)
		    if ( p < abcStart || p >= abcEnd )
				toplevel->throwVerifyError(kCorruptABCError);
#endif //SAFE_PARSE

			int result = p[0];
			if (!(result & 0x00000080))
			{
				p++;
				return result;
			}
			result = (result & 0x0000007f) | p[1]<<7;
			if (!(result & 0x00004000))
			{
				p += 2;
				return result;
			}
			result = (result & 0x00003fff) | p[2]<<14;
			if (!(result & 0x00200000))
			{
				p += 3;
				return result;
			}
			result = (result & 0x001fffff) | p[3]<<21;
			if (!(result & 0x10000000))
		{
				p += 4;
				return result;
			}
			result = (result & 0x0fffffff) | p[4]<<28;
			p += 5;
			return result;
		}

		unsigned int readU30(const byte *&p) const;

	private:
		Toplevel* const		toplevel;
		Domain* const       domain;
		AvmCore*		core;
		ScriptBuffer	code;
		int				version;
		PoolObject*		pool;
		const byte*			pos;
		AbstractFunction **nativeMethods;
		NativeClassInfo **nativeClasses;
		NativeScriptInfo **nativeScripts;
		int				classCount;
		List<Traits*, LIST_GCObjects>		instances;
		byte* abcStart;
		byte* abcEnd; // one past the end, actually
		Stringp* metaNames;
		Stringp kNeedsDxns;
#ifdef AVMPLUS_VERBOSE
		Stringp kVerboseVerify;
#endif
#ifdef FEATURE_BUFFER_GUARD // no Carbon
		BufferGuard *guard;
#endif
		void addTraits(Hashtable *ht, Traits *traits, Traits *baseTraits);

	};

}

#endif // __avmplus_AbcParser__
