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

#include <LAttachment.h>
//======================================
class CKeyStealingAttachment : public LAttachment
// This guy is added at the front of the attachment list in expanded mode, so that
// certain keypresses go to the message view instead of to the table view.
//======================================
{
public:
					CKeyStealingAttachment(LCommander *inKeyTarget)
					:	LAttachment(msg_KeyPress)
					,	mKeysToSteal(sizeof(char))
					,	mTarget(inKeyTarget) {}	
	virtual void	ExecuteSelf(MessageT inMessage, void *ioParam);
	void			StealKey(char keyToSteal)
					{
						mKeysToSteal.InsertItemsAt(
							1,
							LArray::index_Last,
							&keyToSteal);
					}
protected:
	LCommander*		mTarget;
	LArray			mKeysToSteal;
};

