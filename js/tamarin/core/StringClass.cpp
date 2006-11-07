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


#include "avmplus.h"

namespace avmplus
{
#ifdef __MWERKS__
	typedef void (String::*StringHandler)();
	
	NativeTableEntry::Handler stringMethod(StringHandler stringHandler)
	{
		union 
		{
			StringHandler foo;
			NativeTableEntry::Handler bar;
		};
		foo = stringHandler;
		return bar;
	}
	
	#define STRING_METHOD(handler) stringMethod((StringHandler)handler)
#else
	#define STRING_METHOD(x) x
#endif
	
	BEGIN_NATIVE_MAP(StringClass)
		// instance methods
		NATIVE_METHOD2_FLAGS(String_length_get, STRING_METHOD(&String::length), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_localeCompare, STRING_METHOD(&String::localeCompare), AbstractFunction::NEED_REST)

		NATIVE_METHOD2_FLAGS(String_private__indexOf, STRING_METHOD(&String::indexOf), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_indexOf, STRING_METHOD(&String::indexOfDouble), 0)

		NATIVE_METHOD2_FLAGS(String_private__lastIndexOf, STRING_METHOD(&String::lastIndexOf), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_lastIndexOf, STRING_METHOD(&String::lastIndexOfDouble), 0)

		NATIVE_METHOD2_FLAGS(String_private__charAt, STRING_METHOD(&String::charAt), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_charAt, STRING_METHOD(&String::charAtDouble), 0)

		NATIVE_METHOD2_FLAGS(String_private__charCodeAt, STRING_METHOD(&String::charCodeAt), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_charCodeAt, STRING_METHOD(&String::charCodeAtDouble), 0)

		NATIVE_METHOD2_FLAGS(String_private__substring, STRING_METHOD(&String::substring), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_substring, STRING_METHOD(&String::substringDouble), 0)

		NATIVE_METHOD2_FLAGS(String_private__slice, STRING_METHOD(&String::slice), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_slice, STRING_METHOD(&String::sliceDouble), 0)

		NATIVE_METHOD2_FLAGS(String_private__substr, STRING_METHOD(&String::substr), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_substr, STRING_METHOD(&String::substrDouble), 0)

		NATIVE_METHOD2_FLAGS(String_AS3_toLowerCase, STRING_METHOD(&String::toLowerCase), 0)
		NATIVE_METHOD2_FLAGS(String_AS3_toUpperCase, STRING_METHOD(&String::toUpperCase), 0)

		// static method (language feature - by design)
		NATIVE_METHODV(String_AS3_fromCharCode, StringClass::fromCharCode)

		// static methods (require AvmCore *)
		NATIVE_METHOD2(String_private__match, &StringClass::match)
		NATIVE_METHOD2(String_private__replace, &StringClass::replace)
		NATIVE_METHOD2(String_private__search, &StringClass::search)
		NATIVE_METHOD2(String_private__split, &StringClass::split)

	END_NATIVE_MAP()
		
