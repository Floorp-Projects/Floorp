/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * June 30, 2009.
 *
 * The Initial Developer of the Original Code is
 *   Andreas Gal <gal@mozilla.com>
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jstask_h___
#define jstask_h___

class JSBackgroundTask {
    friend class JSBackgroundThread;
    JSBackgroundTask* next;
  public:
    virtual ~JSBackgroundTask() {}
    virtual void run() = 0;
};

#ifdef JS_THREADSAFE

#include "prthread.h"
#include "prlock.h"
#include "prcvar.h"

class JSBackgroundThread {
    PRThread*         thread;
    JSBackgroundTask* stack;
    PRLock*           lock;
    PRCondVar*        wakeup;
    bool              shutdown;

  public:
    JSBackgroundThread();
    ~JSBackgroundThread();

    bool init();
    void cancel();
    void work();
    bool busy();
    void schedule(JSBackgroundTask* task);
};

#else

class JSBackgroundThread {
  public:
    void schedule(JSBackgroundTask* task) {
        task->run();
    }
};

#endif

#endif /* jstask_h___ */
