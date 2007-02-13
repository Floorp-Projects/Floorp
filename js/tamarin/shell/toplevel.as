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

package avmplus 
{
	public class System
	{
		public native static function exit(status:int):void
		public native static function exec(command:String):int
		public native static function getAvmplusVersion():String
		public native static function trace(a:Array):void
		public native static function write(s:String):void
		public native static function debugger():void
		public native static function isDebugger():Boolean
		public native static function getTimer():uint
		private native static function getArgv():Array
		public static const argv:Array = getArgv();
		public native static function readLine():String
	}	
	
	public class File
	{
		public native static function exists(filename:String):String;
		public native static function read(filename:String):String;
		public native static function write(filename:String, data:String):void;
	}

	public function debugger()
	{
		System.debugger()
	}
}

// The flash.system package is present so identical ATS test media can be used
// in the command-line VM and the Player
package flash.system
{
	import avmplus.*;
	
	public final class Capabilities
	{
		public static function get playerType():String { return "AVMPlus"; }
		public static function get isDebugger():Boolean { return System.isDebugger(); }
	}
}
		
package {

	import avmplus.*

	public function getClassByName(name:String):Class
	{
		return Domain.currentDomain.getClass(name);
	}
		
	// nonstandard extensions to ECMAScript
	public function print(...s)
	{
		System.trace(s)
	}

	// nonstandard Flash Player extensions
	public function trace(...s)
	{
		System.trace(s)
	}

	public function getTimer():uint
	{
		return System.getTimer()
	}

	public function readLine():String
	{
		return System.readLine()
	}
}
