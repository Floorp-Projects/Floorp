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
 * The Original Code is Mozilla Firefox.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation <http://www.mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#ifndef mozilla_jetpack_JetpackProcessChild_h
#define mozilla_jetpack_JetpackProcessChild_h

#include "mozilla/ipc/ProcessChild.h"

#include "mozilla/jetpack/JetpackChild.h"

namespace mozilla {
namespace jetpack {

// The JetpackProcessChild class represents the thread where jetpack code is run;
// the main() thread is the I/O thread of the jetpack process.

class JetpackProcessChild : public mozilla::ipc::ProcessChild
{
  typedef mozilla::ipc::ProcessChild ProcessChild;

public:
  JetpackProcessChild(ProcessHandle aParentHandle)
    : ProcessChild(aParentHandle)
  { }

  virtual ~JetpackProcessChild()
  { }

  NS_OVERRIDE virtual bool Init();
  NS_OVERRIDE virtual void CleanUp();

  static JetpackProcessChild* current() {
    return static_cast<JetpackProcessChild*>(ProcessChild::current());
  }

private:
  JetpackChild mJetpack;

  DISALLOW_EVIL_CONSTRUCTORS(JetpackProcessChild);
};

}
}

#endif // mozilla_jetpack_JetpackProcessChild_h
