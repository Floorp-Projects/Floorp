/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "avmplus.h"

#ifdef _MAC
#ifndef __GNUC__
// inline_max_total_size() defaults to 10000.
// This module includes so many inline functions that we
// exceed this limit and we start getting compile warnings,
// so bump up the limit for this file. 
#pragma inline_max_total_size(100000)
#endif
#endif

using namespace MMgc;

namespace avmplus
{
	BEGIN_NATIVE_MAP(ArrayClass)
		NATIVE_METHOD_FLAGS(Array_length_get, ArrayObject::getLength, 0)
		NATIVE_METHOD_FLAGS(Array_length_set, ArrayObject::setLength, 0)
		NATIVE_METHOD_FLAGS(Array_AS3_pop, ArrayObject::pop, 0)
		NATIVE_METHOD_FLAGS(Array_AS3_push, ArrayObject::push, 0)
		NATIVE_METHOD_FLAGS(Array_AS3_unshift, ArrayObject::unshift, 0)

		NATIVE_METHOD(Array_private__pop, ArrayClass::pop)
		NATIVE_METHOD(Array_private__reverse, ArrayClass::reverse)
		NATIVE_METHOD(Array_private__concat, ArrayClass::concat)
		NATIVE_METHOD(Array_private__shift, ArrayClass::shift)
		NATIVE_METHOD(Array_private__slice, ArrayClass::slice)
		NATIVE_METHOD(Array_private__splice, ArrayClass::splice)
		NATIVE_METHOD(Array_private__sort, ArrayClass::sort)
		NATIVE_METHOD(Array_private__sortOn, ArrayClass::sortOn)

		// Array extensions that are in Mozilla...
		// http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array
		//
		// These all work on generic objects (array like objects) as well as arrays
		NATIVE_METHOD(Array_private__indexOf, ArrayClass::indexOf)
		NATIVE_METHOD(Array_private__lastIndexOf, ArrayClass::lastIndexOf)
		NATIVE_METHOD(Array_private__every, ArrayClass::every)
		NATIVE_METHOD(Array_private__filter, ArrayClass::filter)
		NATIVE_METHOD(Array_private__forEach, ArrayClass::forEach)
		NATIVE_METHOD(Array_private__map, ArrayClass::map)
		NATIVE_METHOD(Array_private__some, ArrayClass::some)

	END_NATIVE_MAP()

	// todo while close, the implementations here do not exactly match the
	// ECMA spec, and probably in ways that the SpiderMonkey test suite won't
	// catch... for instance, it will go haywire with array indices over
	// 2^32 because we're not applying the ToInteger algorithm correctly.
	// Indices have to handled as doubles until the last minute, then...
	// the full 2^32 range is needed but there is behavior involving
	// negative numbers too.
	
	ArrayClass::ArrayClass(VTable* cvtable)
	: ClassClosure(cvtable), 
	  kComma(core()->constantString(","))
	{
		AvmCore* core = this->core();
		Toplevel* toplevel = this->toplevel();

		toplevel->arrayClass = this;
		AvmAssert(traits()->sizeofInstance == sizeof(ArrayClass));

		VTable* ivtable = this->ivtable();
		ScriptObject* objectPrototype = toplevel->objectClass->prototype;
		prototype = new (core->GetGC(), ivtable->getExtraSize()) ArrayObject(ivtable, objectPrototype, 0);

		// According to ECMAscript spec (ECMA-262.pdf)
		// generic support: concat, join, pop, push, reverse, shift, slice, sort, splice, unshift
		// NOT generic: toString, toLocaleString
		// unknown: sortOn (our own extension)

#if defined(MMGC_DRC) && defined(_DEBUG)

		// AtomArray DRC unit tests, put here b/c this always runs once, has a GC * and
		// this class has to do with arrays.  this is more convienent that trying to test
		// through actionscript

		// create an atom
		Stringp s = core->newString("foo");
		Atom a = s->atom();
		AvmAssert(s->RefCount()==0);
		AtomArray *ar = new (gc()) AtomArray();
		
		// test push/pop
		ar->push(a); AvmAssert(s->RefCount()==1);
		ar->push(a); AvmAssert(s->RefCount()==2);
		ar->pop(); AvmAssert(s->RefCount()==1);

		// reverse
		ar->push(a); AvmAssert(s->RefCount()==2);
		ar->reverse(); AvmAssert(s->RefCount()==2);
		
		// shift
		ar->shift(); AvmAssert(s->RefCount()==1);

		// splice
		AtomArray *ar2 = new (gc()) AtomArray();
		ar->push(a);
		ar2->push(ar); AvmAssert(s->RefCount()==4);
		ar->splice(1, 2, 1, ar2, 0);  // [a,a,a]
		AvmAssert(s->RefCount()==5);

		// unshift
		Atom as[4] = {a,a,a,a};
		ar->unshift(as, 4);
		AvmAssert(s->RefCount()==9);

		// removeAt
		ar->removeAt(1); AvmAssert(s->RefCount()==8);

		// setAt
		ar->setAt(2, a); AvmAssert(s->RefCount()==8);

		// insert
		ar->insert(2, a); AvmAssert(s->RefCount()==9);

		// clear
		ar->clear(); AvmAssert(s->RefCount() == 2);
		ar2->clear(); AvmAssert(s->RefCount() == 0);

		gc()->Free(ar);
		gc()->Free(ar2);
#endif
	}
	
