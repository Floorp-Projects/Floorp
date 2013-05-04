/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScrollPositionListener_h___
#define nsIScrollPositionListener_h___

#include "nsCoord.h"

/**
 * Provides a way to learn about scroll position changes of nsIScrollableFrame's.
 */
class nsIScrollPositionListener {
public:

	virtual void ScrollPositionWillChange(nscoord aX, nscoord aY) = 0;
	virtual void ScrollPositionDidChange(nscoord aX, nscoord aY) = 0;
};

#endif /* nsIScrollPositionListener_h___ */

