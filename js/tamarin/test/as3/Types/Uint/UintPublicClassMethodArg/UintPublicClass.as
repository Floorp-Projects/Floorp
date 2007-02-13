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
// author: Michael Tilburg

package UintPublicClassMethodArg {


	public class UintPublicClass{

		public var pubProp:uint;
		public static var pubStatProp:uint;

		public function oneArg(arg:uint):uint {
			return arg;
		}

		public function twoArg(arg1:uint, arg2:uint):uint {
			return arg1+arg2;
		}

		public function threeArg(arg1:uint, arg2:uint, arg3:uint):uint {
			return arg1+arg2+arg3;
		}

		public function diffArg(arg1:uint, arg2:int, arg3:Number):uint{
			return arg1+arg2+arg3;
		}

		public function diffArg2(arg1:int, arg2:uint, arg3:Number):uint{
			return arg1+arg2+arg3;
		}

		public function diffArg3(arg1:Number, arg2:int, arg3:uint):uint{
			return arg1+arg2+arg3;
		}

		public function useProp(arg1:uint):uint {
			pubProp = 10;
			return pubProp+arg1;
		}

	}
}



