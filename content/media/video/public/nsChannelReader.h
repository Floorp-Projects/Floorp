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
#if !defined(nsChannelReader_h_)
#define nsChannelReader_h_

#include "nsAutoPtr.h"
#include "nsMediaStream.h"
#include "nsMediaDecoder.h"
#include "nsIPrincipal.h"

#include "oggplay/oggplay.h"

class nsIChannel;
class nsIStreamListener;

class nsChannelReader : public OggPlayReader
{
public:
  nsChannelReader();
  ~nsChannelReader();

  /**
   * Initialize the reader with the given decoder, URI, and
   * optional channel.
   * @param aChannel may be null
   * @param aStreamListener if aChannel is non-null, this will return
   * a stream listener which should be attached to the channel.
   */
  nsresult Init(nsMediaDecoder* aDecoder, nsIURI* aURI, nsIChannel* aChannel,
                nsIStreamListener** aStreamListener);

  // Cancel any blocking request currently in progress and cause that
  // request to return an error. Call on main thread only.
  void Cancel();

  // Return the number of bytes buffered from the file. This can safely
  // be read without blocking.
  PRUint32 Available();

  // Return average number of bytes per second that the 
  // download of the media resource is achieving.
  float DownloadRate();

  // Return average number of bytes per second that the 
  // playback of the media resource is achieving.
  float PlaybackRate();

  nsIPrincipal* GetCurrentPrincipal();
  
  // Implementation of OggPlay Reader API.
  OggPlayErrorCode initialise(int aBlock);
  OggPlayErrorCode destroy();
  size_t io_read(char* aBuffer, size_t aCount);
  int io_seek(long aOffset, int aWhence);
  long io_tell();  
  int duration();
  
public:
  nsMediaStream mStream;
  unsigned long mCurrentPosition;
};

#endif
