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


package
{
	public final class String extends Object
	{
		// String.length = 1 per ES3
		// E262 {ReadOnly, DontDelete, DontEnum }
		public static const length:int = 1
		
		AS3 native static function fromCharCode(... arguments):String
		String.fromCharCode = function(... arguments) {
			return AS3::fromCharCode.apply(String,arguments)
		}

		// E262 {DontEnum, DontDelete, ReadOnly}
		public native function get length():int
		
		// indexOf and other _ functions get early bound by MIR
		private native function _indexOf(s:String, i:int=0):int
		AS3 native function indexOf(s:String="undefined", i:Number=0):int
		prototype.indexOf = function(s:String="undefined", i:Number=0):int 
		{
			return String(this).AS3::indexOf(s, i)
		}

		// lastIndexOf
		private native function _lastIndexOf(s:String, i:int=0x7fffffff):int
		AS3 native function lastIndexOf(s:String="undefined", i:Number=0x7FFFFFFF):int 
		
		prototype.lastIndexOf = function(s:String="undefined", i:Number=0x7fffffff):int 
		{
			return String(this).AS3::lastIndexOf(s, i)
		}

		// charAt
		private native function _charAt(i:int=0):String
		AS3 native function charAt(i:Number=0):String

		prototype.charAt = function(i:Number=0):String
		{
			return String(this).AS3::charAt(i)
		}

		// charCodeAt
		private native function _charCodeAt(i:int=0):Number
		AS3 native function charCodeAt(i:Number=0):Number

		prototype.charCodeAt = function(i:Number=0):Number
		{
			return String(this).AS3::charCodeAt(i)
		}

		// concat
		AS3 function concat(...args):String
		{
			var s:String = this
			for (var i:uint = 0, n:uint = args.length; i < n; i++)
				s = s + String(args[i])
			return s
		}

		prototype.concat = function(... args):String
		{
			// todo: use function.apply or array.join?
			var s:String = String(this)
			for (var i:uint = 0, n:uint = args.length; i < n; i++)
				s = s + String(args[i])
			return s
		}

		AS3 native function localeCompare(other:String=void 0):int
		prototype.localeCompare = function(other:String=void 0):int 
		{
			return String(this).AS3::localeCompare(other)
		}

		// match
		// P can be a RegEx or is coerced to a string (and then RegEx constructor is called)
		private static native function _match(s:String, p):Array
		AS3 function match(p=void 0):Array
		{
			return _match(this, p)
		}
		prototype.match = function(p=void 0):Array
		{
			return _match(String(this), p)
		}

		// replace
		// p is a RegEx or string
		// repl is a function or coerced to a string
		private static native function _replace(s:String, p, repl):String
		AS3 function replace(p=void 0, repl=void 0):String
		{
			return _replace(this, p, repl)
		}
		prototype.replace = function(p=void 0, repl=void 0):String
		{
			return _replace(String(this), p, repl)
		}

		// search
		// P can be a RegEx or is coerced to a string (and then RegEx constructor is called)
		private static native function _search(s:String, p):int
		AS3 function search(p=void 0):int
		{
			return _search(this, p)
		}

		prototype.search = function(p=void 0):int
		{
			return _search(String(this), p)
		}

		// slice
		private native function _slice(start:int=0, end:int=0x7fffffff):String
		AS3 native function slice(start:Number=0, end:Number=0x7fffffff):String
		prototype.slice = function(start:Number=0, end:Number=0x7fffffff):String
		{
			return String(this).AS3::slice(start, end)
		}

		// This is a static helper since it depends on AvmCore which String objects
		// don't have access to.
		// delim can be a RegEx or is coerced to a string (and then RegEx constructor is called)
		private static native function _split(s:String, delim, limit:uint):Array
		AS3 function split(delim=void 0, limit=0xffffffff):Array
		{
			// ECMA compatibility - limit can be undefined
			if (limit == undefined)
				limit = 0xffffffff;
			return _split(this, delim, limit)
		}
		prototype.split = function(delim=void 0, limit=0xffffffff):Array
		{
			// ECMA compatibility - limit can be undefined
			if (limit == undefined)
				limit = 0xffffffff;
			return _split(String(this), delim, limit)
		}

		// substring
		private native function _substring(start:int=0, end:int=0x7fffffff):String
		AS3 native function substring(start:Number=0, end:Number=0x7fffffff):String
		prototype.substring = function(start:Number=0, end:Number=0x7fffffff):String
		{
			return String(this).AS3::substring(start, end)
		}

		// substr
		private native function _substr(start:int=0, end:int=0x7fffffff):String
		AS3 native function substr(start:Number=0, len:Number=0x7fffffff):String
		prototype.substr = function(start:Number=0, len:Number=0x7fffffff):String
		{
			return String(this).AS3::substr(start, len)
		}

		AS3 native function toLowerCase():String
		AS3 function toLocaleLowerCase():String
		{
			return this.AS3::toLowerCase();
		}

		prototype.toLowerCase = prototype.toLocaleLowerCase = 
		function():String
		{
			return String(this).AS3::toLowerCase()
		}

		AS3 native function toUpperCase():String
		AS3 function toLocaleUpperCase():String
		{
			return this.AS3::toUpperCase();
		}

		prototype.toUpperCase = prototype.toLocaleUpperCase = 
		function():String
		{
			return String(this).AS3::toUpperCase()
		}

		AS3 function toString():String { return this }
		AS3 function valueOf():String { return this }

		prototype.toString = function():String
		{
			if (this === prototype)
				return ""

			if (!(this is String))
				Error.throwError( TypeError, 1004 /*kInvokeOnIncompatibleObjectError*/, "String.prototype.toString" );

			return this
		}
			
		prototype.valueOf = function()
		{
			if (this === prototype)
				return ""

			if (!(this is String))
				Error.throwError( TypeError, 1004 /*kInvokeOnIncompatibleObjectError*/, "String.prototype.valueOf" );

			return this
		}
        
        // Dummy constructor function - This is neccessary so the compiler can do arg # checking for the ctor in strict mode
        // The code for the actual ctor is in StringClass::construct in the avmplus
        public function String(value = "")
        {}

        _dontEnumPrototype(prototype);
	}
}
