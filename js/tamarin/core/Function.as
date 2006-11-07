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
	//pseudo-final - no user class can extend Function
	dynamic public class Function
	{
		// Function.length = 1 per ES3
		// E262 {ReadOnly, DontDelete, DontEnum }
		public static const length:int = 1

		// E262 {DontDelete}
		// JS {DontEnum,DontDelete}
		public native function get prototype()
		public native function set prototype(p)
		
		// E262 {DontEnum, DontDelete, ReadOnly}
		public native function get length():int

		// called by native code to create empty functions used for 
		// prototype and no-arg constructor.
		private static function emptyCtor() 
		{
			return function() {}
		}
		
		/* cn:  Spidermonkey returns the actual source text of the function here.  The ES3
		//  standard only says:
			15.3.4.2 Function.prototype.toString ( )
			An implementation-dependent representation of the function is returned. This 
			representation has the syntax of a FunctionDeclaration. Note in particular 
			that the use and placement of white space, line terminators, and semicolons 
			within the representation string is implementation-dependent.
			The toString function is not generic; it throws a TypeError exception if its this value is not a Function object.
			Therefore, it cannot be transferred to other kinds of objects for use as a method.		
		//
		// We don't have the source text, so this impl follows the letter if not the intent
		//  of the spec.  
		//
		// Note: we only honor the compact ES3/4 spec, which means 
		//  we don't support new Function(stringArg) where stringArg is the text of
		//  the function to be compiled at runtime.  Returning the true text of the
		//  function in toString() seems to be a bookend to this feature to me, and
		//  thus shouldn't be in the compact specification either. */

		prototype.toLocaleString =
		prototype.toString = function():String
		{
			var f:Function = this
			return "function Function() {}"
		}

		AS3 native function call(thisArg=void 0, ...args)
		prototype.call = function(thisArg=void 0, ...args)
		{
			var f:Function = this
			return f.AS3::apply(thisArg, args)
		}

		AS3 native function apply(thisArg=void 0, argArray=void 0)
		prototype.apply = function(thisArg=void 0, argArray=void 0)
		{
			var f:Function = this
			return f.AS3::apply(thisArg, argArray)
		}

		_dontEnumPrototype(prototype);
	}
}

// not dynamic
final class MethodClosure extends Function
{
	override public function get prototype()
	{
		return null
	}

	override public function set prototype(p)
	{
		Error.throwError( ReferenceError, 1074 /*kConstWriteError*/, "prototype", "MethodClosure" );
	}

	public override native function get length():int
}
