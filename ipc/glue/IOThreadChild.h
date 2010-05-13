/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
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
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Benoit Girard <b56girard@gmail.com>.
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

#ifndef dom_plugins_IOThreadChild_h
#define dom_plugins_IOThreadChild_h 1

#include "chrome/common/child_thread.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

// The IOThreadChild class represents a background thread where the
// IPC IO MessageLoop lives.
class IOThreadChild : public ChildThread {
public:
  IOThreadChild()
    : ChildThread(base::Thread::Options(MessageLoop::TYPE_IO,
                                        0)) // stack size
  { }

  ~IOThreadChild()
  { }

  static MessageLoop* message_loop() {
    return IOThreadChild::current()->Thread::message_loop();
  }

  // IOThreadChild owns the returned IPC::Channel.
  static IPC::Channel* channel() {
    return IOThreadChild::current()->ChildThread::channel();
  }

protected:
  static IOThreadChild* current() {
    return static_cast<IOThreadChild*>(ChildThread::current());
  }

private:
  DISALLOW_EVIL_CONSTRUCTORS(IOThreadChild);
};

}  // namespace plugins
}  // namespace mozilla

#endif  // ifndef dom_plugins_IOThreadChild_h
