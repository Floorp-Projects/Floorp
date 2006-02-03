/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#ifndef nsMetricsEvent_h__
#define nsMetricsEvent_h__

#include "nsTArray.h"

/**
 * Every metrics event has a unique type.  This value is constrained to 16-bits.
 */
enum nsMetricsEventType {
  NS_METRICS_PROFILE_EVENT,
  NS_METRICS_CUSTOM_EVENT,
  NS_METRICS_LOAD_EVENT,
  NS_METRICS_UI_EVENT,
  // Insert additional event types here
  NS_METRICS_EVENT_MAX
};

/**
 * Base class for metric events.  This class provides basic structure for each
 * event and methods to pack integers and byte arrays into the event.
 */
class nsMetricsEvent {
public:
  nsMetricsEvent(nsMetricsEventType type) : mType(type) {
    mBuf.SetCapacity(16);
  }

  void PutInt8(PRUint8 val) {
    mBuf.AppendElement(val);
  }

  void PutInt16(PRUint16 val) {
    mBuf.AppendElements((PRUint8 *) &val, sizeof(val));
  }

  void PutInt32(PRUint32 val) {
    mBuf.AppendElements((PRUint8 *) &val, sizeof(val));
  }

  void PutBytes(const void *ptr, PRUint32 num) {
    mBuf.AppendElements((PRUint8 *) ptr, num);
  }

  nsMetricsEventType Type() const {
    return mType;
  }

  const PRUint8 *Buffer() const {
    return mBuf.Elements();
  }

  PRUint32 BufferLength() const {
    return mBuf.Length();
  }

private:
  nsMetricsEventType mType;
  nsTArray<PRUint8>  mBuf;
};

#endif  // nsMetricsEvent_h__
