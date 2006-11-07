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

package flash.utils
{
// 
// Dictionary
//

/**
 * The Dictionary class lets you create a dynamic collection of properties, which uses strict equality
 * (<code>===</code>) for key comparison. When an object is used as a key, the object's
 * identity is used to look up the object, and not the value returned from calling <code>toString()</code> on it.
 * <p>The following statements show the relationship between a Dictionary object and a key object:</p>
 * <pre>
 * var dict = new Dictionary();
 * var obj = new Object();
 * var key:Object = new Object();
 * key.toString = function() { return "key" }
 *  
 * dict[key] = "Letters";
 * obj["key"] = "Letters";
 *  
 * dict[key] == "Letters"; // true
 * obj["key"] == "Letters"; // true 
 * obj[key] == "Letters"; // true because key == "key" is true b/c key.toString == "key"
 * dict["key"] == "Letters"; // false because "key" === key is false
 * delete dict[key]; //removes the key
 * </pre>
 *
 * @playerversion Flash 8.5
 * @langversion 3.0
 * @see ../../operators.html#strict_equality === (strict equality)
 * 
 */
dynamic public class Dictionary
{
	/**
	 * Creates a new Dictionary object. To remove a key from a Dictionary object, use the <code>delete</code> operator.
  	 *
  	 * @param weakKeys Instructs the Dictionary object to use "weak" references on object keys.
  	 * If the only reference to an object is in the specified Dictionary object, the key is eligible for 
  	 * garbage collection and is removed from the table when the object is collected.
  	 *
  	 * @playerversion Flash 8.5
 	 * @langversion 3.0
 	 */
	public native function Dictionary(weakKeys:Boolean=false);
};
}
