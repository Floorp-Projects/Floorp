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

#ifndef mozilla_dom_ContentProcessParent_h
#define mozilla_dom_ContentProcessParent_h

#include "base/waitable_event_watcher.h"

#include "mozilla/dom/PContentProcessParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"

#include "nsIObserver.h"
#include "mozilla/Monitor.h"

namespace mozilla {

namespace ipc {
class TestShellParent;
}

namespace dom {

class TabParent;

class ContentProcessParent : private PContentProcessParent,
                             public base::WaitableEventWatcher::Delegate,
                             public nsIObserver
{
private:
    typedef mozilla::ipc::GeckoChildProcessHost GeckoChildProcessHost;

public:
    static ContentProcessParent* GetSingleton();

#if 0
    // TODO: implement this somewhere!
    static ContentProcessParent* FreeSingleton();
#endif

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER

    virtual void OnWaitableEventSignaled(base::WaitableEvent *event);

    TabParent* CreateTab(const MagicWindowHandle& hwnd);
    mozilla::ipc::TestShellParent* CreateTestShell();

private:
    static ContentProcessParent* gSingleton;

    // Hide the raw constructor methods since we don't want client code
    // using them.
    using PContentProcessParent::SendPIFrameEmbeddingConstructor;
    using PContentProcessParent::SendPTestShellConstructor;

    ContentProcessParent();
    virtual ~ContentProcessParent();

    virtual PIFrameEmbeddingParent* PIFrameEmbeddingConstructor(
            const MagicWindowHandle& parentWidget);
    virtual nsresult PIFrameEmbeddingDestructor(PIFrameEmbeddingParent* frame);

    virtual PTestShellParent* PTestShellConstructor();
    virtual nsresult PTestShellDestructor(PTestShellParent* shell);

    virtual PNeckoParent* PNeckoConstructor();
    virtual nsresult PNeckoDestructor(PNeckoParent* necko);

    mozilla::Monitor mMonitor;

    GeckoChildProcessHost* mSubprocess;
};

} // namespace dom
} // namespace mozilla

#endif
