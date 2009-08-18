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

#include "ContentProcessParent.h"

#include "mozilla/ipc/GeckoThread.h"

#include "TabParent.h"
#include "mozilla/ipc/TestShellParent.h"
#include "mozilla/net/NeckoParent.h"

#include "nsIObserverService.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;
using namespace mozilla::net;
using mozilla::MonitorAutoEnter;

namespace {
PRBool gSingletonDied = PR_FALSE;
}

namespace mozilla {
namespace dom {

ContentProcessParent* ContentProcessParent::gSingleton;

ContentProcessParent*
ContentProcessParent::GetSingleton()
{
    if (!gSingleton && !gSingletonDied) {
        nsRefPtr<ContentProcessParent> parent = new ContentProcessParent();
        if (parent) {
            nsCOMPtr<nsIObserverService> obs =
                do_GetService("@mozilla.org/observer-service;1");
            if (obs) {
                if (NS_SUCCEEDED(obs->AddObserver(parent, "xpcom-shutdown",
                                                  PR_FALSE))) {
                    gSingleton = parent;
                }
            }
        }
    }
    return gSingleton;
}

TabParent*
ContentProcessParent::CreateTab(const MagicWindowHandle& hwnd)
{
  return static_cast<TabParent*>(SendPIFrameEmbeddingConstructor(hwnd));
}

TestShellParent*
ContentProcessParent::CreateTestShell()
{
  return static_cast<TestShellParent*>(SendPTestShellConstructor());
}

ContentProcessParent::ContentProcessParent()
    : mMonitor("ContentProcessParent::mMonitor")
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    // TODO: async launching!
    mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, this);
    mSubprocess->SyncLaunch();
    Open(mSubprocess->GetChannel());
}

ContentProcessParent::~ContentProcessParent()
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(gSingleton == this, "More than one singleton?!");
    gSingletonDied = PR_TRUE;
    gSingleton = nsnull;
}

NS_IMPL_ISUPPORTS1(ContentProcessParent, nsIObserver)

NS_IMETHODIMP
ContentProcessParent::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
    if (!strcmp(aTopic, "xpcom-shutdown") && mSubprocess) {
        SendQuit();
#ifdef OS_WIN
        MonitorAutoEnter mon(mMonitor);
        while (mSubprocess) {
            mon.Wait();
        }
#endif
    }
    return NS_OK;
}

void
ContentProcessParent::OnWaitableEventSignaled(base::WaitableEvent *event)
{
    // The child process has died! Sadly we're on the wrong thread to do much.
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
    MonitorAutoEnter mon(mMonitor);
    mSubprocess = nsnull;
    mon.Notify();
}

PIFrameEmbeddingParent*
ContentProcessParent::PIFrameEmbeddingConstructor(
        const MagicWindowHandle& parentWidget)
{
    return new TabParent();
}

nsresult
ContentProcessParent::PIFrameEmbeddingDestructor(PIFrameEmbeddingParent* frame)
{
  delete frame;
  return NS_OK;
}

PTestShellParent*
ContentProcessParent::PTestShellConstructor()
{
  return new TestShellParent();
}

nsresult
ContentProcessParent::PTestShellDestructor(PTestShellParent* shell)
{
  delete shell;
  return NS_OK;
}

PNeckoParent* 
ContentProcessParent::PNeckoConstructor()
{
    return new NeckoParent();
}

nsresult 
ContentProcessParent::PNeckoDestructor(PNeckoParent* necko)
{
    delete necko;
    return NS_OK;
}

} // namespace dom
} // namespace mozilla
