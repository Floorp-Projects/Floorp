/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the IDispatch implementation for XPConnect.
 *
 * The Initial Developer of the Original Code is
 * David Bradley.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
 * \file
 * This file contains the tests for making sure that JS Objects access via the IDispatch interface
 * work properly
 */

test();

function test()
{
	try
	{
		var variantObj = Components.classes["@mozilla.org/variant;1"].createInstance(Components.interfaces.nsIVariant);
		var jsobj = 
		{
			Boolean : false,
			Short : 0,
			Long : 0,
			Float : 0.0,
			Double : 0.0,
			Currency : 0.0,
			Date : new Date(),
			String : "",
			DispatchPtr : {},
			SCode : 0,
			Variant : variantObj,
			Char : 'a'
		};
		var obj = COMObject(CLSID_nsXPCDispTestWrappedJS);
		reportCompare(
			obj.TestParamTypes(jsobj),
			"",
			"Testing IDispatch wrapped JS objects");
	}
	catch (e)
	{
		reportFailure(e.toString());
	}
}