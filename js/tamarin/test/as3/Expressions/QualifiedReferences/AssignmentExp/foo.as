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
package ns {
	public namespace N1;
	public namespace N2;
        
	public namespace N3 = "foo";
	public namespace N4 = N3;
	 
 	public class foo {
		public var v1;
		public var v2;
		public var v3;
                public var o:Object;
		var holder;
		
		N1 var v3;
		N2 var n2;
		N2 var n3;
		N2 var n4;
		N3 var v1;
		N3 var v2;
                
	
		public function foo() {
			v1 = 1;
			v2 = 2;
			v3 = 5;
                        

			 o = new Object();
			 o.v4 = 4;
			 holder = "v4";

			 

			 N1::v3 = v1+v2;
			 N2::n2 = N1::v3;
			 N2::n3 = v3;
			 N2::n4 = o[holder];
			 N3::v1 = v3;
                         try{
			     N3::['v2'] = v3;
                         }catch(e:Error){
                             thisError="no error";
                         }finally{
                          }
                         N4::v1 = v3;
                         N4::v2 = v3;

		}
	}
}
