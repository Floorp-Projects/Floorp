/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsAboutBookmarks_h__
#define nsAboutBookmarks_h__

#include "nsIAboutModule.h"

class nsAboutBookmarks : public nsIAboutModule 
{
public:
    NS_DECL_ISUPPORTS

    NS_DECL_NSIABOUTMODULE

                      nsAboutBookmarks(PRBool inIsBookmarks);
    virtual           ~nsAboutBookmarks() {}

    static NS_METHOD  CreateBookmarks(nsISupports *aOuter, REFNSIID aIID, void **aResult);
    static NS_METHOD  CreateHistory(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:

    PRBool            mIsBookmarks;   // or history
};

#define NS_ABOUT_BOOKMARKS_MODULE_CID                 \
{ /* AF110FA0-8C4D-11D9-83C4-00 03 93 D7 25 4A */     \
    0xAF110FA0,                                       \
    0x8C4D,                                           \
    0x11D9,                                           \
    { 0x83, 0xC4, 0x00, 0x03, 0x93, 0xD7, 0x25, 0x4A} \
}

#endif // nsAboutBookmarks_h__
