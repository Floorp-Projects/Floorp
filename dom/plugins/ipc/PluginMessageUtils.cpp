/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
 * The Original Code is Mozilla Content App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
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

#include "PluginMessageUtils.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#include "PluginInstanceParent.h"
#include "PluginInstanceChild.h"
#include "PluginScriptableObjectParent.h"
#include "PluginScriptableObjectChild.h"

using std::string;

using mozilla::ipc::RPCChannel;

namespace {

class DeferNPObjectReleaseRunnable : public nsRunnable
{
public:
  DeferNPObjectReleaseRunnable(const NPNetscapeFuncs* f, NPObject* o)
    : mFuncs(f)
    , mObject(o)
  {
    NS_ASSERTION(o, "no release null objects");
  }

  NS_IMETHOD Run();

private:
  const NPNetscapeFuncs* mFuncs;
  NPObject* mObject;
};

NS_IMETHODIMP
DeferNPObjectReleaseRunnable::Run()
{
  mFuncs->releaseobject(mObject);
  return NS_OK;
}

} // anonymous namespace

namespace mozilla {
namespace plugins {

NPRemoteWindow::NPRemoteWindow() :
  window(0), x(0), y(0), width(0), height(0), type(NPWindowTypeDrawable)
#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
  , visualID(0)
  , colormap(0)
#endif /* XP_UNIX */
#if defined(XP_WIN)
  ,surfaceHandle(0)
#endif
{
  clipRect.top = 0;
  clipRect.left = 0;
  clipRect.bottom = 0;
  clipRect.right = 0;
}

RPCChannel::RacyRPCPolicy
MediateRace(const RPCChannel::Message& parent,
            const RPCChannel::Message& child)
{
  switch (parent.type()) {
  case PPluginInstance::Msg_Paint__ID:
  case PPluginInstance::Msg_NPP_SetWindow__ID:
  case PPluginInstance::Msg_NPP_HandleEvent_Shmem__ID:
  case PPluginInstance::Msg_NPP_HandleEvent_IOSurface__ID:
    // our code relies on the frame list not changing during paints and
    // reflows
    return RPCChannel::RRPParentWins;

  default:
    return RPCChannel::RRPChildWins;
  }
}

static string
ReplaceAll(const string& haystack, const string& needle, const string& with)
{
  string munged = haystack;
  string::size_type i = 0;

  while (string::npos != (i = munged.find(needle, i))) {
    munged.replace(i, needle.length(), with);
    i += with.length();
  }

  return munged;
}

string
MungePluginDsoPath(const string& path)
{
#if defined(OS_LINUX)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=519601
  return ReplaceAll(path, "netscape", "netsc@pe");
#else
  return path;
#endif
}

string
UnmungePluginDsoPath(const string& munged)
{
#if defined(OS_LINUX)
  return ReplaceAll(munged, "netsc@pe", "netscape");
#else
  return munged;
#endif
}


PRLogModuleInfo* gPluginLog = PR_NewLogModule("IPCPlugins");

void
DeferNPObjectLastRelease(const NPNetscapeFuncs* f, NPObject* o)
{
  if (!o)
    return;

  if (o->referenceCount > 1) {
    f->releaseobject(o);
    return;
  }

  NS_DispatchToCurrentThread(new DeferNPObjectReleaseRunnable(f, o));
}

void DeferNPVariantLastRelease(const NPNetscapeFuncs* f, NPVariant* v)
{
  if (!NPVARIANT_IS_OBJECT(*v)) {
    f->releasevariantvalue(v);
    return;
  }
  DeferNPObjectLastRelease(f, v->value.objectValue);
  VOID_TO_NPVARIANT(*v);
}

#ifdef XP_WIN

// The private event used for double-pass widgetless plugin rendering.
UINT DoublePassRenderingEvent()
{
  static UINT gEventID = 0;
  if (!gEventID)
    gEventID = ::RegisterWindowMessage(L"MozDoublePassMsg");
  return gEventID;
}

#endif

} // namespace plugins
} // namespace mozilla
