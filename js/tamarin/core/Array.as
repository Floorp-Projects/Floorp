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


package
{
	public dynamic class Array extends Object
	{
		// option flags for sort and sortOn
		public static const CASEINSENSITIVE:uint = 1;
		public static const DESCENDING:uint = 2;
		public static const UNIQUESORT:uint = 4;
		public static const RETURNINDEXEDARRAY:uint = 8;
		public static const NUMERIC:uint = 16;	

		// E262 {DontEnum, DontDelete}
		public native function get length():uint
		public native function set length(newLength:uint)
		
		// Array.length = 1 per ES3
		public static const length:int = 1
		
		// ECMA 15.4.2.2
		public function Array(...args)
		{
			var n:uint = args.length
			if (n == 1 && (args[0] is Number))
			{
				var dlen:Number = args[0];
				var ulen:uint = dlen
				if (ulen != dlen)
					Error.throwError( RangeError, 1005 /*kArrayIndexNotIntegerError*/, dlen );
				length = ulen;
			}
			else
			{
			// FIXME why not just return args?
				length = n
				for (var i:uint=0; i < n; i++)
					this[i] = args[i]
			}
		}
		
		/**
		15.4.4.5 Array.prototype.join (separator)
		The elements of the array are converted to strings, and these strings are then concatenated, separated by
		occurrences of the separator. If no separator is provided, a single comma is used as the separator.
		The join method takes one argument, separator, and performs the following steps:
		1. Call the [[Get]] method of this object with argument "length".
		2. Call ToUint32(Result(1)).
		3. If separator is undefined, let separator be the single-character string ",".
		4. Call ToString(separator).
		5. If Result(2) is zero, return the empty string.
		6. Call the [[Get]] method of this object with argument "0".
		7. If Result(6) is undefined or null, use the empty string; otherwise, call ToString(Result(6)).
		8. Let R be Result(7).
		9. Let k be 1.
		10. If k equals Result(2), return R.
		11. Let S be a string value produced by concatenating R and Result(4).
		12. Call the [[Get]] method of this object with argument ToString(k).
		13. If Result(12) is undefined or null, use the empty string; otherwise, call ToString(Result(12)).
		14. Let R be a string value produced by concatenating S and Result(13).
		15. Increase k by 1.
		16. Go to step 10.
		*/

		private static function _join(o, sep):String
		{
			var s:String = (sep === undefined) ? "," : String(sep)
			var out:String = ""
			for (var i:uint = 0, n:uint=uint(o.length); i < n; i++)
			{
				var x = o[i]
				if (x != null)
					out += x
				if (i+1 < n)
					out += s
			}
			return out
		}
		AS3 function join(sep=void 0):String
		{
			return _join(this, sep)
		}
		prototype.join = function(sep=void 0):String
		{
			return _join(this, sep)
		}

		private static native function _pop(o)
		AS3 native function pop()
		prototype.pop = function()
		{
			return _pop(this)
		}

		/**
		15.4.4.2 Array.prototype.toString ( )
		The result of calling this function is the same as if the built-in join method were invoked for this object with no
		argument.
		The toString function is not generic; it throws a TypeError exception if its this value is not an Array object.
		Therefore, it cannot be transferred to other kinds of objects for use as a method.
		*/
		
		prototype.toString = function():String
		{
			var a:Array = this  // TypeError if not compatible
			return _join(a, ",");
		}

		/**
		15.4.4.3 Array.prototype.toLocaleString ( )
		The elements of the array are converted to strings using their toLocaleString methods, and these strings are
		then concatenated, separated by occurrences of a separator string that has been derived in an implementationdefined
		locale-specific way. The result of calling this function is intended to be analogous to the result of
		toString, except that the result of this function is intended to be locale-specific.
		The result is calculated as follows:
		1. Call the [[Get]] method of this object with argument "length".
		2. Call ToUint32(Result(1)).
		3. Let separator be the list-separator string appropriate for the host environment’s current locale (this is derived in
		an implementation-defined way).
		4. Call ToString(separator).
		5. If Result(2) is zero, return the empty string.
		6. Call the [[Get]] method of this object with argument "0".
		7. If Result(6) is undefined or null, use the empty string; otherwise, call ToObject(Result(6)).toLocaleString().
		8. Let R be Result(7).
		9. Let k be 1.
		10. If k equals Result(2), return R.
		11. Let S be a string value produced by concatenating R and Result(4).
		12. Call the [[Get]] method of this object with argument ToString(k).
		13. If Result(12) is undefined or null, use the empty string; otherwise, call ToObject(Result(12)).toLocaleString().
		14. Let R be a string value produced by concatenating S and Result(13).
		15. Increase k by 1.
		16. Go to step 10.
		The toLocaleString function is not generic; it throws a TypeError exception if its this value is not an Array
		object. Therefore, it cannot be transferred to other kinds of objects for use as a method.
		*/

		prototype.toLocaleString = function():String
		{
			var a:Array = this // TypeError if not compatible

			var out:String = ""
			for (var i:uint = 0, n:uint=a.length; i < n; i++)
			{
				var x = a[i]
				if (x != null)
					out += x.toLocaleString()
				if (i+1 < n)
					out += ","
			}
			return out
		}

		/**
		When the push method is called with zero or more arguments item1, item2, etc., the following steps are taken:
		1. Call the [[Get]] method of this object with argument "length".
		2. Let n be the result of calling ToUint32(Result(1)).
		3. Get the next argument in the argument list; if there are no more arguments, go to step 7.
		4. Call the [[Put]] method of this object with arguments ToString(n) and Result(3).
		5. Increase n by 1.
		6. Go to step 3.
		7. Call the [[Put]] method of this object with arguments "length" and n.
		8. Return n.
		The length property of the push method is 1.
		NOTE The push function is intentionally generic; it does not require that its this value be an Array object. Therefore it can be
		transferred to other kinds of objects for use as a method. Whether the push function can be applied successfully to a host object
		is implementation-dependent.
		*/
		AS3 native function push(...args):uint
		prototype.push = function(...args):uint
		{
			var n:uint = uint(this.length)
			for (var i:uint=0, argc:uint=args.length; i < argc; i++)
				this[n++] = args[i]
			this.length = n
			return n
		}

		private static native function _reverse(o)
		AS3 function reverse():Array
		{
			return _reverse(this)  // return will cast to Array
		}
		prototype.reverse = function()
		{
			return _reverse(this)
		}

		private static native function _concat(o, args:Array):Array
		AS3 function concat(...args):Array
		{
			return _concat(this, args)
		}
		prototype.concat = function(...args):Array
		{
			return _concat(this, args)
		}

		private static native function _shift(o)
		AS3 function shift()
		{
			return _shift(this)
		}
		prototype.shift = function()
		{
			return _shift(this)
		}

		private static native function _slice(o, A:Number, B:Number):Array
		AS3 function slice(A=0, B=0xffffffff):Array
		{
			return _slice(this, Number(A), Number(B))
		}
		prototype.slice = function(A=0, B=0xffffffff):Array
		{
			return _slice(this, Number(A), Number(B))
		}

		/**
		15.4.4.13 Array.prototype.unshift ( [ item1 [ , item2 [ , … ] ] ] )
		The arguments are prepended to the start of the array, such that their order within the array is the same as the
		order in which they appear in the argument list.
		When the unshift method is called with zero or more arguments item1, item2, etc., the following steps are taken:
		1. Call the [[Get]] method of this object with argument "length".
		2. Call ToUint32(Result(1)).
		3. Compute the number of arguments.
		4. Let k be Result(2).
		5. If k is zero, go to step 15.
		6. Call ToString(k–1).
		7. Call ToString(k+Result(3)–1).
		8. If this object has a property named by Result(6), go to step 9; but if this object has no property named by
		Result(6), then go to step 12.
		9. Call the [[Get]] method of this object with argument Result(6).
		10. Call the [[Put]] method of this object with arguments Result(7) and Result(9).
		11. Go to step 13.
		12. Call the [[Delete]] method of this object with argument Result(7).
		13. Decrease k by 1.
		14. Go to step 5.
		15. Let k be 0.
		16. Get the next argument in the part of the argument list that starts with item1; if there are no more arguments, go
		to step 21.
		17. Call ToString(k).
		18. Call the [[Put]] method of this object with arguments Result(17) and Result(16).
		19. Increase k by 1.
		20. Go to step 16.
		21. Call the [[Put]] method of this object with arguments "length" and (Result(2)+Result(3)).
		22. Return (Result(2)+Result(3)).
		The length property of the unshift method is 1.
		NOTE The unshift function is intentionally generic; it does not require that its this value be an Array object. Therefore it can
		be transferred to other kinds of objects for use as a method. Whether the unshift function can be applied successfully to a
		host object is implementation-dependent.
		*/
		native AS3 function unshift(...args):uint
		prototype.unshift = function(...args):uint
		{
			var len:uint = uint(this.length)
			var argc:uint = args.length
			var k:uint
			for (k=len; k > 0; )
			{
				k--
				var d:uint = k+argc
				if (k in this)
					this[d] = this[k]
				else
					delete this[d]
			}

			for (var i:uint = 0; i < argc; i++)
				this[k++] = args[i]

			len += argc
			this.length = len
			return len
		}

		private static native function _splice(o, args:Array):Array

		// splice with zero args returns undefined. All other cases return Array.
		AS3 function splice(...args)
		{
			if (!args.length)
				return undefined;

			return _splice(this, args)
		}
		prototype.splice = function(...args)
		{
			if (!args.length)
				return undefined;

			return _splice(this, args)
		}

		// sort can return an Array or a Number (unique sort option)
		private static native function _sort(o, args:Array)
		AS3 function sort(...args)
		{
			return _sort (this, args);
		}
		prototype.sort = function(...args)
		{
			return _sort (this, args);
		}

		private static native function _sortOn(o, names, options)
		AS3 function sortOn(names, options=0, ...ignored)
		{
			// this is our own addition so we don't have to make names be optional
			return _sortOn(this, names, options);
		}
		prototype.sortOn = function(names, options=0, ...ignored)
		{
			return _sortOn(this, names, options)
		}

		// Array extensions that are in Mozilla...
		// http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array
		//
		// These all work on generic objects (array like objects) as well as arrays

		private static native function _indexOf (o, searchElement, fromIndex:int):int;
		AS3 function indexOf(searchElement, fromIndex=0):int
		{
			return _indexOf (this, searchElement, int(fromIndex));
		}
		prototype.indexOf = function(searchElement, fromIndex=0):int
		{
			return _indexOf (this, searchElement, int(fromIndex));
		}

		private static native function _lastIndexOf (o, searchElement, fromIndex:int=0):int;
		AS3 function lastIndexOf(searchElement, fromIndex=0x7fffffff):int
		{
			return _lastIndexOf (this, searchElement, int(fromIndex));
		}
		prototype.lastIndexOf = function(searchElement, fromIndex=0x7fffffff):int
		{
			return _lastIndexOf (this, searchElement, int(fromIndex));
		}

		// Returns true if every element in this array satisfies the provided testing function.
		private static native function _every(o, callback:Function, thisObject):Boolean;
		AS3 function every(callback:Function, thisObject=null):Boolean
		{
			return _every (this, callback, thisObject);
		}
		prototype.every = function(callback:Function, thisObject=null):Boolean
		{
			return _every (this, callback, thisObject);
		}

		// Creates a new array with all elements that pass the test implemented by the provided function.
		private static native function _filter(o, callback:Function, thisObject):Array;
		AS3 function filter(callback:Function, thisObject=null):Array
		{
			return _filter (this, callback, thisObject);
		}
		prototype.filter = function(callback:Function, thisObject=null):Array
		{
			return _filter (this, callback, thisObject);
		}

		// Calls a function for each element in the array.
		private static native function _forEach(o, callback:Function, thisObject):void;
		AS3 function forEach(callback:Function, thisObject=null):void
		{
			_forEach (this, callback, thisObject);
		}
		prototype.forEach = function(callback:Function, thisObject=null):void
		{
			_forEach (this, callback, thisObject);
		}

		// Creates a new array with the results of calling a provided function on every element in this array.
		private native static function _map(o, callback:Function, thisObject):Array;
		AS3 function map(callback:Function, thisObject=null):Array
		{
			return _map (this, callback, thisObject);
		}
		prototype.map = function(callback:Function, thisObject=null):Array
		{
			return _map (this, callback, thisObject);
		}
		
		// Returns true if at least one element in this array satisfies the provided testing function.
		private static native function _some(o, callback:Function, thisObject):Boolean;
		AS3 function some(callback:Function, thisObject=null):Boolean
		{
			return _some (this, callback, thisObject);
		}
		prototype.some = function(callback:Function, thisObject=null):Boolean
		{
			return _some (this, callback, thisObject);
		}

		_dontEnumPrototype(prototype);
	}
}
