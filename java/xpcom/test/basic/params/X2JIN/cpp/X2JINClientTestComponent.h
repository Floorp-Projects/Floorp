/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
 Client QA Team, St. Petersburg, Russia
*/

#include "iX2JINClientTestComponent.h"
#include "iX2JINServerTestComponent.h"
#include "iClientTestComponent.h"
#include "iExclusionSupport.h"
#include "ProceedResults.h"
#include "VarContainer.h"
#include "PseudoHash.h"

class X2JINClientTestComponentImpl : 	public iX2JINClientTestComponent,
					public iClientTestComponent,
					public iExclusionSupport
{
 public:
	X2JINClientTestComponentImpl();
	virtual ~X2JINClientTestComponentImpl();
	void TestByte(void);
	void TestShort(void);
	void TestLong(void);
	void TestLonglong(void);
	void TestUShort(void);
	void TestULong(void);
	void TestULonglong(void);
	void TestFloat(void);
	void TestDouble(void);
	void TestBoolean(void);
	void TestChar(void);
	void TestWChar(void);
	void TestString(void);
	void TestWString(void);
	void TestStringArray(void);
	void TestLongArray(void);
	void TestCharArray(void);
	void TestMixed(void);
	void TestObject(void);
	void TestIID(void);
	void TestCID(void);

	NS_DECL_ISUPPORTS
	NS_DECL_ICLIENTTESTCOMPONENT
	NS_DECL_IEXCLUSIONSUPPORT
  
	DECL_PROCEED_RESULTS
	DECL_VAR_STACKS
};
	DECL_PSEUDOHASH
