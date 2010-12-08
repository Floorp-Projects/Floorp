/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
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
 * The Original Code is Mozilla Audio IPC
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@mozilla.com>
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

#ifndef mozilla_dom_AudioParent_h
#define mozilla_dom_AudioParent_h

#include "mozilla/dom/PAudioParent.h"
#include "nsAudioStream.h"
#include "nsITimer.h"

namespace mozilla {
namespace dom {
class AudioParent : public PAudioParent, public nsITimerCallback
{
 public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK

    virtual bool
    RecvWrite(
            const nsCString& data,
            const PRUint32& count);

    virtual bool
    RecvSetVolume(const float& aVolume);

    virtual bool
    RecvDrain();

    virtual bool
    RecvPause();

    virtual bool
    RecvResume();

    virtual bool
    Recv__delete__();

    AudioParent(PRInt32 aNumChannels, PRInt32 aRate, PRInt32 aFormat);
    virtual ~AudioParent();
    virtual void ActorDestroy(ActorDestroyReason);

    nsRefPtr<nsAudioStream> mStream;
    nsCOMPtr<nsITimer> mTimer;

private:
    PRPackedBool mIPCOpen;
};
} // namespace dom
} // namespace mozilla

#endif