	StringClass::StringClass(VTable* cvtable)
		: ClassClosure(cvtable)
	{
		toplevel()->stringClass = this;
        createVanillaPrototype();

		// Some sanity tests for string/wchar* comparison routines
#if 0 && defined(_DEBUG)
		Stringp a = core()->newString ("a", 1);
		Stringp b = core()->newString ("b", 1);
		Stringp c = core()->newString ("c", 1);

		AvmAssert( (*a == *a));
		AvmAssert(!(*a != *a));
		AvmAssert( (*a >= *a));
		AvmAssert( (*a <= *a));
		AvmAssert(!(*a > *a));
		AvmAssert(!(*a < *a));
	 
		AvmAssert(!(*a == *b));
		AvmAssert( (*a != *b));
		AvmAssert(!(*a >= *b));
		AvmAssert( (*a <= *b));
		AvmAssert(!(*a > *b));
		AvmAssert( (*a < *b));

		AvmAssert(!(*c == *b));
		AvmAssert( (*c != *b));
		AvmAssert( (*c >= *b));
		AvmAssert(!(*c <= *b));
		AvmAssert( (*c > *b));
		AvmAssert(!(*c < *b));

		wchar d[2];
		d[0] = 'a';
		d[1] = 0;

		AvmAssert( (d == *a));
		AvmAssert(!(d != *a));
		AvmAssert( (d >= *a));
		AvmAssert( (d <= *a));
		AvmAssert(!(d > *a));
		AvmAssert(!(d < *a));

		AvmAssert( (*a == d));
		AvmAssert(!(*a != d));
		AvmAssert( (*a >= d));
		AvmAssert( (*a <= d));
		AvmAssert(!(*a > d));
		AvmAssert(!(*a < d));

		AvmAssert(!(d == *b));
		AvmAssert( (d != *b));
		AvmAssert(!(d >= *b));
		AvmAssert( (d <= *b));
		AvmAssert(!(d > *b));
		AvmAssert( (d < *b));

		wchar e[2];
		e[0] = 'b';
		e[1] = 0;

		AvmAssert(!(*c == e));
		AvmAssert( (*c != e));
		AvmAssert( (*c >= e));
		AvmAssert(!(*c <= e));
		AvmAssert( (*c > e));
		AvmAssert(!(*c < e));
#endif
	}

	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom StringClass::construct(int argc, Atom* argv)
	{
		if (argc == 0) {
			return core()->kEmptyString->atom();
		} else {
			return core()->string(argv[1])->atom();
		}
		// TODO ArgumentError if argc > 1
	}
	
	// this = argv[0] (ignored)
	// arg1 = argv[1]
	// argN = argv[argc]
	Atom StringClass::call(int argc, Atom* argv)
	{
		return construct(argc, argv);
	}

	Stringp StringClass::fromCharCode(Atom *argv, int argc)
	{
		AvmCore* core = this->core();
		Stringp out = new (core->GetGC()) String(argc);
		wchar *ptr = out->lockBuffer();

		for (int i=0; i<argc; i++) {
			*ptr++ = wchar(core->integer(argv[i]));
		}
		*ptr = 0;

		out->unlockBuffer();
		return out;
	}

	ArrayObject *StringClass::match(Stringp in, Atom regexpAtom)
	{
		AvmCore* core = this->core();

		if (!core->istype(regexpAtom, core->traits.regexp_itraits))
		{
			// ECMA-262 15.5.4.10
			// If the argument is not a RegExp, invoke RegExp(exp)
			regexpAtom = core->newRegExp(toplevel()->regexpClass(),
										 core->string(regexpAtom),
										 core->kEmptyString)->atom();
		}

		RegExpObject *reObj = (RegExpObject*) AvmCore::atomToScriptObject(regexpAtom);
		return reObj->match(in);
	}

	Stringp StringClass::replace(Stringp subject, Atom pattern, Atom replacementAtom)
	{
		AvmCore* core = this->core();

		ScriptObject *replaceFunction = NULL;
		Stringp replacement = NULL;
		if (core->istype(replacementAtom, core->traits.function_itraits)) {
			replaceFunction = AvmCore::atomToScriptObject(replacementAtom);
		} else {
			replacement = core->string(replacementAtom);
		}

		if (core->istype(pattern, core->traits.regexp_itraits)) {
			// RegExp mode
			RegExpObject *reObj = (RegExpObject*) core->atomToScriptObject(pattern);
			if (replaceFunction) {
				return core->string(reObj->replace(subject, replaceFunction));
			} else {
				return core->string(reObj->replace(subject, replacement));
			}
			
		} else {
			// String replace mode
			Stringp searchString = core->string(pattern);
			
			int index = subject->indexOf(searchString);
			if (index == -1) {
				// Search string not found; return input unchanged.
				return subject;
			}

			if (replaceFunction) {
				// Invoke the replacement function to figure out the
				// replacement string
				Atom argv[4] = { undefinedAtom,
								 searchString->atom(),
								 core->uintToAtom(index),
								 subject->atom() };
				replacement = core->string(toplevel()->op_call(replaceFunction->atom(),
															   3, argv));
			}

			int newlen = subject->length() - searchString->length() + replacement->length();

			Stringp out = new (core->GetGC()) String(newlen);

			wchar *buffer = out->lockBuffer();
			memcpy(buffer, subject->c_str(), index*sizeof(wchar));
			memcpy(buffer+index, replacement->c_str(), replacement->length()*sizeof(wchar));
			memcpy(buffer+index+replacement->length(),
				   subject->c_str()+index+searchString->length(),
				   (subject->length()-searchString->length()-index+1)*sizeof(wchar));
			buffer[newlen] = 0;
			out->unlockBuffer();

			return out;
		}
	}

