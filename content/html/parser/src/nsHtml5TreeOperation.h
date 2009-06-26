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
 * The Original Code is HTML Parser C++ Translator code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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
 
#ifndef nsHtml5TreeOperation_h__
#define nsHtml5TreeOperation_h__

#include "nsIContent.h"

class nsHtml5TreeBuilder;

enum eHtml5TreeOperation {
  // main HTML5 ops
  eTreeOpAppend,
  eTreeOpDetach,
  eTreeOpAppendChildrenToNewParent,
  eTreeOpFosterParent,
  eTreeOpAppendToDocument,
  eTreeOpAddAttributes,
  // Gecko-specific on-pop ops
  eTreeOpDoneAddingChildren,
  eTreeOpDoneCreatingElement,
  eTreeOpUpdateStyleSheet,
  eTreeOpProcessBase,
  eTreeOpProcessMeta,
  eTreeOpProcessOfflineManifest,
  eTreeOpStartLayout
};

class nsHtml5TreeOperation {

  public:
    nsHtml5TreeOperation();

    ~nsHtml5TreeOperation();

    inline void Init(nsIContent* aNode, nsIContent* aParent) {
      mNode = aNode;
      mParent = aParent;
    }

    inline void Init(eHtml5TreeOperation aOpCode, nsIContent* aNode) {
      mOpCode = aOpCode;
      mNode = aNode;
    }

    inline void Init(eHtml5TreeOperation aOpCode, 
                     nsIContent* aNode,
                     nsIContent* aParent) {
      mOpCode = aOpCode;
      mNode = aNode;
      mParent = aParent;
    }

    inline void Init(eHtml5TreeOperation aOpCode,
                     nsIContent* aNode,
                     nsIContent* aParent, 
                     nsIContent* aTable) {
      mOpCode = aOpCode;
      mNode = aNode;
      mParent = aParent;
      mTable = aTable;
    }
    inline void DoTraverse(nsCycleCollectionTraversalCallback &cb) {
      nsHtml5TreeOperation* tmp = this;
      NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mNode);
      NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mParent);
      NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mTable);
    }

    nsresult Perform(nsHtml5TreeBuilder* aBuilder);

  private:
    eHtml5TreeOperation mOpCode;
    nsCOMPtr<nsIContent> mNode;
    nsCOMPtr<nsIContent> mParent;
    nsCOMPtr<nsIContent> mTable;
};

#endif // nsHtml5TreeOperation_h__
