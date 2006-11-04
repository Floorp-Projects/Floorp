/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp sw=2 ts=2 tw=78 et
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <jonas@sicking.cc> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsContentErrors_h___
#define nsContentErrors_h___

/** Error codes for nsHTMLStyleSheet */
// XXX this is not really used
#define NS_HTML_STYLE_PROPERTY_NOT_THERE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 2)


/** Error codes for MaybeTriggerAutoLink */
#define NS_XML_AUTOLINK_EMBED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 3)
#define NS_XML_AUTOLINK_NEW \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 4)
#define NS_XML_AUTOLINK_REPLACE \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 5)
#define NS_XML_AUTOLINK_UNDEFINED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 6)

/** Error codes for nsScriptLoader */
#define NS_CONTENT_SCRIPT_IS_EVENTHANDLER \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 7)

/** Error codes for image loading */
#define NS_ERROR_IMAGE_SRC_CHANGED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 8)

#define NS_ERROR_IMAGE_BLOCKED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 9)

/** Error codes for content policy blocking */
#define NS_ERROR_CONTENT_BLOCKED \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 10)

#define NS_ERROR_CONTENT_BLOCKED_SHOW_ALT \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 11)

/** Success variations of content policy blocking */
#define NS_CONTENT_BLOCKED \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 12)

#define NS_CONTENT_BLOCKED_SHOW_ALT \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 13)

#define NS_PROPTABLE_PROP_NOT_THERE \
  NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_CONTENT, 14)

#define NS_PROPTABLE_PROP_OVERWRITTEN \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 15)

/* Error codes for FindBroadcaster in nsXULDocument.cpp */

#define NS_FINDBROADCASTER_NOT_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 16)

#define NS_FINDBROADCASTER_FOUND \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 17)

#define NS_FINDBROADCASTER_AWAIT_OVERLAYS \
  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_CONTENT, 18)

#endif // nsContentErrors_h___
