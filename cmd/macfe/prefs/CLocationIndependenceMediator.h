/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#pragma once
#ifdef MOZ_LOC_INDEP
#include "CPrefsMediator.h"

/* Its reason for existence is li.protocol preference, which is a radio text :( */
class CLocationIndependenceMediator : public CPrefsMediator
//======================================
{
	private:
		typedef CPrefsMediator Inherited;
		
	public:

		enum { class_ID = PrefPaneID::eLocationIndependence_Server };
		CLocationIndependenceMediator(LStream*);
		virtual	~CLocationIndependenceMediator();
		virtual	void	LoadPrefs();
		virtual	void	WritePrefs();
};

#endif // MOZ_LOC_INDEP