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
 * Boris Zbarsky.
 * Portions created by the Initial Developer are Copyright (C) 2004
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
#ifndef mozFlushType_h___
#define mozFlushType_h___

/**
 * This is the enum used by nsIDocument::FlushPendingNotifications to
 * decide what to flush.
 */
enum mozFlushType {
  Flush_Content           = 0x1,   /* flush the content model construction */
  Flush_SinkNotifications = 0x2,   /* flush the frame model construction */
  Flush_StyleReresolves   = 0x4,   /* flush style reresolution */
  Flush_OnlyReflow        = 0x8,   /* flush reflows */
  Flush_OnlyPaint         = 0x10,  /* flush painting */
  Flush_ContentAndNotify  = (Flush_Content | Flush_SinkNotifications),
  Flush_Frames            = (Flush_Content | Flush_SinkNotifications |
                             Flush_StyleReresolves),
  Flush_Style             = (Flush_Content | Flush_SinkNotifications |
                             Flush_StyleReresolves),
  Flush_Layout            = (Flush_Content | Flush_SinkNotifications |
                             Flush_StyleReresolves | Flush_OnlyReflow),
  Flush_Display           = (Flush_Content | Flush_SinkNotifications |
                             Flush_StyleReresolves | Flush_OnlyReflow |
                             Flush_OnlyPaint)
};

#endif /* mozFlushType_h___ */
