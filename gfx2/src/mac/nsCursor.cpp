/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsCursor.h"

#include <Appearance.h>

NS_IMPL_ISUPPORTS1(nsCursor, nsICursor)

nsCursor::nsCursor() :
  mCursor(0)
{
  NS_INIT_ISUPPORTS();
}

nsCursor::~nsCursor()
{
}

NS_IMETHODIMP nsCursor::Init(const PRInt32 aCursor)
{
    short cursor = -1;
    switch (aCursor) {
    case nsICursor::standard:				cursor = kThemeArrowCursor;             break;
    case nsICursor::wait:					cursor = kThemeWatchCursor;             break;
    case nsICursor::select:					cursor = kThemeIBeamCursor;             break;
    case nsICursor::hyperlink:				cursor = kThemePointingHandCursor;      break;
    case nsICursor::sizeWE:					cursor = kThemeResizeLeftRightCursor;   break;
    case nsICursor::sizeNS:					cursor = 129; 	                        break;
#if 0
    case nsICursor::sizeNW:					cursor = 130; 	                        break;
    case nsICursor::sizeSE:					cursor = 131; 	                        break;
    case nsICursor::sizeNE:					cursor = 132;                           break;
    case nsICursor::sizeSW:					cursor = 133;                           break;
#endif
    case nsICursor::arrow_north:			cursor = 134;                           break;
    case nsICursor::arrow_north_plus:       cursor = 135;                           break;
    case nsICursor::arrow_south:			cursor = 136;                           break;
    case nsICursor::arrow_south_plus:       cursor = 137;                           break;
    case nsICursor::arrow_west:			    cursor = 138;                           break;
    case nsICursor::arrow_west_plus:	    cursor = 139;                           break;
    case nsICursor::arrow_east:             cursor = 140;                           break;
    case nsICursor::arrow_east_plus:        cursor = 141;                           break;
    case nsICursor::crosshair:				cursor = kThemeCrossCursor;             break;
    case nsICursor::move:                   cursor = kThemeOpenHandCursor;          break;
    case nsICursor::help:                   cursor = 143;                           break;
#if 0
    case nsICursor::copy:                   cursor = 144;                           break; // CSS3
    case nsICursor::alias:                  cursor = 145;                           break;
    case nsICursor::context_menu:           cursor = 146;                           break;
    case nsICursor::cell:                   cursor = kThemePlusCursor;              break;
    case nsICursor::grab:                   cursor = kThemeOpenHandCursor;          break;
    case nsICursor::grabbing:				cursor = kThemeClosedHandCursor;        break;
    case nsICursor::spinning:				cursor = kThemeSpinningCursor;          break;
    case nsICursor::count_up:				cursor = kThemeCountingUpHandCursor;    break;
    case nsICursor::count_down:			    cursor = kThemeCountingDownHandCursor;  break;
    case nsICursor::count_up_down:		    cursor = kThemeCountingUpAndDownHandCursor; break;
#endif
    }
    if (cursor >= 0) {
        if (cursor >= 128) {
            //nsMacResources::OpenLocalResourceFile();
            mCursor = ::GetCursor(cursor);
            //nsMacResources::CloseLocalResourceFile();
            if (mCursor != NULL) {
                SInt8 state = ::HGetState(Handle(mCursor));
                ::HLock(Handle(mCursor));
                ::SetCursor(*mCursor);
                ::HSetState(Handle(mCursor), state);
            }
        } else {
            ::ShowCursor();
            ::SetThemeCursor(cursor);
        }
    }
    return NS_OK;
}
