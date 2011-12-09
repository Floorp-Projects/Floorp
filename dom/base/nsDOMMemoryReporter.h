/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (original author)
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

#ifndef nsDOMMemoryReporter_h__
#define nsDOMMemoryReporter_h__

#include "nsIMemoryReporter.h"


/**
 * Helper methods for the DOM Memory Reporter.
 */
namespace mozilla {
  namespace dom {
    namespace MemoryReporter {
      /**
       * It will compute the basic size of an object. This means the size of the
       * object itself plus everything owned by its superclasses.  This will not
       * include the size of objects owned by this objects (which have to be
       * manually added to ::SizeOf), but does include the size of any pointers
       * to those objects stored in this object.
       */
      template <class TypeCurrent, class TypeParent>
      inline PRInt64 GetBasicSize(const TypeCurrent* const obj) {
        return obj->TypeParent::SizeOf() - sizeof(TypeParent)
                                         + sizeof(TypeCurrent);
      }
    }
  }
}

/**
 * Helper macros to declare/implement SizeOf() method for DOM objects.
 */
#define NS_DECL_DOM_MEMORY_REPORTER_SIZEOF  \
  virtual PRInt64 SizeOf() const;

#define NS_DECL_AND_IMPL_DOM_MEMORY_REPORTER_SIZEOF(TypeCurrent, TypeParent) \
  virtual PRInt64 SizeOf() const {                                           \
    return mozilla::dom::MemoryReporter::GetBasicSize<TypeCurrent,           \
                                                      TypeParent>(this);     \
  }

class nsDOMMemoryMultiReporter: public nsIMemoryMultiReporter
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMEMORYMULTIREPORTER

  static void Init();

private:
  // Protect ctor, use Init() instead.
  nsDOMMemoryMultiReporter();
};

#endif // nsDOMMemoryReporter_h__

