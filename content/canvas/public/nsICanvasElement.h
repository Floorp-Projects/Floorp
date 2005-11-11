/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#ifndef nsICanvasElement_h___
#define nsICanvasElement_h___

#include "nsISupports.h"
#include "nsIFrame.h"
#include "imgIContainer.h"

// {8fd94ec9-d4e0-409b-af8f-828c7d7648f5}
#define NS_ICANVASELEMENT_IID \
    { 0x8fd94ec9, 0xd4e0, 0x409b, { 0xaf, 0x8f, 0x82, 0x8c, 0x7d, 0x76, 0x48, 0xf5 } }

class nsICanvasElement : public nsISupports {
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASELEMENT_IID)

  /**
   * Ask the Canvas Element to return the current image container that
   * the frame should render.
   */
  NS_IMETHOD GetCanvasImageContainer (imgIContainer **aImageContainer) = 0;

  /**
   * Ask the canvas Element to return the primary frame, if any
   */
  NS_IMETHOD GetPrimaryCanvasFrame (nsIFrame **aFrame) = 0;

  /**
   * Ask the canvas element to notify the rendering context to update
   * the image frame, in preparation for rendering
   */
  NS_IMETHOD UpdateImageFrame () = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasElement, NS_ICANVASELEMENT_IID)

#endif /* nsICanvasElement_h___ */
