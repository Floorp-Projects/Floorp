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
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
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

 /*
 This file is imported with other .as files
 */

package packOne {
	public var var1="packOne:var1";
	public var var2="packOne:var2";
	public var var3="packOne:var3";
	public var var4="packOne:var4";
}

package packTwo {
	public var p2var1="packTwo:var1";
	public var p2var2="packTwo:var2";
	public var p2var3="packTwo:var3";
	public var p2var4="packTwo:var4";
}

package {
public var number:Number = 50;
public var str1:String = "str1:String";
}

class testClass {
	var testClassVar1 = "Inside testClass";
	function func1() {
		return "Inside func1()";
	}
	function func2() {
		return "Inside func2()";

	}
}

final class finClass {
	var finClassVar1 = "Inside finClass";
	function finFunc1() {
		return "Inside func1()";
	}
	function finFunc2() {
		return "Inside func2()";

	}
}

interface inInterface {
	function intFunc1();
	function intFunc2();
}


class accSpecClass {
	var accSpec1 = "string";
	public function func1() {
		return "Inside func1()-public function";
	}
	function func2() {
		return "Inside func2()-Dynamic function";
	}

}

dynamic class dynClass {
	var dynSpec1 = "string";
	public function func1() {
		return "Inside func1()-public function";
	}
	function func2() {
		return "Inside func2()-Dynamic function";
	}
}

class baseClass
{
  public function overrideFunc() {
  }
}

class childClass extends baseClass
{
}


var str="imported string";
var num=10;


namespace Baseball;
namespace Basketball;

namespace foo = "http://www.macromedia.com/"



