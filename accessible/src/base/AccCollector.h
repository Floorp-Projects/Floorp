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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef AccCollector_h_
#define AccCollector_h_

#include "filters.h"

#include "nscore.h"
#include "nsTArray.h"

/**
 * Collect accessible children complying with filter function. Provides quick
 * access to accessible by index.
 */
class AccCollector
{
public:
  AccCollector(nsAccessible* aRoot, filters::FilterFuncPtr aFilterFunc);
  virtual ~AccCollector();

  /**
   * Return accessible count within the collection.
   */
  PRUint32 Count();

  /**
   * Return an accessible from the collection at the given index.
   */
  nsAccessible* GetAccessibleAt(PRUint32 aIndex);

  /**
   * Return index of the given accessible within the collection.
   */
  virtual PRInt32 GetIndexAt(nsAccessible* aAccessible);

protected:
  /**
   * Ensure accessible at the given index is stored and return it.
   */
  nsAccessible* EnsureNGetObject(PRUint32 aIndex);

  /**
   * Ensure index for the given accessible is stored and return it.
   */
  PRInt32 EnsureNGetIndex(nsAccessible* aAccessible);

  /**
   * Append the object to collection.
   */
  virtual void AppendObject(nsAccessible* aAccessible);

  filters::FilterFuncPtr mFilterFunc;
  nsAccessible* mRoot;
  PRInt32 mRootChildIdx;

  nsTArray<nsAccessible*> mObjects;

private:
  AccCollector();
  AccCollector(const AccCollector&);
  AccCollector& operator =(const AccCollector&);
};

/**
 * Collect embedded objects. Provide quick access to accessible by index and
 * vice versa.
 */
class EmbeddedObjCollector : public AccCollector
{
public:
  virtual ~EmbeddedObjCollector() { };

public:
  virtual PRInt32 GetIndexAt(nsAccessible* aAccessible);

protected:
  // Make sure it's used by nsAccessible class only.
  EmbeddedObjCollector(nsAccessible* aRoot) :
    AccCollector(aRoot, filters::GetEmbeddedObject) { }

  virtual void AppendObject(nsAccessible* aAccessible);

  friend class nsAccessible;
};

#endif
