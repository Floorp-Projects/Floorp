/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLayoutErrors_h___
#define nsLayoutErrors_h___

/** Error codes for nsITableLayout */
#define NS_TABLELAYOUT_CELL_NOT_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 0)

/** Error codes for nsFrame::GetNextPrevLineFromeBlockFrame */
#define NS_POSITION_BEFORE_TABLE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 3)

/** Error codes for nsPresState::GetProperty() */

/** Returned if the property exists */
#define NS_STATE_PROPERTY_EXISTS NS_OK

/** Returned if the property does not exist */
#define NS_STATE_PROPERTY_NOT_THERE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_LAYOUT, 5)


#endif // nsLayoutErrors_h___
