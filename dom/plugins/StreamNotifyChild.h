/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=2 ts=2 et : */
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
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

#ifndef mozilla_plugins_StreamNotifyChild_h
#define mozilla_plugins_StreamNotifyChild_h

#include "mozilla/plugins/PStreamNotifyChild.h"

namespace mozilla {
namespace plugins {

class BrowserStreamChild;

class StreamNotifyChild : public PStreamNotifyChild
{
  friend class PluginInstanceChild;
  friend class BrowserStreamChild;

public:
  StreamNotifyChild(const nsCString& aURL)
    : mURL(aURL)
    , mClosure(NULL)
    , mBrowserStream(NULL)
  { }

  NS_OVERRIDE virtual void ActorDestroy(ActorDestroyReason why);

  void SetValid(void* aClosure) {
    mClosure = aClosure;
  }

  void NPP_URLNotify(NPReason reason);

private:
  NS_OVERRIDE virtual bool Recv__delete__(const NPReason& reason);

  /**
   * If a stream is created for this this URLNotify, we associate the objects
   * so that the NPP_URLNotify call is not fired before the stream data is
   * completely delivered. The BrowserStreamChild takes responsibility for
   * calling NPP_URLNotify and deleting this object.
   */
  void SetAssociatedStream(BrowserStreamChild* bs);

  nsCString mURL;
  void* mClosure;

  /**
   * If mBrowserStream is true, it is responsible for deleting this C++ object
   * and DeallocPStreamNotify is not, so that the delayed delivery of
   * NPP_URLNotify is possible.
   */
  BrowserStreamChild* mBrowserStream;
};

} // namespace plugins
} // namespace mozilla

#endif
