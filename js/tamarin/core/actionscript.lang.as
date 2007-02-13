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

	public namespace AS3 = "http://adobe.com/AS3/2006/builtin"

	/**
	* @name Toplevel Function Properties
	* Function properties of the global object (ECMA 15.1.2)
	*/
	
	// {DontEnum} length=1
	public native function decodeURI(uri:String="undefined"):String

	// {DontEnum} length=1
	public native function decodeURIComponent(uri:String="undefined"):String

	// {DontEnum} length=1
	public native function encodeURI(uri:String="undefined"):String

	// {DontEnum} length=1
	public native function encodeURIComponent(uri:String="undefined"):String
	
	// {DontEnum} length=1
	public native function isNaN(n:Number = void 0):Boolean
	
	// {DontEnum} length=1
	public native function isFinite(n:Number = void 0):Boolean
	
	// {DontEnum} length=1
	public native function parseInt(s:String = "NaN", radix:int=0):Number
	
	// {DontEnum} length=1
	public native function parseFloat(str:String = "NaN"):Number
 

	/**
	* @name ECMA-262 Appendix B.2 extensions
	* Extensions to ECMAScript, in ECMA-262 Appendix B.2
	*/
	
	// {DontEnum} length=1
	public native function escape(s:String="undefined"):String

	// {DontEnum} length=1
	public native function unescape(s:String="undefined"):String

	// value properties of global object (ECMA 15.1.1)

	// in E262, these are var {DontEnum,DontDelete}  but not ReadOnly
	// but in E327, these are {ReadOnly, DontEnum, DontDelete}
	// we choose to make them const ala E327
	
	// The initial value of NaN is NaN (section 8.5). 
	// E262 { DontEnum, DontDelete}
	// E327 { DontEnum, DontDelete, ReadOnly}
	public const NaN:Number = 0/0
	
	// The initial value of Infinity is +8 (section 8.5). 
	// E262 { DontEnum, DontDelete}
	// E327 { DontEnum, DontDelete, ReadOnly}
	public const Infinity:Number = 1/0
	
	// The initial value of undefined is undefined (section 8.1).
	// E262 { DontEnum, DontDelete}
	// E327 { DontEnum, DontDelete, ReadOnly}
	public const undefined = void 0
}
