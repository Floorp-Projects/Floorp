/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
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
 * The Original Code is Mozilla Plugins.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "PluginStreamChild.h"
#include "mozilla/plugins/PluginInstanceChild.h"

namespace mozilla {
namespace plugins {

PluginStreamChild::PluginStreamChild()
  : mClosed(false)
{
  memset(&mStream, 0, sizeof(mStream));
  mStream.ndata = static_cast<AStream*>(this);
}

bool
PluginStreamChild::Answer__delete__(const NPReason& reason,
                                    const bool& artificial)
{
  AssertPluginThread();
  if (!artificial)
    NPP_DestroyStream(reason);
  return true;
}

int32_t
PluginStreamChild::NPN_Write(int32_t length, void* buffer)
{
  AssertPluginThread();

  int32_t written = 0;
  CallNPN_Write(nsCString(static_cast<char*>(buffer), length),
                &written);
  if (written < 0)
    PPluginStreamChild::Call__delete__(this, NPERR_GENERIC_ERROR, true);
  // careful after here! |this| just got deleted 

  return written;
}

void
PluginStreamChild::NPP_DestroyStream(NPError reason)
{
  AssertPluginThread();

  if (mClosed)
    return;

  mClosed = true;
  Instance()->mPluginIface->destroystream(
    &Instance()->mData, &mStream, reason);
}

PluginInstanceChild*
PluginStreamChild::Instance()
{
  return static_cast<PluginInstanceChild*>(Manager());
}

} // namespace plugins
} // namespace mozilla
