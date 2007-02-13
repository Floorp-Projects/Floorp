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

package GetSetDiffScope {
	public namespace mx_internal = "http://www.macromedia.com/2005/flex/mx/internal";

	public class GetSetDiffScope {

		private var _s1;
		private var _s2;
		private var _g1 = "original value";
		private var _g2 = 5;
		private var ind:int = 1;

		public function get index():int
		{
			return ind;
		}

		mx_internal function set index(v:int):void
		{
			ind = v;
		}

		// g1 - Setter missing
		public function get g1()
		{
			return _g1;

		}

		// g2 - Setter private
		public function get g2()
		{
			return _g2;

		}
		private function set g2(g) {
			_g2 = s;
		}

		// s1 - Getter missing
		private function set s1(s) {
			_s1 = s;
		}

		// s2 - Getter private
		private function get s2()
		{
			return _s2;

		}
		public function set s2(s) {
			_s2 = s;
		}

		private function blah() {};

	}
	public class testclass
	{
		var getset:GetSetDiffScope;
		function testclass() {
			getset = new GetSetDiffScope();
		}
		public function doGet()
		{
			return getset.index;
		}
		public function doSet()
		{	getset.index = 5;
			return getset.index;
		}
	}

}
