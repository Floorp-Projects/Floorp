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
	public final dynamic class Date
	{
		// Date.length = 7 per ES3
		// E262 {ReadOnly, DontDelete, DontEnum }
		public static const length:int = 7

		public native static function parse(s):Number	
		public native static function UTC(year, month, date=1, hours=0, minutes=0, seconds=0, ms=0, ... rest):Number

		AS3 native function valueOf():Number
		private native function _toString(index:int):String
		private native function _setTime(value:Number):Number
		private native function _get(index:int):Number

		AS3 function setTime(t=void 0):Number
		{
			return _setTime(t)
		}
		
		prototype.setTime = function(t=void 0):Number
		{
			var d:Date = this
			return d._setTime(t)
		}

		prototype.valueOf = function()
		{
			var d:Date = this
			return d.AS3::valueOf()
		}
		
		AS3 function toString():String				{ return _toString(0) }
		AS3 function toDateString():String			{ return _toString(1) }
		AS3 function toTimeString():String			{ return _toString(2) }
		AS3 function toLocaleString():String		{ return _toString(3) }
		AS3 function toLocaleDateString():String	{ return _toString(4) }
		AS3 function toLocaleTimeString():String	{ return _toString(5) }
		AS3 function toUTCString():String			{ return _toString(6) }

		AS3 native function getUTCFullYear():Number		
		AS3 native function getUTCMonth():Number			
		AS3 native function getUTCDate():Number			
		AS3 native function getUTCDay():Number		
		AS3 native function getUTCHours():Number	
		AS3 native function getUTCMinutes():Number	
		AS3 native function getUTCSeconds():Number	
		AS3 native function getUTCMilliseconds():Number	
		AS3 native function getFullYear():Number	
		AS3 native function getMonth():Number		
		AS3 native function getDate():Number		
		AS3 native function getDay():Number		
		AS3 native function getHours():Number		
		AS3 native function getMinutes():Number	
		AS3 native function getSeconds():Number	
		AS3 native function getMilliseconds():Number	
		AS3 native function getTimezoneOffset():Number	
		AS3 native function getTime():Number			

		AS3 native function setFullYear(year=void 0, month=void 0, date=void 0):Number
		AS3 native function setMonth(month=void 0, date=void 0):Number
		AS3 native function setDate(date=void 0):Number
		AS3 native function setHours(hour=void 0, min=void 0, sec=void 0, ms=void 0):Number
		AS3 native function setMinutes(min=void 0, sec=void 0, ms=void 0):Number
		AS3 native function setSeconds(sec=void 0, ms=void 0):Number
		AS3 native function setMilliseconds(ms=void 0):Number

		AS3 native function setUTCFullYear(year=void 0, month=void 0, date=void 0):Number
		AS3 native function setUTCMonth(month=void 0, date=void 0):Number
		AS3 native function setUTCDate(date=void 0):Number
		AS3 native function setUTCHours(hour=void 0, min=void 0, sec=void 0, ms=void 0):Number
		AS3 native function setUTCMinutes(min=void 0, sec=void 0, ms=void 0):Number
		AS3 native function setUTCSeconds(sec=void 0, ms=void 0):Number
		AS3 native function setUTCMilliseconds(ms=void 0):Number

		prototype.toString = function():String
		{
			var d:Date = this
			return d._toString(0)
		}
		prototype.toDateString = function():String
		{
			var d:Date = this
			return d.AS3::toDateString()
		}
		prototype.toTimeString = function():String
		{
			var d:Date = this
			return d.AS3::toTimeString()
		}
		prototype.toLocaleString = function():String
		{
			var d:Date = this
			return d.AS3::toLocaleString()
		}
		prototype.toLocaleDateString = function():String
		{
			var d:Date = this
			return d.AS3::toLocaleDateString()
		}
		prototype.toLocaleTimeString = function():String
		{
			var d:Date = this
			return d.AS3::toLocaleTimeString()
		}
		prototype.toUTCString = function():String
		{
			var d:Date = this
			return d.AS3::toUTCString()
		}

		prototype.getUTCFullYear = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCFullYear()
		}
		
		prototype.getUTCMonth = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCMonth()
		}
		
		prototype.getUTCDate = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCDate()
		}
		
		prototype.getUTCDay = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCDay()
		}
		
		prototype.getUTCHours = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCHours()
		}
		
		prototype.getUTCMinutes = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCMinutes()
		}
		
		prototype.getUTCSeconds = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCSeconds()
		}
		
		prototype.getUTCMilliseconds = function():Number
		{
			var d:Date = this
			return d.AS3::getUTCMilliseconds()
		}
		
		prototype.getFullYear = function():Number
		{
			var d:Date = this
			return d.AS3::getFullYear()
		}
		
		prototype.getMonth = function():Number
		{
			var d:Date = this
			return d.AS3::getMonth()
		}
		
		prototype.getDate = function():Number
		{
			var d:Date = this
			return d.AS3::getDate()
		}
		
		prototype.getDay = function():Number
		{
			var d:Date = this
			return d.AS3::getDay()
		}
		
		prototype.getHours = function():Number
		{
			var d:Date = this
			return d.AS3::getHours()
		}
		
		prototype.getMinutes = function():Number
		{
			var d:Date = this
			return d.AS3::getMinutes()
		}
		
		prototype.getSeconds = function():Number
		{
			var d:Date = this
			return d.AS3::getSeconds()
		}
		
		prototype.getMilliseconds = function():Number
		{
			var d:Date = this
			return d.AS3::getMilliseconds()
		}
		
		prototype.getTimezoneOffset = function():Number
		{
			var d:Date = this
			return d.AS3::getTimezoneOffset()
		}
		
		prototype.getTime = function():Number
		{
			var d:Date = this
			return d.AS3::getTime()
		}

		prototype.setFullYear = function(year=void 0, month=void 0, date=void 0):Number
		{
			var d:Date = this
			return d.AS3::setFullYear.apply(d, arguments);
		}
		prototype.setMonth = function(month=void 0, date=void 0):Number
		{
			var d:Date = this
			return d.AS3::setMonth.apply(d, arguments);
		}
		prototype.setDate = function(date=void 0):Number
		{
			var d:Date = this
			return d.AS3::setDate.apply(d, arguments);
		}
		prototype.setHours = function(hour=void 0, min=void 0, sec=void 0, ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setHours.apply(d, arguments);
		}
		prototype.setMinutes = function(min=void 0, sec=void 0, ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setMinutes.apply(d, arguments);
		}
		prototype.setSeconds = function(sec=void 0, ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setSeconds.apply(d, arguments);
		}
		prototype.setMilliseconds = function(ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setMilliseconds.apply(d, arguments);
		}

		prototype.setUTCFullYear = function(year=void 0, month=void 0, date=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCFullYear.apply(d, arguments);
		}
		prototype.setUTCMonth = function(month=void 0, date=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCMonth.apply(d, arguments);
		}
		prototype.setUTCDate = function(date=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCDate.apply(d, arguments);
		}
		prototype.setUTCHours = function(hour=void 0, min=void 0, sec=void 0, ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCHours.apply(d, arguments);
		}
		prototype.setUTCMinutes = function(min=void 0, sec=void 0, ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCMinutes.apply(d, arguments);
		}
		prototype.setUTCSeconds = function(sec=void 0, ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCSeconds.apply(d, arguments);
		}
		prototype.setUTCMilliseconds = function(ms=void 0):Number
		{
			var d:Date = this
			return d.AS3::setUTCMilliseconds.apply(d, arguments);
		}

		public function get fullYear():Number { return AS3::getFullYear(); }
		public function set fullYear(value:Number) { AS3::setFullYear(value); }

		public function get month():Number { return AS3::getMonth(); }
		public function set month(value:Number) { AS3::setMonth(value); }

		public function get date():Number { return AS3::getDate(); }
		public function set date(value:Number) { AS3::setDate(value); }

		public function get hours():Number { return AS3::getHours(); }
		public function set hours(value:Number) { AS3::setHours(value); }

		public function get minutes():Number { return AS3::getMinutes(); }
		public function set minutes(value:Number) { AS3::setMinutes(value); }

		public function get seconds():Number { return AS3::getSeconds(); }
		public function set seconds(value:Number) { AS3::setSeconds(value); }

		public function get milliseconds():Number { return AS3::getMilliseconds(); }
		public function set milliseconds(value:Number) { AS3::setMilliseconds(value); }

		public function get fullYearUTC():Number { return AS3::getUTCFullYear(); }
		public function set fullYearUTC(value:Number) { AS3::setUTCFullYear(value); }

		public function get monthUTC():Number { return AS3::getUTCMonth(); }
		public function set monthUTC(value:Number) { AS3::setUTCMonth(value); }

		public function get dateUTC():Number { return AS3::getUTCDate(); }
		public function set dateUTC(value:Number) { AS3::setUTCDate(value); }

		public function get hoursUTC():Number { return AS3::getUTCHours(); }
		public function set hoursUTC(value:Number) { AS3::setUTCHours(value); }

		public function get minutesUTC():Number { return AS3::getUTCMinutes(); }
		public function set minutesUTC(value:Number) { AS3::setUTCMinutes(value); }

		public function get secondsUTC():Number { return AS3::getUTCSeconds(); }
		public function set secondsUTC(value:Number) { AS3::setUTCSeconds(value); }

		public function get millisecondsUTC():Number { return AS3::getUTCMilliseconds(); }
		public function set millisecondsUTC(value:Number) { AS3::setUTCMilliseconds(value); }

		public function get time():Number { return AS3::getTime(); }
		public function set time(value:Number) { AS3::setTime(value); }

		public function get timezoneOffset():Number { return AS3::getTimezoneOffset(); }
		public function get day():Number { return AS3::getDay(); }
		public function get dayUTC():Number { return AS3::getUTCDay(); }


        // Dummy constructor function - This is neccessary so the compiler can do arg # checking for the ctor in strict mode
        // The code for the actual ctor is in DateClass::construct in the avmplus
        public function Date(year = void 0, month = void 0, date = void 0, hours = void 0, minutes = void 0, seconds = void 0, ms = void 0)
        {}
        

		// These are not part of ECMA-262, and thus we will not be exposing
		// them via the new-style get/set functions (this is provided here
		// just to let you know we didn't overlook them)
		//public function get year():Number { return getYear(); }
		//public function get yearUTC():Number { return getUTCYear(); }	

		// The following older ECMA and/or AS2 functions are not supported since
		// they are not Y2K compliant (only get/set 2 digits)
		// getYear
		// setYear
		// getUTCYear
		// setUTCYear

		_dontEnumPrototype(prototype);
	}
}