	/**
	15.4.4.4 Array.prototype.concat ( [ item1 [ , item2 [ , 
 ] ] ] )
	When the concat method is called with zero or more arguments item1, item2, etc., it returns an array containing
	the array elements of the object followed by the array elements of each argument in order.
	The following steps are taken:
	1. Let A be a new array created as if by the expression new Array().
	2. Let n be 0.
	3. Let E be this object.
	4. If E is not an Array object, go to step 16.
	5. Let k be 0.
	6. Call the [[Get]] method of E with argument "length".
	7. If k equals Result(6) go to step 19.
	8. Call ToString(k).
	9. If E has a property named by Result(8), go to step 10, 
	but if E has no property named by Result(8), go to step 13.
	10. Call ToString(n).
	11. Call the [[Get]] method of E with argument Result(8).
	12. Call the [[Put]] method of A with arguments Result(10) and Result(11).
	13. Increase n by 1.
	14. Increase k by 1.
	15. Go to step 7.
	16. Call ToString(n).
	17. Call the [[Put]] method of A with arguments Result(16) and E.
	18. Increase n by 1.
	19. Get the next argument in the argument list; if there are no more arguments, go to step 22.
	20. Let E be Result(19).
	21. Go to step 4.
	22. Call the [[Put]] method of A with arguments "length" and n.
	23. Return A.
	The length property of the concat method is 1.
	NOTE The concat function is intentionally generic; it does not require that its this value be an Array object. Therefore it can be
	transferred to other kinds of objects for use as a method. Whether the concat function can be applied successfully to a host
	object is implementation-dependent.
	*/
	ArrayObject* ArrayClass::concat(Atom thisAtom, ArrayObject* args)
    {
		AvmCore* core = this->core();
		ScriptObject *d = AvmCore::isObject(thisAtom) ? AvmCore::atomToScriptObject(thisAtom) : 0;
		
		uint32 len = 0;
		if (d)
		{
			len = getLengthHelper (d);
		}

        ArrayObject *a = isArray(thisAtom);
		uint32 i;

		uint32 argc = args->getLength();

		int  newLength = len;
		for (i = 0; i< argc; i++) 
		{
			Atom atom = args->getUintProperty(i);
			if (core->istype(atom, ARRAY_TYPE)) 
			{
				ArrayObject *b = (ArrayObject*) AvmCore::atomToScriptObject(atom);
				newLength += b->getLength();
			}
			else
			{
				newLength++;
			}
		}

		ArrayObject *out = newArray(newLength);
		int denseLength = 0;
		// Only copy over elements for Arrays, not objects according to spec
		// 4. If E is not an Array object, go to step 16.
		if (a && newLength)
		{
			denseLength = a->getDenseLength();

			// copy over our dense part
			out->m_denseArr.push (&a->m_denseArr);
			out->m_length += denseLength;

			// copy over any non-dense values (or all values if this isn't an array object)
			for (i = denseLength; i < len; i++) {
				out->setUintProperty(i, d->getUintProperty(i));
			}
		}

		for (i = 0; i< (uint32)argc; i++) 
		{
			Atom atom = args->getUintProperty(i);
			if (core->istype(atom, ARRAY_TYPE)) 
			{
				ArrayObject *b = (ArrayObject*) AvmCore::atomToScriptObject(atom);
				// copy over dense parts
				out->m_denseArr.push (&b->m_denseArr);
				out->m_length += b->getDenseLength();

				// copy over any non-dense values
				uint32 len = b->getLength();
				for (uint32 j=b->getDenseLength(); j<len; j++) {
					out->m_denseArr.push (b->getUintProperty(j));
					out->m_length++;
				}
			}
			else
			{
				out->m_denseArr.push (atom);
				out->m_length++;
			}
		}

		return out;
	}

