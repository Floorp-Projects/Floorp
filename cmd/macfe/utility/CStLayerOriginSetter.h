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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
CStLayerOriginSetter

This class is used to set the layer origin temporarily.  For example, if you are dragging and
dropping and want the origin to be changed to the item you're dragging, you would do (in
CHTMLView):


do stuff with old origin
{
	CStOriginSetter setLayerOrigin(this, in_layer);
	do your dragging stuff with new origin
}
do other stuff with old origin

This class thus handles any exceptions, etc. that are done while doing stuff with the new
origin.  

NOTE: This class should never be subclassed.
*/

#pragma once

#include "CHTMLView.h"

class CStLayerOriginSetter {

	public:
			CStLayerOriginSetter(CHTMLView *viewToSet, CL_Layer *the_layer);
			~CStLayerOriginSetter();
			
	private:
			Int32 mOldx;
			Int32 mOldy;
			CHTMLView *mViewToSet;
			Boolean mShouldChange;
};