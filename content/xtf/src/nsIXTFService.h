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
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
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

#ifndef __NS_IXTFSERVICE_H__
#define __NS_IXTFSERVICE_H__

#include "nsISupports.h"

class nsIContent;
class nsINodeInfo;

// {02AD2ADD-C5EC-4362-BB5F-E2C69BA76151}
#define NS_IXTFSERVICE_IID                             \
  { 0x02ad2add, 0xc5ec, 0x4362, { 0xbb, 0x5f, 0xe2, 0xc6, 0x9b, 0xa7, 0x61, 0x51 } }

class nsIXTFService : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXTFSERVICE_IID)

    // try to create an xtf element based on namespace
    virtual nsresult CreateElement(nsIContent** aResult,
                                   nsINodeInfo* aNodeInfo)=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXTFService, NS_IXTFSERVICE_IID)

//----------------------------------------------------------------------
// The one class implementing this interface:

// {4EC832DA-6AE7-4185-807B-DADDCB5DA37A}
#define NS_XTFSERVICE_CID                             \
  { 0x4ec832da, 0x6ae7, 0x4185, { 0x80, 0x7b, 0xda, 0xdd, 0xcb, 0x5d, 0xa3, 0x7a } }

#define NS_XTFSERVICE_CONTRACTID "@mozilla.org/xtf/xtf-service;1"

nsresult NS_NewXTFService(nsIXTFService** aResult);

#endif // __NS_IXTFSERVICE_H__

