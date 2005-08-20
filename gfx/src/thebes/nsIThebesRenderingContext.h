/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Christopher Blizzard <blizzard@mozilla.org>.  
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#ifndef __nsIThebesRenderingContext_h
#define __nsIThebesRenderingContext_h

#include "nsIRenderingContext.h"

// IID for the nsIRenderingContext interface
#define NSI_THEBES_RENDERING_CONTEXT_IID \
{ 0x8591c4c6, 0x41d4, 0x485a, \
{ 0xb2, 0x4f, 0x9d, 0xe1, 0x9b, 0x69, 0xce, 0x02 } }

class nsIThebesRenderingContext : public nsISupports
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NSI_THEBES_RENDERING_CONTEXT_IID)

    NS_IMETHOD CreateDrawingSurface(nsNativeWidget aWidget, nsIDrawingSurface* &aSurface) = 0;

};

#endif /* __nsIThebesRenderingContext_h */
