/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
 */

// File Overview....
// 
// Header file for nsIProfile interface test cases

class CProfile
{	
public:

	CProfile(nsIWebBrowser* mWebBrowser);

public:
	~CProfile();

public:
	nsCOMPtr<nsIWebBrowser> qaWebBrowser ;

	// Place Test Cases Here
	void OnStartTests(UINT nMenuID);
	void RunAllTests();
	void GetProfileCount();
	void GetCurrentProfile();
	void SetCurrentProfile();
	void GetProfileList();
	void ProfileExists();
	void CreateNewProfile();
	void RenameProfile();
	void DeleteProfile();
	void CloneProfile();
	void ShutDownCurrentProfile();

private:
protected:


};