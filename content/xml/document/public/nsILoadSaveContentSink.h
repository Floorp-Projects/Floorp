/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communiactions Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Boris Zbarsky <bzbarsky@mit.edu>  (original author)
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

#ifndef nsILoadSaveContentSink_h__
#define nsILoadSaveContentSink_h__

#include "nsIXMLContentSink.h"

#define NS_ILOADSAVE_CONTENT_SINK_IID \
  { 0xa39ed66a, 0x6ef5, 0x49da, \
  { 0xb6, 0xe4, 0x9e, 0x15, 0x85, 0xf0, 0xba, 0xc9 } }

/**
 * This interface represents a content sink used by the DOMBuilder in
 * DOM3 Load/Save.
 */

class nsILoadSaveContentSink : public nsIXMLContentSink {
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILOADSAVE_CONTENT_SINK_IID)

};

/**
 * The nsIXMLContentSink passed to this method must also implement
 * nsIExpatSink.
 */

nsresult
NS_NewLoadSaveContentSink(nsILoadSaveContentSink** aResult,
                          nsIXMLContentSink* aBaseSink);

#endif // nsILoadSaveContentSink_h__
