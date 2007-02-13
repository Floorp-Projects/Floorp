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
package Attrs {

 public class Attrs
 {
	/*=======================================================================*/
	/* Variables                                                             */
	/*=======================================================================*/
                    var emptyVar    : String;
    public          var pubVar      : String;
    private         var privVar     : String;
    static          var statVar     : String;
    public  static  var pubStatVar  : String;
    private static  var privStatVar : String;
    static  public  var statPubVar  : String;
    static  private var statPrivVar : String;

	function Attrs()
	{
		emptyVar    = "var, empty         ";
		pubVar      = "var, public        ";
		privVar     = "var, private       ";
		statVar     = "var, static        ";
		pubStatVar  = "var, public static ";
		privStatVar = "var, private static";
		statPubVar  = "var, static public ";
		statPrivVar = "var, static private";
	};

	function getStatVar()     { return statVar;     };
	function getPubStatVar()  { return pubStatVar;  };
	function getPrivStatVar() { return privStatVar; };
	function getStatPubVar()  { return statPubVar;  };
	function getStatPrivVar() { return statPrivVar; };

	/*=======================================================================*/
	/* Functions                                                             */
	/*=======================================================================*/

	                function emptyFunc()    { return "func, empty         "; };
	public          function pubFunc()      { return "func, public        "; };
	private         function privFunc()     { return "func, private       "; };
	static          function statFunc()     { return "func, static        "; };
	public  static  function pubStatFunc()  { return "func, public static "; };
	private static  function privStatFunc() { return "func, private static"; };
	static  public  function statPubFunc()  { return "func, static public "; };
	static  private function statPrivFunc() { return "func, static private"; };

	function getStatFunc()     { return statFunc();     };
	function getPubStatFunc()  { return pubStatFunc();  };
	function getPrivStatFunc() { return privStatFunc(); };
	function getStatPubFunc()  { return statPubFunc();  };
	function getStatPrivFunc() { return statPrivFunc(); };
 }
}
