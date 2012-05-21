/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp sw=2 ts=2 tw=78 et
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsContentErrors_h___
#define nsContentErrors_h___

/** Error codes for nsHTMLStyleSheet */
// XXX this is not really used
#define NS_HTML_STYLE_PROPERTY_NOT_THERE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 2)

/** Error codes for image loading */
#define NS_ERROR_IMAGE_SRC_CHANGED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 4)

#define NS_ERROR_IMAGE_BLOCKED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 5)

/** Error codes for content policy blocking */
#define NS_ERROR_CONTENT_BLOCKED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 6)

#define NS_ERROR_CONTENT_BLOCKED_SHOW_ALT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 7)

/** Success variations of content policy blocking */
#define NS_CONTENT_BLOCKED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 8)

#define NS_CONTENT_BLOCKED_SHOW_ALT \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 9)

#define NS_PROPTABLE_PROP_NOT_THERE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 10)

#define NS_PROPTABLE_PROP_OVERWRITTEN \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 11)

/* Error codes for FindBroadcaster in nsXULDocument.cpp */

#define NS_FINDBROADCASTER_NOT_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 12)

#define NS_FINDBROADCASTER_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 13)

#define NS_FINDBROADCASTER_AWAIT_OVERLAYS \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 14)

/* Error codes for CSP */
#define NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY, 99)

/* Error codes for XBL */
#define NS_ERROR_XBL_BLOCKED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 15)

#endif // nsContentErrors_h___
