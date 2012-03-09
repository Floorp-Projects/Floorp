/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
   ***** BEGIN LICENSE BLOCK *****
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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
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

#ifndef mozilla_imagelib_DiscardTracker_h_
#define mozilla_imagelib_DiscardTracker_h_

#define DISCARD_TIMEOUT_PREF "image.mem.min_discard_timeout_ms"

class nsITimer;

namespace mozilla {
namespace image {
class RasterImage;

// Struct to make a RasterImage insertable into the tracker list. This
// is embedded within each RasterImage object, and we do 'this->curr = this'
// on RasterImage construction. Thus, a RasterImage must always call
// DiscardTracker::Remove() in its destructor to avoid having the tracker
// point to bogus memory.
struct DiscardTrackerNode
{
  // Pointer to the RasterImage that this node tracks
  RasterImage *curr;

  // Pointers to the previous and next nodes in the list
  DiscardTrackerNode *prev, *next;
};

/**
 * This static class maintains a linked list of RasterImage nodes. When Reset()
 * is called, the node is removed from its position in the list (if it was there
 * before) and appended to the end. When Remove() is called, the node is removed
 * from the list. The timer fires once every MIN_DISCARD_TIMEOUT_MS ms. When it
 * does, it calls Discard() on each container preceding it, and then appends
 * itself to the end of the list. Thus, the discard timeout varies between
 * MIN_DISCARD_TIMEOUT_MS and 2*MIN_DISCARD_TIMEOUT_MS.
 */
class DiscardTracker
{
  public:
    static nsresult Reset(struct DiscardTrackerNode *node);
    static void Remove(struct DiscardTrackerNode *node);
    static void Shutdown();
    static void ReloadTimeout();
    static void DiscardAll();
  private:
    static nsresult Initialize();
    static nsresult TimerOn();
    static void TimerOff();
    static void TimerCallback(nsITimer *aTimer, void *aClosure);
};

} // namespace image
} // namespace mozilla

#endif /* mozilla_imagelib_DiscardTracker_h_ */