	int StringClass::search(Stringp in, Atom regexpAtom)
	{
		AvmCore* core = this->core();

		if (!core->istype(regexpAtom, core->traits.regexp_itraits)) {
			// ECMA-262 15.5.4.10
			// If the argument is not a RegExp, invoke RegExp(exp)
			regexpAtom = core->newRegExp(toplevel()->regexpClass(),
												core->string(regexpAtom),
												core->kEmptyString)->atom();
		}

		RegExpObject *reObj = (RegExpObject*) AvmCore::atomToScriptObject(regexpAtom);
		return reObj->search(in);
	}

	ArrayObject* StringClass::split(Stringp in, Atom delimAtom, uint32 limit)
    {
		AvmCore* core = this->core();

		if (limit == 0)
			return toplevel()->arrayClass->newArray();

		if (in->length() == 0)
		{
			ArrayObject* out = toplevel()->arrayClass->newArray();
			out->setUintProperty(0,in->atom());
			return out;
		}

		// handle RegExp case
		if (core->istype(delimAtom, core->traits.regexp_itraits))
		{
			RegExpObject *reObj = (RegExpObject*) AvmCore::atomToScriptObject(delimAtom);			
			return reObj->split(in, limit);
		}		

		ArrayObject *out = toplevel()->arrayClass->newArray();		
        Stringp delim = core->string(delimAtom);
		
        int ilen = in->length();
        int dlen = delim->length();
        int count = 0;
        int start = 0;
        int w = 0;

		if (dlen <= 0)
		{
			// delim is empty string, split on each char
			for (int i = 0; i < ilen && (unsigned)i < limit; i++)
			{
				Stringp sub = new (core->GetGC()) String(in, i, 1);
				out->setUintProperty(count++, sub->atom());
			}
			return out;
		}

		const wchar *delimch = delim->c_str();
		wchar dlast = delimch[dlen-1];
		while (delimch[w] != dlast)
			w++;

        //loop1:
		unsigned numSeg = 0;
		const wchar *inchar = in->c_str();
		for (int i = w; i < ilen; i++)
        {
			bool continue_loop1 = false;
			wchar c = inchar[i];
            if (c == dlast)
            {
                int k = i-1, j;
				for (j=dlen-2; j >= 0; j--, k--) {
					if (inchar[k] != delimch[j]) {
						continue_loop1 = true;
						break;
					}
				}
				if (!continue_loop1) {
					numSeg++;

					// if we have found more segments than 
					// the limit we can stop looking
					if( numSeg > limit )
						break;

					int sublen=k+1-start;
					Stringp sub = new (core->GetGC()) String(in, start, sublen);

					out->setUintProperty(count++, sub->atom());
					
					start = i+1;
					i += w;
				}
            }
        }

		// if numSeg is less than limit when we're done, add the rest of
		// the string to the last element of the array
		if( numSeg < limit )
        {
			Stringp sub = new (core->GetGC()) String(in, start, ilen);
            out->setUintProperty(count, sub->atom());
        }
        return out;
    }
}
