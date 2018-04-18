/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __OmxCoreLibLinker_h__
#define __OmxCoreLibLinker_h__

struct PRLibrary;

namespace mozilla
{

class OmxCoreLibLinker
{
public:
  static bool Link();
  static void Unlink();

private:
  static PRLibrary* sLinkedLib;
  static const char* sLibName;
  static const char* sLibNames[];

  static bool TryLinkingLibrary(const char *libName);
  static bool Bind(const char* aLibName);

  static enum LinkStatus
  {
    LinkStatus_INIT = 0,
    LinkStatus_FAILED,
    LinkStatus_SUCCEEDED
  } sLinkStatus;
};

}

#endif // __OmxCoreLibLinker_h__
