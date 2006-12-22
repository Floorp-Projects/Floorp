/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#ifndef nsIXULContentSink_h__
#define nsIXULContentSink_h__

#include "nsIXMLContentSink.h"

class nsIDocument;
class nsXULPrototypeDocument;

// {26b87c63-1b6b-46af-b685-11bbf39aa84a}
#define NS_IXULCONTENTSINK_IID \
{ 0x26b87c63, 0x1b6b, 0x46af, \
  { 0xb6, 0x85, 0x11, 0xbb, 0xf3, 0x9a, 0xa8, 0x4a } }

class nsIXULContentSink : public nsIXMLContentSink
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXULCONTENTSINK_IID)

    /**
     * Initialize the content sink, giving it an nsIDocument object
     * with which to communicate with the outside world, and an
     * nsXULPrototypeDocument to build.
     */
    NS_IMETHOD Init(nsIDocument* aDocument, nsXULPrototypeDocument* aPrototype) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXULContentSink, NS_IXULCONTENTSINK_IID)

nsresult
NS_NewXULContentSink(nsIXULContentSink** aResult);

#endif // nsIXULContentSink_h__
