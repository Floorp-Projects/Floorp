/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


//returns the location of N7.x install directory 
extern "C" _declspec(dllexport) char*  GetNSInstallDir();

//returns absolute path to all-ns.js pref default file
extern "C" _declspec(dllexport) char*  GetPrefTemplateFilePath();

//returns absolute path to all.js pref default file
extern "C" _declspec(dllexport) char*  GetNSPrefDefaultsFilePath();

//returns the location of the current profile [user-profile-dir]
extern "C" _declspec(dllexport) char*  GetNSCurrentProfile();

//returns the location of profile-name, [user-profile-dir]
extern "C" _declspec(dllexport) char*  GetNSUserProfile(char* profileName);

//returns absolute path to pref.js in [user-profile-dir]
extern "C" _declspec(dllexport) char*  GetNSUserProfileJS(char* profileName);

//appends "path_to_isp.js" to "[install-dir]\defaults\pref\target-default-pref-filename". 
//When "target-default-pref-filename" is not specified, i.e., "null", the default is "all-ns.js".
extern "C" _declspec(dllexport) int    AppendDefaultPref(char* templateFileAbsoutePath, char* prefDefaultFileName);

//appends "path_to_isp.js" to "[user-profile-dir]\target-default-user-filename". 
//When "target-default-user-filename" is not specified, i.e., "null", the default is 
//"prefs.js". Npte that the JS function. "pref()" in "isp.js" needs to be converted 
//into "user_pref()" before appended to "target-user-pref-filename".
extern "C" _declspec(dllexport) int    AppendUserPref(char* templateFileAbsoutePath, char* profileName);
