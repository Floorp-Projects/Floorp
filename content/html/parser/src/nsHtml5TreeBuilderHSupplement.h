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
 * Portions created by the Initial Developer are Copyright (C) 2008-2009
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

  private:
#ifdef DEBUG_hsivonen
    static PRUint32 sInsertionBatchMaxLength;
    static PRUint32 sAppendBatchMaxSize;
    static PRUint32 sAppendBatchSlotsExamined;
    static PRUint32 sAppendBatchExaminations;
#endif
    nsHtml5Parser* mParser; // weak ref
    PRBool         mHasProcessedBase;
    PRBool         mFlushing;
    nsTArray<nsHtml5TreeOperation>       mOpQueue;
    nsTArray<nsIContentPtr>              mElementsSeenInThisAppendBatch;
    nsTArray<nsHtml5PendingNotification> mPendingNotifications;
    void           MaybeFlushAndMaybeSuspend();
  public:
    nsHtml5TreeBuilder(nsHtml5Parser* aParser);
    ~nsHtml5TreeBuilder();
    void Flush();
    
    inline void PostPendingAppendNotification(nsIContent* aParent, nsIContent* aChild) {
      PRBool newParent = PR_TRUE;
      const nsIContentPtr* first = mElementsSeenInThisAppendBatch.Elements();
      const nsIContentPtr* last = first + (mElementsSeenInThisAppendBatch.Length() - 1);
      for (const nsIContentPtr* iter = last; iter >= first; --iter) {
#ifdef DEBUG_hsivonen
        sAppendBatchSlotsExamined++;
#endif
        if (*iter == aParent) {
          newParent = PR_FALSE;
          break;
        }
      }
      if (aChild->IsNodeOfType(nsINode::eELEMENT)) {
        mElementsSeenInThisAppendBatch.AppendElement(aChild);
      }
      mElementsSeenInThisAppendBatch.AppendElement(aParent);
      if (newParent) {
        mPendingNotifications.AppendElement(aParent);
      }

#ifdef DEBUG_hsivonen
      sAppendBatchExaminations++;
#endif

    }
    
    inline void FlushPendingAppendNotifications() {
      const nsHtml5PendingNotification* start = mPendingNotifications.Elements();
      const nsHtml5PendingNotification* end = start + mPendingNotifications.Length();
      for (nsHtml5PendingNotification* iter = (nsHtml5PendingNotification*)start; iter < end; ++iter) {
        iter->Fire(this);
      }
      mPendingNotifications.Clear();
#ifdef DEBUG_hsivonen
      if (mElementsSeenInThisAppendBatch.Length() > sAppendBatchMaxSize) {
        sAppendBatchMaxSize = mElementsSeenInThisAppendBatch.Length();
      }
#endif
      mElementsSeenInThisAppendBatch.Clear();
    }
    
    inline void NotifyAppend(nsIContent* aParent, PRUint32 aChildCount) {
      mParser->NotifyAppend(aParent, aChildCount);
    }
    
    inline nsIDocument* GetDocument() {
      return mParser->GetDocument();
    }
    
    inline void SetScriptElement(nsIContent* aScript) {
      mParser->SetScriptElement(aScript);
    }
    
    inline void UpdateStyleSheet(nsIContent* aSheet) {
      mParser->UpdateStyleSheet(aSheet);
    }
    
    inline nsresult ProcessBase(nsIContent* aBase) {
      if (!mHasProcessedBase) {
        nsresult rv = mParser->ProcessBASETag(aBase);
        NS_ENSURE_SUCCESS(rv, rv);
        mHasProcessedBase = PR_TRUE;
      }
      return NS_OK;
    }
    
    inline void StartLayout() {
      mParser->StartLayout(PR_FALSE);
    }
