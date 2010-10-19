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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Humphrey <david.humphrey@senecac.on.ca>
 *  Yury Delendik
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

#ifndef nsAudioAvailableEventManager_h__
#define nsAudioAvailableEventManager_h__

#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsBuiltinDecoder.h"
#include "nsBuiltinDecoderReader.h"

using namespace mozilla;

class nsAudioAvailableEventManager
{
public:
  nsAudioAvailableEventManager(nsBuiltinDecoder* aDecoder);
  ~nsAudioAvailableEventManager();

  // Initialize the event manager with audio metadata.  Called before
  // audio begins to get queued or events are dispatched.
  void Init(PRUint32 aChannels, PRUint32 aRate);

  // Dispatch pending MozAudioAvailable events in the queue.  Called
  // from the state machine thread.
  void DispatchPendingEvents(PRUint64 aCurrentTime);

  // Queues audio sample data and re-packages it into equal sized
  // framebuffers.  Called from the audio thread.
  void QueueWrittenAudioData(SoundDataValue* aAudioData,
                             PRUint32 aAudioDataLength,
                             PRUint64 aEndTimeSampleOffset);

  // Clears the queue of any existing events.  Called from both the state
  // machine and audio threads.
  void Clear();

  // Fires one last event for any extra samples that didn't fit in a whole
  // framebuffer. This is meant to be called only once when the audio finishes.
  // Called from the state machine thread.
  void Drain(PRUint64 aTime);

private:
  // The decoder associated with the event manager.  The event manager shares
  // the same lifetime as the decoder (the decoder holds a reference to the
  // manager).
  nsBuiltinDecoder* mDecoder;

  // The number of samples per second.
  float mSamplesPerSecond;

  // A buffer for audio data to be dispatched in DOM events.
  nsAutoArrayPtr<float> mSignalBuffer;

  // The current size of the signal buffer, may change due to DOM calls.
  PRUint32 mSignalBufferLength;

  // The position of the first available item in mSignalBuffer
  PRUint32 mSignalBufferPosition;

  // The MozAudioAvailable events to be dispatched.  This queue is shared
  // between the state machine and audio threads.
  nsTArray< nsCOMPtr<nsIRunnable> > mPendingEvents;

  // Monitor for shared access to mPendingEvents queue.
  Monitor mMonitor;
};

#endif
