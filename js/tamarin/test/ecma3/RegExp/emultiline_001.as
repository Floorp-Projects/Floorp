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

    var SECTION = "RegExp/multiline-001";
    var VERSION = "ECMA_2";
    var TITLE   = "RegExp: multiline flag";
    var BUGNUMBER="343901";

	startTest();
	writeHeaderToLog(SECTION + " " + TITLE);
	var testcases = getTestCases();
	test();

function getTestCases() {
	var array = new Array();
	var item = 0;

    var woodpeckers = "ivory-billed\ndowny\nhairy\nacorn\nyellow-bellied sapsucker\n" +
        "northern flicker\npileated\n";

    AddRegExpCases( /.*[y]$/m, woodpeckers, woodpeckers.indexOf("downy"), ["downy"] );

    AddRegExpCases( /.*[d]$/m, woodpeckers, woodpeckers.indexOf("ivory-billed"), ["ivory-billed"] );


	function AddRegExpCases
	    ( regexp, pattern, index, matches_array ) {

	    // prevent a runtime error

	    if ( regexp.exec(pattern) == null || matches_array == null ) {
	        array[item++] = new TestCase(SECTION,
	            regexp + ".exec(" + pattern +")",
	            matches_array,
	            regexp.exec(pattern) );

	        return;
	    }

	    array[item++] = new TestCase(SECTION,
	        regexp.toString() + ".exec(" + pattern +").length",
	        matches_array.length,
	        regexp.exec(pattern).length );

	    array[item++] = new TestCase(SECTION,
	        regexp.toString() + ".exec(" + pattern +").index",
	        index,
	        regexp.exec(pattern).index );

	    array[item++] = new TestCase(SECTION,
	        regexp + ".exec(" + pattern +").input",
	        pattern,
	        regexp.exec(pattern).input );


	    for ( var matches = 0; matches < matches_array.length; matches++ ) {
	        array[item++] = new TestCase(SECTION,
	            regexp + ".exec(" + pattern +")[" + matches +"]",
	            matches_array[matches],
	            regexp.exec(pattern)[matches] );
	    }
	}

	return array;
}
