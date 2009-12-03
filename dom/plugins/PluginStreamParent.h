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

#ifndef mozilla_plugins_PluginStreamParent_h
#define mozilla_plugins_PluginStreamParent_h

#include "mozilla/plugins/PPluginStreamParent.h"
#include "mozilla/plugins/AStream.h"

namespace mozilla {
namespace plugins {

class PluginInstanceParent;

class PluginStreamParent : public PPluginStreamParent, public AStream
{
  friend class PluginModuleParent;
  friend class PluginInstanceParent;

public:
  PluginStreamParent(PluginInstanceParent* npp, const nsCString& mimeType,
                     const nsCString& target, NPError* result);
  virtual ~PluginStreamParent() { }

  NS_OVERRIDE virtual bool IsBrowserStream() { return false; }

  virtual bool AnswerNPN_Write(const Buffer& data, int32_t* written);

  virtual bool Answer__delete__(const NPError& reason, const bool& artificial);

private:
  void NPN_DestroyStream(NPReason reason);

  PluginInstanceParent* mInstance;
  NPStream* mStream;
  bool mClosed;
};

} // namespace plugins
} // namespace mozilla

#endif
