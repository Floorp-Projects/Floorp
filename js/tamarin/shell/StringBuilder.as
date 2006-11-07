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

package avmplus
{

public class StringBuilder
{
	public function StringBuilder(str:String = null)
	{
		if (str != null) 
		{
			append(str);
		}
	}

	public native function append(value):void;
	public native function get capacity():uint;
	public native function charAt(index:uint):String;
	public native function charCodeAt(index:uint):uint;
	public native function ensureCapacity(minimumCapacity:uint):void;
	public native function indexOf(str:String, index:uint=0):int;
	public native function insert(index:uint, value):void;
	public native function lastIndexOf(str:String, index:uint=0xFFFFFFFF):int;
	public native function get length():uint;
	public native function set length(value:uint)
	public native function remove(beginIndex:uint, endIndex:uint):void;
	public native function removeCharAt(index:uint):void;
	public native function replace(beginIndex:uint, endIndex:uint, replacement:String):void;
	public native function reverse():void;
	public native function setCharAt(index:uint, ch:String):void;
	public native function substring(beginIndex:uint, endIndex:uint=0xFFFFFFFF):String;
	public native function toString():String;
	public native function trimToSize():void;
}

}
