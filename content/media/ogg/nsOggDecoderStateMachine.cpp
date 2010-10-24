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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Double <chris.double@double.co.nz>
 *  Chris Pearce <chris@pearce.org.nz>
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

#include "nsOggDecoderStateMachine.h"
#include "nsOggReader.h"
#include "nsOggDecoder.h"

nsOggDecoderStateMachine::nsOggDecoderStateMachine(nsBuiltinDecoder* aDecoder) :
  nsBuiltinDecoderStateMachine(aDecoder, new nsOggReader(aDecoder))
{
}

void nsOggDecoderStateMachine::LoadMetadata()
{
  nsBuiltinDecoderStateMachine::LoadMetadata();

  // Get the duration from the media file. We only do this if the
  // content length of the resource is known as we need to seek
  // to the end of the file to get the last time field. We also
  // only do this if the resource is seekable and if we haven't
  // already obtained the duration via an HTTP header.

  if (mState != DECODER_STATE_SHUTDOWN &&
      mDecoder->GetCurrentStream()->GetLength() >= 0 &&
      mSeekable &&
      mEndTime == -1)
  {
    mDecoder->StopProgressUpdates();
    FindEndTime();
    mDecoder->StartProgressUpdates();
  }
}