	/**
     * Array.prototype.pop()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */
	Atom ArrayClass::pop(Atom thisAtom)
    {
        ArrayObject *a = isArray(thisAtom);

		if (a)
			return a->pop();

		if (!AvmCore::isObject(thisAtom))
			return undefinedAtom;

		// Different than Rhino (because of delete) but matches 262.pdf
		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);
		if (!len)
		{
			setLengthHelper (d, 0);
			return undefinedAtom;
		}
		else
		{
			Atom outAtom = d->getUintProperty (len-1); 
			d->delUintProperty (len - 1);
			setLengthHelper (d, len - 1);
			return outAtom;
		}
    }
	
    /**
     * Array.prototype.reverse()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */
	Atom ArrayClass::reverse(Atom thisAtom)
    {
        ArrayObject *a = isArray(thisAtom);
	
		if (a && (a->isSimpleDense()))
		{
			a->m_denseArr.reverse();
			return thisAtom;
		}

		// generic object version
		if (!AvmCore::isObject(thisAtom))
			return thisAtom;

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 j = getLengthHelper (d);

		uint32 i = 0;
		if (j)
			j--;

		while (i < j) {
			Atom frontAtom = d->getUintProperty(i);
			Atom backAtom  = d->getUintProperty(j);

			d->setUintProperty(i++, backAtom);
			d->setUintProperty(j--, frontAtom);
		}

		return thisAtom;
	}
	
	/**
     * Array.prototype.shift()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */
	Atom ArrayClass::shift(Atom thisAtom)
	{
		ArrayObject *a = isArray(thisAtom);

		if (a && a->isSimpleDense())
		{
			if (!a->m_length)
				return undefinedAtom;

			a->m_length--;
			return (a->m_denseArr.shift());
		}

		if (!AvmCore::isObject(thisAtom))
			return undefinedAtom;

		Atom outAtom;
		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		if (len == 0)
		{
			// set len to 0 (ecmascript spec)
			setLengthHelper (d, 0);
			outAtom = undefinedAtom;
		}
		else
		{
			// Get the 0th element to return
			outAtom = d->getUintProperty(0);

			// Move all of the elements down
			for (uint32 i=0; i<len-1; i++) {
				d->setUintProperty(i, d->getUintProperty(i+1));
			}
			
			d->delUintProperty (len - 1);
			setLengthHelper (d, len - 1);
		}

		return outAtom;
	}

    /**
     * Array.prototype.slice()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */
	ArrayObject* ArrayClass::slice(Atom thisAtom, double A, double B)
    {
		if (!AvmCore::isObject(thisAtom))
			return 0;

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		// if a param is passed then the first one is A
		// if no params are passed then A = 0
		uint32 a = NativeObjectHelpers::ClampIndex(A, len);
		uint32 b = NativeObjectHelpers::ClampIndex(B, len);
		if (b < a)
			b = a;

		ArrayObject *out = newArray(b-a);

		uint32 outIndex=0;
		for (uint32 i=a; i<b; i++) {
			out->setUintProperty (outIndex++, d->getUintProperty (i));
		}

		return out;
	}
	
	static inline bool defined(Atom atom) { return (atom != undefinedAtom); }
	static inline bool isInteger(Atom atom) { return ((atom&7) == kIntegerType); }

    /**
     * ArraySort implements actionscript Array.sort().
	 * It's also the base class SortWithParameters, which handles all other permutations of Array
     */
	class ArraySort
	{
	public:
		/*************************************************************
		 * Forward declarations required by public methods
		 *************************************************************/

		/** Options from script */
		enum {
            kCaseInsensitive		= 1
			,kDescending			= 2
			,kUniqueSort			= 4
			,kReturnIndexedArray	= 8
			,kNumeric				= 16
		};

		/** Array.sortOn() will pass an array of field names */
		struct FieldName 
		{
			DRCWB(Stringp) name;
			int options;
		};

		typedef int		(*CompareFuncPtr)	(const ArraySort *s, uint32 i, uint32 j);

		static int StringCompareFunc(const ArraySort *s, uint32 j, uint32 k) { return s->StringCompare(j, k); }
		static int CaseInsensitiveStringCompareFunc(const ArraySort *s, uint32 j, uint32 k) { return s->CaseInsensitiveStringCompare(j, k); }
		static int ScriptCompareFunc(const ArraySort *s, uint32 j, uint32 k) { return s->ScriptCompare(j, k); }
		static int NumericCompareFunc(const ArraySort *s, uint32 j, uint32 k) { return s->NumericCompare(j, k); }
		static int DescendingCompareFunc(const ArraySort *s, uint32 j, uint32 k) { return s->altCmpFunc(s, k, j); }
		static int FieldCompareFunc(const ArraySort *s, uint32 j, uint32 k) { return s->FieldCompare(j, k); }

	public:
		/*************************************************************
		 * Public Functions
		 *************************************************************/

		ArraySort(Atom &result, ArrayClass *f, ScriptObject *d, int options, CompareFuncPtr cmpFunc,
			CompareFuncPtr altCmpFunc, Atom cmpActionScript, uint32 numFields = 0, FieldName *fields = NULL);

		~ArraySort();

		// do the sort
		Atom sort();
	
	private:
		/*************************************************************
		 * Private Functions 
		 *************************************************************/

		// non-recursive quicksort implementation
		void qsort(uint32 lo, uint32 hi);

		// qsort() is abstracted from the array by swap() and compare()
		// compare(), in turn, is abstracted from the array via get()
		//
		// cmpFunc is conditional, for instance : 
		//		cmpFunc = DefaultCompareFunc;	// Array.sort()
		//		cmpFunc = ScriptCompareFunc;	// Array.sort(compareFunction)
		int compare(uint32 lhs, uint32 rhs)	const	{ return cmpFunc(this, lhs, rhs); }
		Atom get(uint32 i)		const	{ return atoms->getAt(index[i]); }
		void swap(uint32 j, uint32 k)
		{
			uint32 temp = index[j];
			index[j] = index[k];
			index[k] = temp;
		}

		inline int StringCompare(uint32 j, uint32 k) const;
		inline int CaseInsensitiveStringCompare(uint32 j, uint32 k) const;
		inline int ScriptCompare(uint32 j, uint32 k) const;
		inline int NumericCompare(uint32 j, uint32 k) const;
		inline int FieldCompare(uint32 j, uint32 k) const;

		/**
		 * null check + pointer cast.  only used in contexts where we know we
		 * have a ScriptObject* already (verified, etc)
		 */
		ScriptObject* toFieldObject(Atom atom) const;

	private:
		/*************************************************************
		 * Private Member Variables
		 *************************************************************/

		ScriptObject *d;
		AvmCore* core;
		Toplevel* toplevel;
		int options;

		CompareFuncPtr cmpFunc;
		CompareFuncPtr altCmpFunc;
		Atom cmpActionScript;							

		uint32 *index;
		AtomArray *atoms;

		uint32 numFields;
		FieldName *fields;
		AtomArray *fieldatoms;
	};

	ArraySort::ArraySort(
		Atom &result,
		ArrayClass *f, 
		ScriptObject *d,
		int options, 
		CompareFuncPtr cmpFunc,
		CompareFuncPtr altCmpFunc,
		Atom cmpActionScript, 
		uint32 numFields, 
		FieldName *fields
	) :
		d(d), 
		core(f->core()), 
		toplevel(f->toplevel()),
		options(options), 
		cmpFunc(cmpFunc), 
		altCmpFunc(altCmpFunc), 
		cmpActionScript(cmpActionScript), 
		index(NULL), 
		atoms(NULL), 
		numFields(numFields), 
		fields(fields),
		fieldatoms(NULL)
	{
		uint32 len = f->getLengthHelper (d);
		uint32 iFirstUndefined = len;
		uint32 iFirstAbsent = len;

		// new class[n] compiles into code which tries to allocate n * sizeof(class).
		// unfortunately, the generated assembly does not protect against this product overflowing int.
		// So I limit the length -- for larger values, I expect new will fail anyways.
		if ((len > 0) && (len < (0x10000000)))
		{

			index = new uint32[len];
			atoms = new (core->GetGC()) AtomArray(len);
			atoms->setLength(len);
		}

		if (!index || !atoms)
		{
			// return the unsorted array.

			// todo : grandma : Should I be raising an exception? Sort too big?
			// could also fallback to an unindexed sort, but with that many elements, it won't finish for hours
			result = d->atom();
			return;
		}
		
		uint32 i, j;
		uint32 newlen = len;

		// One field value - pre-get our field values so we can just do a regular sort
		if (cmpFunc == ArraySort::FieldCompareFunc && numFields == 1)
		{
			fieldatoms = new (core->GetGC()) AtomArray(len);
			fieldatoms->setLength(len);

			// note, loop needs to go until i = -1, but i is unsigned. 0xffffffff is a valid index, so check (i+1) != 0.
			for (i = (len - 1), j = len; (i+1) != 0; i--)
			{
				index[i] = i;
				Atom a = d->getUintProperty(i);
				fieldatoms->setAt(i, a);

				if (AvmCore::isObject (a))
				{
					ScriptObject* obj = AvmCore::atomToScriptObject (a);

					Stringp name = fields[0].name;
					Multiname mname(core->publicNamespace, name);
					
					// An undefined prop just becomes undefined in our sort
					Atom x = toplevel->getproperty(obj->atom(), &mname, obj->vtable);
					atoms->setAt(i, x);
				}
				else
				{
					j--;

					uint32 temp = index[i];
					index[i] = index[j];

					if (!d->hasUintProperty(i)) {
						newlen--;
						index[j] = index[newlen];
						index[newlen] = temp;
					} else {
						index[j] = temp;
					}
				}
			}

			int opt = fields[0].options;

			if (opt & ArraySort::kNumeric) {
				this->cmpFunc = ArraySort::NumericCompareFunc;
			} else if (opt & ArraySort::kCaseInsensitive) {
				this->cmpFunc = ArraySort::CaseInsensitiveStringCompareFunc;
			} else {
				this->cmpFunc = ArraySort::StringCompareFunc;
			}

			if (opt & ArraySort::kDescending) {
				this->altCmpFunc = this->cmpFunc;
				this->cmpFunc = ArraySort::DescendingCompareFunc;
			}
		}
		else
		{
			bool isNumericCompare = (cmpFunc == ArraySort::NumericCompareFunc) || (altCmpFunc == ArraySort::NumericCompareFunc);
			// note, loop needs to go until i = -1, but i is unsigned. 0xffffffff is a valid index, so check (i+1) != 0.
			for (i = (len - 1), j = len; (i+1) != 0; i--)
			{
				index[i] = i;
				atoms->setAt(i, d->getUintProperty(i));

				// We want to throw if this is an Array.NUMERIC sort and any items are not numbers,
				// and not strings that can be converted into numbers
				if(isNumericCompare && !core->isNumber(atoms->getAt(i)))
				{
					double val = core->number(atoms->getAt(i));
					if(MathUtils::isNaN(val))
						// throw exception (not a Number)
						toplevel->throwTypeError(kCheckTypeFailedError, core->atomToErrorString(atoms->getAt(i)), core->toErrorString(core->traits.number_itraits));

				}

				// getAtomProperty() returns undefined when that's the value of the property.
				// It also returns undefined when the object does not have the property.
				// 
				// SortCompare() from ECMA 262 distinguishes these two cases. The end
				// result is undefined comes after everything else, and missing properties
				// come after that.
				//
				// To simplify our compare, partitition the array into { defined | undefined | missing }
				// Note that every missing element shrinks the length -- we'll set this new
				// length at the end of this routine when we are done.

				if (!defined(atoms->getAt(i))) {
					j--;

					uint32 temp = index[i];
					index[i] = index[j];
					
					if (!d->hasUintProperty(i)) {
						newlen--;
						index[j] = index[newlen];
						index[newlen] = temp;
					} else {
						index[j] = temp;
					}
				}
			}
		}

		iFirstAbsent = newlen;
		iFirstUndefined = j;

		// The portion of the array containing defined values is now [0, iFirstUndefined).
		// The portion of the array containing values undefined is now [iFirstUndefined, iFirstAbsent).
		// The portion of the array containing absent values is now [iFirstAbsent, len).

		// now sort the remaining defined() elements
		qsort(0, j-1);
		
		if (options & kUniqueSort)
		{
			// todo : kUniqueSort could throw an exception.
			// todo : kUniqueSort could abort the sort once equal members are found
			for (uint32 i = 0; i < (len - 1); i++)
			{
				if (compare(i, (i+1)) == 0)
				{
					result = core->uintToAtom(0);
					return;
				}
			}
		}

		if (options & kReturnIndexedArray)
		{
			// return the index array without modifying the original array
			ArrayObject *obj = d->toplevel()->arrayClass->newArray(len);

			for (uint32 i = 0; i < len; i++) 
			{
				obj->setUintProperty(i, core->uintToAtom(index[i]));
			}

			result = obj->atom();
		}
		else
		{
			// If we need to use our fieldatoms as results, temporarily swap them with
			// our atoms array so the below code works on the right data. Fieldatoms contain
			// our original objects while atoms contain our objects[field] values for faster
			// sorting.
			AtomArray *temp = atoms;
			if (fieldatoms)
			{
				atoms = fieldatoms;	
			}

			for (i = 0; i < iFirstAbsent; i++) {
				d->setUintProperty(i, get(i));
			}

			for (i = iFirstAbsent; i < len; i++) {
				d->delUintProperty(i);
			}

			//a->setLength(len);  ES3: don't shrink array on sort.  Seems silly
			result = d->atom();
			atoms = temp;

		}

		return;
	}

	ArraySort::~ArraySort()
	{
		delete [] index;
		if(atoms) {
			atoms->clear();
			core->GetGC()->Free(atoms);
			atoms = NULL;
		}
		if(fieldatoms) {
			fieldatoms->clear();
			core->GetGC()->Free(fieldatoms);
			fieldatoms = NULL;
		}
		if(fields) {
			int numFields = GC::Size(fields)/sizeof(FieldName);
			for(int i=0; i<numFields; i++) {
				fields[i].name = NULL;
			}
			core->GetGC()->Free(fields);
			fields = NULL;
		}
	}

	/*
	 * QuickSort a portion of the ArrayObject.
	 */
	void ArraySort::qsort(uint32 lo, uint32 hi)
	{
		// This is an iterative implementation of the recursive quick sort.
		// Recursive implementations are basically storing nested (lo,hi) pairs
		// in the stack frame, so we can avoid the recursion by storing them
		// in an array.
		//
		// Once partitioned, we sub-partition the smaller half first. This means
		// the greatest stack depth happens with equal partitions, all the way down,
		// which would be 1 + log2(size), which could never exceed 33.

		uint32 size;
		struct StackFrame { uint32 lo, hi; };
		StackFrame stk[33];
		int stkptr = 0;

		// leave without doing anything if the array is empty (lo > hi) or only one element (lo == hi)
		if (lo >= hi)
			return;

		// code below branches to this label instead of recursively calling qsort()
	recurse:

		size = (hi - lo) + 1; // number of elements in the partition

		if (size < 4) {
		
			// It is standard to use another sort for smaller partitions,
			// for instance c library source uses insertion sort for 8 or less.
			//
			// However, as our swap() is essentially free, the relative cost of
			// compare() is high, and with profiling, I found quicksort()-ing
			// down to four had better performance.
			//
			// Although verbose, handling the remaining cases explicitly is faster,
			// so I do so here.

			if (size == 3) {
				if (compare(lo, lo + 1) > 0) {
					swap(lo, lo + 1);
					if (compare(lo + 1, lo + 2) > 0) {
						swap(lo + 1, lo + 2);
						if (compare(lo, lo + 1) > 0) {
							swap(lo, lo + 1);
						}
					}
				} else {
					if (compare(lo + 1, lo + 2) > 0) {
						swap(lo + 1, lo + 2);
						if (compare(lo, lo + 1) > 0) {
							swap(lo, lo + 1);
						}
					}
				}
			} else if (size == 2) {
				if (compare(lo, lo + 1) > 0)
					swap(lo, lo + 1);
			} else {
				// size is one, zero or negative, so there isn't any sorting to be done
			}
		} else {
			// qsort()-ing a near or already sorted list goes much better if
			// you use the midpoint as the pivot, but the algorithm is simpler
			// if the pivot is at the start of the list, so move the middle
			// element to the front!
			uint32 pivot = lo + (size / 2);
			swap(pivot, lo);


			uint32 left = lo;
			uint32 right = hi + 1;

			for (;;) {
				// Move the left right until it's at an element greater than the pivot.
				// Move the right left until it's at an element less than the pivot.
				// If left and right cross, we can terminate, otherwise swap and continue.
				//
				// As each pass of the outer loop increments left at least once,
				// and decrements right at least once, this loop has to terminate.

				do  {
					left++;
				} while ((left <= hi) && (compare(left, lo) <= 0));

				do  {
					right--;
				} while ((right > lo) && (compare(right, lo) >= 0));

				if (right < left)
					break;

				swap(left, right);
			}

			// move the pivot after the lower partition
			swap(lo, right);

			// The array is now in three partions:
			//	1. left partition	: i in [lo, right), elements less than or equal to pivot
			//	2. center partition	: i in [right, left], elements equal to pivot
			//	3. right partition	: i in (left, hi], elements greater than pivot
			// NOTE : [ means the range includes the lower bounds, ( means it excludes it, with the same for ] and ).

			// Many quick sorts recurse into the left partition, and then the right.
			// The worst case of this can lead to a stack depth of size -- for instance,
			// the left is empty, the center is just the pivot, and the right is everything else.
			//
			// If you recurse into the smaller partition first, then the worst case is an
			// equal partitioning, which leads to a depth of log2(size).
			if ((right - 1 - lo) >= (hi - left)) 
			{
				if ((lo + 1) < right) 
				{
					stk[stkptr].lo = lo;
					stk[stkptr].hi = right - 1;
					++stkptr;
				}

				if (left < hi)
				{
					lo = left;
					goto recurse;
				}
			}
			else
			{
				if (left < hi)
				{
					stk[stkptr].lo = left;
					stk[stkptr].hi = hi;
					++stkptr;
				}

				if ((lo + 1) < right)
				{
					hi = right - 1;
					goto recurse;           /* do small recursion */
				}
			}
		}

		// we reached the bottom of the well, pop the nested stack frame
		if (--stkptr >= 0)
		{
			lo = stk[stkptr].lo;
			hi = stk[stkptr].hi;
			goto recurse;
		}

		// we've returned to the top, so we are done!
		return;
	}

	/*
	 * compare(j, k) as string's
	 */
	int ArraySort::StringCompare(uint32 j, uint32 k) const 
	{
		Atom x = get(j);
		Atom y = get(k);

		Stringp str_lhs = core->string(x);
		Stringp str_rhs = core->string(y);

		return str_rhs->Compare(*str_lhs);
	}

	/*
	 * compare(j, k) as case insensitive string's
	 */
	int ArraySort::CaseInsensitiveStringCompare(uint32 j, uint32 k) const 
	{
		Atom x = get(j);
		Atom y = get(k);

		Stringp str_lhs = core->string(x)->toLowerCase();
		Stringp str_rhs = core->string(y)->toLowerCase();

		return str_rhs->Compare(*str_lhs);
	}

	/*
	 * compare(j, k) using an actionscript function
	 */
	int ArraySort::ScriptCompare(uint32 j, uint32 k) const
	{
		// todo must figure out the kosher way to invoke
		// callbacks like the sort comparator.

		// todo what is thisAtom supposed to be for the
		// comparator?  Passing in the array for now.

		ScriptObject *o = (ScriptObject*) AvmCore::atomToScriptObject(cmpActionScript);

		Atom args[3] = { d->atom(), get(j), get(k) };
		double result = core->toInteger(o->call(2, args));
		// cn: don't use core->integer on result of call.  The returned 
		//  value could be bigger than 2^32 and toInt32 will return the 
		//  ((value % 2^32) - 2^31), which could change the intended sign of the number.
		//  
		//return core->integer(o->call(a->atom(), args, 2));
		return (result > 0 ? 1 : (result < 0 ? -1 : 0));
	}

	/*
	 * compare(j, k) as numbers
	 */
	int ArraySort::NumericCompare(uint32 j, uint32 k) const
	{
		Atom atmj = get(j);
		Atom atmk = get(k);
		// Integer checks makes an int array sort about 3x faster.
		// A double array sort is 5% slower because of this overhead
		if (((atmj & 7) == kIntegerType) && ((atmk & 7) == kIntegerType))
		{
			return ((int)atmj - (int)atmk);
		}

		double x = core->number(atmj);
		double y = core->number(atmk);
		double diff = x - y;

		if (diff == diff) { // same as !isNaN
			return (diff < 0) ? -1 : ((diff > 0) ? 1 : 0);
		} else if (!MathUtils::isNaN(y)) {
			return 1;
		} else if (!MathUtils::isNaN(x)) {
			return -1;
		} else {
			return 0;
		}
	}

	ScriptObject* ArraySort::toFieldObject(Atom atom) const
	{
		if ((atom&7) != kObjectType)
		{
			#if 0
			/* cn: ifdefed out, not sure what the intent was here, but calling code in FieldCompare
			 *  does null checking, apparently expecting this function to return null when the item
			 *  isn't an object (and thus can't have custom properties added to it). */
            // TypeError in ECMA 
			toplevel->throwTypeError(
					   (atom == undefinedAtom) ? kConvertUndefinedToObjectError :
											kConvertNullToObjectError);
			#endif
			return NULL;
		}
		return AvmCore::atomToScriptObject(atom);
	}

	/*
	 * FieldCompare is for Array.sortOn()
	 */
	inline int ArraySort::FieldCompare(uint32 lhs, uint32 rhs) const
	{
		Atom j, k;
		int opt = options;
		int result = 0;

		j = get(lhs);
		k = get(rhs);

		ScriptObject* obj_j = toFieldObject(j);
		ScriptObject* obj_k = toFieldObject(k);

		if (!(obj_j && obj_k))
		{
			if (obj_k) {
				result = 1;
			} else if (obj_j) {
				result = -1;
			} else {
				result = 0;
			}
			return (opt & kDescending) ? -result : result;
		}

		for (uint32 i = 0; i < numFields; i++)
		{
			Stringp name = fields[i].name;
			Multiname mname(core->publicNamespace, name);
			
			opt = fields[i].options; // override the group defaults with the current field

			Atom x = toplevel->getproperty(obj_j->atom(), &mname, obj_j->vtable);
			Atom y = toplevel->getproperty(obj_k->atom(), &mname, obj_k->vtable);

			bool def_x = defined(x);
			bool def_y = defined(y);

			if (!(def_x && def_y))
			{
				// ECMA 262 : Section 15.4.4.11 lists the rules.
				// There is a difference between the object has a property
				// with value undefined, and it does not have the property,
				// for which getAtomProperty() returns undefined.

				// def_x implies has_x
				// def_y implies has_y

				if (def_y) {							
					result = 1;
				} else if (def_x) {
					result = -1;
				} else {
					bool has_x = (toplevel->getBinding(obj_j->vtable->traits, &mname) != BIND_NONE) || obj_j->hasStringProperty(name);
					bool has_y = (toplevel->getBinding(obj_k->vtable->traits, &mname) != BIND_NONE) || obj_k->hasStringProperty(name);

					if (!has_x && has_y) {
						result = 1;
					} else if (has_x && !has_y) {
						result = -1;
					} else {
						result = 0;
					}
				}
			} else if (opt & kNumeric) {
				double lhs = core->number(x);
				double rhs = core->number(y);
				double diff = lhs - rhs;

				if (diff == diff) { // same as !isNaN
					result = (diff < 0) ? -1 : ((diff > 0) ? 1 : 0);
				} else if (!MathUtils::isNaN(rhs)) {
					result = 1;
				} else if (!MathUtils::isNaN(lhs)) {
					result = -1;
				} else {
					result = 0;
				}
			}
			else
			{
				Stringp str_lhs = core->string(x);
				Stringp str_rhs = core->string(y);

				if (opt & kCaseInsensitive)
				{
					str_lhs = str_lhs->toLowerCase();
					str_rhs = str_rhs->toLowerCase();
				}

				result = str_rhs->Compare(*str_lhs);
			}

			if (result != 0)
				break;
		}

		if (opt & kDescending)
			return -result;
		else
			return result;
	}

    /**
     * Array.prototype.sort()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */

	// thisAtom is object to process 
	// 1st arg of args is a function or a number
	// 2nd arg of args is a number 
	//
	// valid AS3 syntax:
	// sort()
	// sort(function object)
	// sort(number flags)
	// sort(function object, number flags)

	// This takes a args object because there is no way to distinguigh between sort()
	// and sort(undefined, 0) if we take default parameters.
	Atom ArrayClass::sort(Atom thisAtom, ArrayObject *args)
    {
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom))
			return undefinedAtom;

        ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		ArraySort::CompareFuncPtr compare = NULL;
		ArraySort::CompareFuncPtr altCompare = NULL;

		Atom cmp = undefinedAtom;
		int opt = 0;
		if (args->getLength() >= 1)
		{
			// function ptr
			Atom arg0 = args->getUintProperty(0);
			if (AvmCore::isObject (arg0))
			{
				// make sure the sort function is callable
				cmp = arg0;
				toplevel()->coerce(cmp, core->traits.function_itraits);
				compare = ArraySort::ScriptCompareFunc;
				if (args->getLength() >= 2)
				{
					Atom arg1 = args->getUintProperty(1);
					if (core->isNumber(arg1))
					{
						opt = core->integer (arg1);
					}
					else
					{
						// throw exception (not a Number)
						toplevel()->throwTypeError(kCheckTypeFailedError, core->atomToErrorString(arg1), core->toErrorString(core->traits.number_itraits));
					}
				}
			}
			else if (core->isNumber(arg0))
			{
				opt = core->integer (arg0);
			}
			else
			{
				// throw exception (not a function)
				toplevel()->throwTypeError(kCheckTypeFailedError, core->atomToErrorString(arg0), core->toErrorString(core->traits.function_itraits));
			}
		}

		if (cmp == undefinedAtom)
		{
			if (opt & ArraySort::kNumeric) {
				compare = ArraySort::NumericCompareFunc;
			} else if (opt & ArraySort::kCaseInsensitive) {
				compare = ArraySort::CaseInsensitiveStringCompareFunc;
			} else {
				compare = ArraySort::StringCompareFunc;
			}
		}

		if (opt & ArraySort::kDescending) {
			altCompare = compare;
			compare = ArraySort::DescendingCompareFunc;
		}

		Atom result;
		ArraySort sort(result, this, d, opt, compare, altCompare, cmp);

		return result;
	}


    /**
     * Array.prototype.sortOn()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */
	Atom ArrayClass::sortOn(Atom thisAtom, Atom namesAtom, Atom optionsAtom)
    {
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom))
			return undefinedAtom;
		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);

		// Possible combinations:
		//	Array.sortOn(String)
		//	Array.sortOn(String, options)
		//	Array.sortOn(Array of String)
		//	Array.sortOn(Array of String, options)
		//  Array.sortOn(Array of String, Array of options)

		//	What about options which must be global, such as kReturnIndexedArray?
		//	Perhaps it is the union of all field's options?
		
		ArraySort::FieldName *fn = NULL;
		uint32 nFields = 0;
		int options = 0;
	
		if (core->istype(namesAtom, STRING_TYPE))
		{
			nFields = 1;

			options = core->integer(optionsAtom);

			fn = (ArraySort::FieldName*) core->GetGC()->Alloc(sizeof(ArraySort::FieldName), GC::kZero|GC::kContainsPointers);
			fn[0].name = core->internString(namesAtom);
			fn[0].options = options;
		}
		else if (core->istype(namesAtom, ivtable()->traits /* array itraits */))
		{
			ArrayObject *obj = (ArrayObject *)AvmCore::atomToScriptObject(namesAtom);

			nFields = obj->getLength();
			fn = (ArraySort::FieldName*) core->GetGC()->Calloc(nFields, sizeof(ArraySort::FieldName), GC::kZero|GC::kContainsPointers);

			for (uint32 i = 0; i < nFields; i++)
			{
				fn[i].name = core->internString(obj->getUintProperty(i));
				fn[i].options = 0;
			}

			if (core->istype(optionsAtom, ivtable()->traits /* array itraits */))
			{
				ArrayObject *obj = (ArrayObject *)AvmCore::atomToScriptObject(optionsAtom);
				uint32 nOptions = obj->getLength();
				if (nOptions == nFields)
				{
					// The first options are used for uniqueSort and returnIndexedArray option
					options = core->integer(obj->getUintProperty(0));
					for (uint32 i = 0; i < nFields; i++)
					{
						fn[i].options = core->integer(obj->getUintProperty (i));
					}
				}
			}
			else
			{
				options = core->integer(optionsAtom);
				for (uint32 i = 0; i < nFields; i++)
				{
					fn[i].options = options;
				}
			}
		}

		// ~ArraySort() will "delete [] fn;"
		Atom result;
		ArraySort sort(result, this, d, options, ArraySort::FieldCompareFunc, NULL, undefinedAtom, nFields, fn);
		return result;
	}

    /**
     * Array.prototype.splice()
	 * TRANSFERABLE: Needs to support generic objects as well as Array objects
     */

	// Spidermonkey behavior that we are mimicking...
	// splice() - no arguments - return undefined
	// splice(org arg) coerce the input to a number (otherwise it's zero) normal behavior
	// splice (two args) - coerce both args to numbers (otherwise they are zero)
	ArrayObject* ArrayClass::splice(Atom thisAtom,
							ArrayObject* args)
    {
		// This will return null but this case should never get hit (see Array.as)
		if (!args->getLength())
			return 0; 

		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom))
			return 0;
		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);

		uint32 len = getLengthHelper (d);
		
		uint32 start = NativeObjectHelpers::ClampIndex(core->toInteger(args->getUintProperty(0)),len);

		double d_deleteCount = args->getLength() > 1 ? core->toInteger(args->getUintProperty(1)) : (len - start); 
		uint32 deleteCount = (d_deleteCount < 0) ? 0 : core->integer_d(d_deleteCount);
		if (deleteCount > (len - start)) {
			deleteCount = len - start;
        }
		uint32 end = start + deleteCount;

		// Copy out the elements we are going to remove
		ArrayObject *out = newArray(deleteCount);
		uint32 i;

		// !!@ add faster version when both arrays are simpleDense
		for (i=0; i< deleteCount; i++) {
			out->setUintProperty(i, d->getUintProperty(i+start));
		}

		uint32 insertCount = (args->getLength() > 2) ? (args->getLength() - 2) : 0;
		long l_shiftAmount = (long)insertCount - (long) deleteCount; // long because result could be negative
		uint32 shiftAmount;

        ArrayObject *a = isArray(thisAtom);
		if (a && a->isSimpleDense() && args->isSimpleDense())
		{
			a->m_denseArr.splice (start, insertCount, deleteCount, &args->m_denseArr, 2);
			a->m_length += l_shiftAmount;
			return out;
		}

		// delete items by shifting elements past end (of delete) by l_shiftAmount
		if (l_shiftAmount < 0) {
			// Shift the remaining elements down
			shiftAmount = (uint32)(-l_shiftAmount);

			for (i=end; i<len; i++) {
				d->setUintProperty(i-shiftAmount, d->getUintProperty(i));
			}
					
			// delete top elements here to match ECMAscript spec (generic object support)
			for (i=len-shiftAmount; i<len; i++) {
				d->delUintProperty (i);
			}
		} else {
			// Shift the remaining elements up. 
			shiftAmount = (uint32)l_shiftAmount;

			for (i=len; i > end; ) { // Note: i is unsigned, can't check if --i >=0.
				--i;
				d->setUintProperty(i+shiftAmount, d->getUintProperty(i));
			}
		}

		// Add the items to insert
		for (i=0; i<insertCount; i++) {
			d->setUintProperty(start+i, args->getUintProperty(i + 2));
		}

		// shrink array if shiftAmount is negative
		setLengthHelper (d, len+l_shiftAmount);
			
		return out;
	}

	ArrayObject* ArrayClass::newarray(Atom* argv, int argc)
	{
		ArrayObject *inst = newArray(argc);

  		for (uint32 i=0; i<uint32(argc); i++) {
  			inst->setUintProperty(i, argv[i]);
  		}
  		
  		return inst;
	}

	ArrayObject* ArrayClass::newArray(uint32 capacity)
	{
		VTable* ivtable = this->ivtable();
		ArrayObject *a = new (core()->GetGC(), ivtable->getExtraSize()) 
			ArrayObject(ivtable, prototype, capacity);
#ifdef DEBUGGER
		if( core()->allocationTracking )
		{
			toplevel()->arrayClass->addInstance(a->atom());
		}
#endif
		return a;
	}


	ArrayObject* ArrayClass::createInstance(VTable *ivtable, ScriptObject* prototype)
	{
		return new (core()->GetGC(), ivtable->getExtraSize()) ArrayObject(ivtable, prototype, 0);
	}

	int ArrayClass::indexOf (Atom thisAtom, Atom searchElement, int startIndex)
	{
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom))
			return -1;

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		uint32 start = NativeObjectHelpers::ClampIndexInt(startIndex, len); 

		for (uint32 i = start; i < len; i++)
		{
			Atom atom = d->getUintProperty(i);
			if (core->stricteq (atom, searchElement) == trueAtom)
				return i;
		}

		return -1;
	}

	int ArrayClass::lastIndexOf (Atom thisAtom, Atom searchElement, int startIndex)
	{
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom))
			return -1;

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		int start = NativeObjectHelpers::ClampIndexInt(startIndex, len); 
		if (start == int(len))
			start--;

		for (int i = start; i >= 0; i--)
		{
			Atom atom = d->getUintProperty(i);
			if (core->stricteq (atom, searchElement) == trueAtom)
				return i;
		}

		return -1;
	}

	bool ArrayClass::every (Atom thisAtom, ScriptObject *callback, Atom thisObject)
	{
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom) || !callback)
			return true;

		if (callback->isMethodClosure() && !AvmCore::isNull(thisObject)) 
		{
			toplevel()->throwTypeError(kArrayFilterNonNullObjectError);
		}

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		// If thisObject is null, the call function will substitute the global object 
		Atom args[4] = { thisObject, nullObjectAtom, nullObjectAtom, thisAtom };

		for (uint32 i = 0; i < len; i++)
		{
			args[1] = d->getUintProperty (i); // element
			args[2] = core->uintToAtom (i); // index

			Atom result = callback->call(3, args);
			if (result != trueAtom)
				return false;
		}

		return true;
	}

	ArrayObject *ArrayClass::filter (Atom thisAtom, ScriptObject *callback, Atom thisObject)
	{
		AvmCore* core = this->core();
		ArrayObject *r = newArray();

		if (!AvmCore::isObject(thisAtom) || !callback)
			return r;

		if (callback->isMethodClosure() && !AvmCore::isNull(thisObject)) 
		{
			toplevel()->throwTypeError(kArrayFilterNonNullObjectError);
		}

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		// If thisObject is null, the call function will substitute the global object 
		Atom args[4] = { thisObject, nullObjectAtom, nullObjectAtom, thisAtom };

		for (uint32 i = 0; i < len; i++)
		{
			args[1] = d->getUintProperty (i); // element
			args[2] = core->uintToAtom (i); // index

			Atom result = callback->call(3, args);
			if (result == trueAtom)
			{
				r->push (args + 1, 1);
			}
		}

		return r;
	}

	void ArrayClass::forEach (Atom thisAtom, ScriptObject *callback, Atom thisObject)
	{
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom) || !callback)
			return;

		if (callback->isMethodClosure() && !AvmCore::isNull(thisObject)) 
		{
			toplevel()->throwTypeError(kArrayFilterNonNullObjectError);
		}

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		// If thisObject is null, the call function will substitute the global object 
		Atom args[4] = { thisObject, nullObjectAtom, nullObjectAtom, thisAtom };

		for (uint32 i = 0; i < len; i++)
		{
			args[1] = d->getUintProperty (i); // element
			args[2] = core->uintToAtom (i); // index

			callback->call(3, args);
		}
	}

	bool ArrayClass::some (Atom thisAtom, ScriptObject *callback, Atom thisObject)
	{
		AvmCore* core = this->core();
		if (!AvmCore::isObject(thisAtom) || !callback)
			return false;

		if (callback->isMethodClosure() && !AvmCore::isNull(thisObject)) 
		{
			toplevel()->throwTypeError(kArrayFilterNonNullObjectError);
		}

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		// If thisObject is null, the call function will substitute the global object 
		Atom args[4] = { thisObject, nullObjectAtom, nullObjectAtom, thisAtom };

		for (uint32 i = 0; i < len; i++)
		{
			args[1] = d->getUintProperty (i); // element
			args[2] = core->uintToAtom (i); // index

			Atom result = callback->call(3, args);
			if (result == trueAtom)
				return true;
		}

		return false;
	}

	ArrayObject *ArrayClass::map (Atom thisAtom, ScriptObject *callback, Atom thisObject)
	{
		AvmCore* core = this->core();
		ArrayObject *r = newArray();

		if (!AvmCore::isObject(thisAtom) || !callback)
			return r;

		ScriptObject *d = AvmCore::atomToScriptObject(thisAtom);
		uint32 len = getLengthHelper (d);

		// If thisObject is null, the call function will substitute the global object 
		Atom args[4] = { thisObject, nullObjectAtom, nullObjectAtom, thisAtom };

		for (uint32 i = 0; i < len; i++)
		{
			args[1] = d->getUintProperty (i); // element
			args[2] = core->uintToAtom (i); // index

			Atom result = callback->call(3, args);

			r->push (&result, 1);
		}

		return r;
	}

	uint32 ArrayClass::getLengthHelper (ScriptObject *d)
	{
		AvmCore* core = this->core();
		Multiname mname(core->publicNamespace, core->klength);
		Atom lenAtm = toplevel()->getproperty (d->atom(), &mname, d->vtable);
		return core->toUInt32 (lenAtm);
	}

	void ArrayClass::setLengthHelper (ScriptObject *d, uint32 newLen)
	{
		AvmCore* core = this->core();
		Multiname mname(core->publicNamespace, core->klength);
		Atom lenAtm = core->uintToAtom(newLen);
		toplevel()->setproperty (d->atom(), &mname, lenAtm, d->vtable);
	}
}
