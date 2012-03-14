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
 * The Original Code is B2G Audio Manager.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Philipp von Weitershausen <philipp@weitershausen.de>
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

#ifndef mozilla_dom_system_b2g_audiomanager_h__
#define mozilla_dom_system_b2g_audiomanager_h__

#include "nsIAudioManager.h"


// {b2b51423-502d-4d77-89b3-7786b562b084}
#define NS_AUDIOMANAGER_CID {0x94f6fd70, 0x7615, 0x4af9, \
      {0x89, 0x10, 0xf9, 0x3c, 0x55, 0xe6, 0x62, 0xec}}
#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"

namespace mozilla {
namespace dom {
namespace gonk {

class AudioManager : public nsIAudioManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIAUDIOMANAGER

  AudioManager() : mPhoneState(PHONE_STATE_CURRENT)
  {
  }

protected:
  PRInt32 mPhoneState;
};


} /* namespace telephony */
} /* namespace dom */
} /* namespace mozilla */

#endif // mozilla_dom_system_b2g_audiomanager_h__
