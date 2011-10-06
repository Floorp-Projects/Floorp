/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
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
 * The Original Code is layout debugging code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
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

#include "nsLayoutDebuggingTools.h"

#include "nsIDocShell.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeItem.h"
#include "nsPIDOMWindow.h"
#include "nsIDocumentViewer.h"

#include "nsIServiceManager.h"
#include "nsIAtom.h"
#include "nsQuickSort.h"

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"

#include "nsIPresShell.h"
#include "nsIViewManager.h"
#include "nsIFrame.h"

#include "nsILayoutDebugger.h"
#include "nsLayoutCID.h"
static NS_DEFINE_CID(kLayoutDebuggerCID, NS_LAYOUT_DEBUGGER_CID);

#include "nsISelectionController.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"

using namespace mozilla;

static already_AddRefed<nsIContentViewer>
doc_viewer(nsIDocShell *aDocShell)
{
    if (!aDocShell)
        return nsnull;
    nsIContentViewer *result = nsnull;
    aDocShell->GetContentViewer(&result);
    return result;
}

static already_AddRefed<nsIPresShell>
pres_shell(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIDocumentViewer> dv =
        do_QueryInterface(nsCOMPtr<nsIContentViewer>(doc_viewer(aDocShell)));
    if (!dv)
        return nsnull;
    nsIPresShell *result = nsnull;
    dv->GetPresShell(&result);
    return result;
}

#if 0 // not currently needed
static already_AddRefed<nsPresContext>
pres_context(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIDocumentViewer> dv =
        do_QueryInterface(nsCOMPtr<nsIContentViewer>(doc_viewer(aDocShell)));
    if (!dv)
        return nsnull;
    nsPresContext *result = nsnull;
    dv->GetPresContext(result);
    return result;
}
#endif

static nsIViewManager*
view_manager(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIPresShell> shell(pres_shell(aDocShell));
    if (!shell)
        return nsnull;
    return shell->GetViewManager();
}

#ifdef DEBUG
static already_AddRefed<nsIDocument>
document(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIContentViewer> cv(doc_viewer(aDocShell));
    if (!cv)
        return nsnull;
    nsCOMPtr<nsIDOMDocument> domDoc;
    cv->GetDOMDocument(getter_AddRefs(domDoc));
    if (!domDoc)
        return nsnull;
    nsIDocument *result = nsnull;
    CallQueryInterface(domDoc, &result);
    return result;
}
#endif

nsLayoutDebuggingTools::nsLayoutDebuggingTools()
  : mPaintFlashing(PR_FALSE),
    mPaintDumping(PR_FALSE),
    mInvalidateDumping(PR_FALSE),
    mEventDumping(PR_FALSE),
    mMotionEventDumping(PR_FALSE),
    mCrossingEventDumping(PR_FALSE),
    mReflowCounts(PR_FALSE)
{
    NewURILoaded();
}

nsLayoutDebuggingTools::~nsLayoutDebuggingTools()
{
}

NS_IMPL_ISUPPORTS1(nsLayoutDebuggingTools, nsILayoutDebuggingTools)

NS_IMETHODIMP
nsLayoutDebuggingTools::Init(nsIDOMWindow *aWin)
{
    if (!Preferences::GetService()) {
        return NS_ERROR_UNEXPECTED;
    }

    {
        nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aWin);
        if (!window)
            return NS_ERROR_UNEXPECTED;
        mDocShell = window->GetDocShell();
    }
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_UNEXPECTED);

    mPaintFlashing =
        Preferences::GetBool("nglayout.debug.paint_flashing", mPaintFlashing);
    mPaintDumping =
        Preferences::GetBool("nglayout.debug.paint_dumping", mPaintDumping);
    mInvalidateDumping =
        Preferences::GetBool("nglayout.debug.invalidate_dumping", mInvalidateDumping);
    mEventDumping =
        Preferences::GetBool("nglayout.debug.event_dumping", mEventDumping);
    mMotionEventDumping =
        Preferences::GetBool("nglayout.debug.motion_event_dumping",
                             mMotionEventDumping);
    mCrossingEventDumping =
        Preferences::GetBool("nglayout.debug.crossing_event_dumping",
                             mCrossingEventDumping);
    mReflowCounts =
        Preferences::GetBool("layout.reflow.showframecounts", mReflowCounts);

    {
        nsCOMPtr<nsILayoutDebugger> ld = do_GetService(kLayoutDebuggerCID);
        if (ld) {
            ld->GetShowFrameBorders(&mVisualDebugging);
            ld->GetShowEventTargetFrameBorder(&mVisualEventDebugging);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::NewURILoaded()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
    // Reset all the state that should be reset between pages.

    // XXX Some of these should instead be transferred between pages!
    mEditorMode = PR_FALSE;
    mVisualDebugging = PR_FALSE;
    mVisualEventDebugging = PR_FALSE;

    mReflowCounts = PR_FALSE;

    ForceRefresh();
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetVisualDebugging(bool *aVisualDebugging)
{
    *aVisualDebugging = mVisualDebugging;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetVisualDebugging(bool aVisualDebugging)
{
    nsCOMPtr<nsILayoutDebugger> ld = do_GetService(kLayoutDebuggerCID);
    if (!ld)
        return NS_ERROR_UNEXPECTED;
    mVisualDebugging = aVisualDebugging;
    ld->SetShowFrameBorders(aVisualDebugging);
    ForceRefresh();
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetVisualEventDebugging(bool *aVisualEventDebugging)
{
    *aVisualEventDebugging = mVisualEventDebugging;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetVisualEventDebugging(bool aVisualEventDebugging)
{
    nsCOMPtr<nsILayoutDebugger> ld = do_GetService(kLayoutDebuggerCID);
    if (!ld)
        return NS_ERROR_UNEXPECTED;
    mVisualEventDebugging = aVisualEventDebugging;
    ld->SetShowEventTargetFrameBorder(aVisualEventDebugging);
    ForceRefresh();
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetPaintFlashing(bool *aPaintFlashing)
{
    *aPaintFlashing = mPaintFlashing;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetPaintFlashing(bool aPaintFlashing)
{
    mPaintFlashing = aPaintFlashing;
    return SetBoolPrefAndRefresh("nglayout.debug.paint_flashing", mPaintFlashing);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetPaintDumping(bool *aPaintDumping)
{
    *aPaintDumping = mPaintDumping;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetPaintDumping(bool aPaintDumping)
{
    mPaintDumping = aPaintDumping;
    return SetBoolPrefAndRefresh("nglayout.debug.paint_dumping", mPaintDumping);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetInvalidateDumping(bool *aInvalidateDumping)
{
    *aInvalidateDumping = mInvalidateDumping;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetInvalidateDumping(bool aInvalidateDumping)
{
    mInvalidateDumping = aInvalidateDumping;
    return SetBoolPrefAndRefresh("nglayout.debug.invalidate_dumping", mInvalidateDumping);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetEventDumping(bool *aEventDumping)
{
    *aEventDumping = mEventDumping;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetEventDumping(bool aEventDumping)
{
    mEventDumping = aEventDumping;
    return SetBoolPrefAndRefresh("nglayout.debug.event_dumping", mEventDumping);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetMotionEventDumping(bool *aMotionEventDumping)
{
    *aMotionEventDumping = mMotionEventDumping;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetMotionEventDumping(bool aMotionEventDumping)
{
    mMotionEventDumping = aMotionEventDumping;
    return SetBoolPrefAndRefresh("nglayout.debug.motion_event_dumping", mMotionEventDumping);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetCrossingEventDumping(bool *aCrossingEventDumping)
{
    *aCrossingEventDumping = mCrossingEventDumping;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetCrossingEventDumping(bool aCrossingEventDumping)
{
    mCrossingEventDumping = aCrossingEventDumping;
    return SetBoolPrefAndRefresh("nglayout.debug.crossing_event_dumping", mCrossingEventDumping);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::GetReflowCounts(bool* aShow)
{
    *aShow = mReflowCounts;
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetReflowCounts(bool aShow)
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
    nsCOMPtr<nsIPresShell> shell(pres_shell(mDocShell)); 
    if (shell) {
#ifdef MOZ_REFLOW_PERF
        shell->SetPaintFrameCount(aShow);
        SetBoolPrefAndRefresh("layout.reflow.showframecounts", aShow);
        mReflowCounts = aShow;
#else
        printf("************************************************\n");
        printf("Sorry, you have not built with MOZ_REFLOW_PERF=1\n");
        printf("************************************************\n");
#endif
    }
    return NS_OK;
}

static void DumpAWebShell(nsIDocShellTreeItem* aShellItem, FILE* out, PRInt32 aIndent)
{
    nsXPIDLString name;
    nsCOMPtr<nsIDocShellTreeItem> parent;
    PRInt32 i, n;

    for (i = aIndent; --i >= 0; )
        fprintf(out, "  ");

    fprintf(out, "%p '", static_cast<void*>(aShellItem));
    aShellItem->GetName(getter_Copies(name));
    aShellItem->GetSameTypeParent(getter_AddRefs(parent));
    fputs(NS_LossyConvertUTF16toASCII(name).get(), out);
    fprintf(out, "' parent=%p <\n", static_cast<void*>(parent));

    ++aIndent;
    nsCOMPtr<nsIDocShellTreeNode> shellAsNode(do_QueryInterface(aShellItem));
    shellAsNode->GetChildCount(&n);
    for (i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        shellAsNode->GetChildAt(i, getter_AddRefs(child));
        if (child) {
            DumpAWebShell(child, out, aIndent);
        }
    }
    --aIndent;
    for (i = aIndent; --i >= 0; )
        fprintf(out, "  ");
    fputs(">\n", out);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpWebShells()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
    nsCOMPtr<nsIDocShellTreeItem> shellAsItem(do_QueryInterface(mDocShell));
    DumpAWebShell(shellAsItem, stdout, 0);
    return NS_OK;
}

static
void
DumpContentRecur(nsIDocShell* aDocShell, FILE* out)
{
#ifdef DEBUG
    if (nsnull != aDocShell) {
        fprintf(out, "docshell=%p \n", static_cast<void*>(aDocShell));
        nsCOMPtr<nsIDocument> doc(document(aDocShell));
        if (doc) {
            dom::Element *root = doc->GetRootElement();
            if (root) {
                root->List(out);
            }
        }
        else {
            fputs("no document\n", out);
        }
        // dump the frames of the sub documents
        PRInt32 i, n;
        nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
        docShellAsNode->GetChildCount(&n);
        for (i = 0; i < n; ++i) {
            nsCOMPtr<nsIDocShellTreeItem> child;
            docShellAsNode->GetChildAt(i, getter_AddRefs(child));
            nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
            if (child) {
                DumpContentRecur(childAsShell, out);
            }
        }
    }
#endif
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpContent()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
    DumpContentRecur(mDocShell, stdout);
    return NS_OK;
}

static void
DumpFramesRecur(nsIDocShell* aDocShell, FILE* out)
{
#ifdef DEBUG
    fprintf(out, "webshell=%p \n", static_cast<void*>(aDocShell));
    nsCOMPtr<nsIPresShell> shell(pres_shell(aDocShell));
    if (shell) {
        nsIFrame* root = shell->GetRootFrame();
        if (root) {
            root->List(out, 0);
        }
    }
    else {
        fputs("null pres shell\n", out);
    }

    // dump the frames of the sub documents
    PRInt32 i, n;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        docShellAsNode->GetChildAt(i, getter_AddRefs(child));
        nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
        if (childAsShell) {
            DumpFramesRecur(childAsShell, out);
        }
    }
#endif
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpFrames()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
    DumpFramesRecur(mDocShell, stdout);
    return NS_OK;
}

static
void
DumpViewsRecur(nsIDocShell* aDocShell, FILE* out)
{
#ifdef DEBUG
    fprintf(out, "docshell=%p \n", static_cast<void*>(aDocShell));
    nsCOMPtr<nsIViewManager> vm(view_manager(aDocShell));
    if (vm) {
        nsIView* root = vm->GetRootView();
        if (root) {
            root->List(out);
        }
    }
    else {
        fputs("null view manager\n", out);
    }

    // dump the views of the sub documents
    PRInt32 i, n;
    nsCOMPtr<nsIDocShellTreeNode> docShellAsNode(do_QueryInterface(aDocShell));
    docShellAsNode->GetChildCount(&n);
    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        docShellAsNode->GetChildAt(i, getter_AddRefs(child));
        nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
        if (childAsShell) {
            DumpViewsRecur(childAsShell, out);
        }
    }
#endif // DEBUG
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpViews()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
    DumpViewsRecur(mDocShell, stdout);
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpStyleSheets()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
    FILE *out = stdout;
    nsCOMPtr<nsIPresShell> shell(pres_shell(mDocShell)); 
    if (shell)
        shell->ListStyleSheets(out);
    else
        fputs("null pres shell\n", out);
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpStyleContexts()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
    FILE *out = stdout;
    nsCOMPtr<nsIPresShell> shell(pres_shell(mDocShell)); 
    if (shell) {
        nsIFrame* root = shell->GetRootFrame();
        if (!root) {
            fputs("null root frame\n", out);
        } else {
            shell->ListStyleContexts(root, out);
        }
    } else {
        fputs("null pres shell\n", out);
    }
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpReflowStats()
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
    nsCOMPtr<nsIPresShell> shell(pres_shell(mDocShell)); 
    if (shell) {
#ifdef MOZ_REFLOW_PERF
        shell->DumpReflows();
#else
        printf("************************************************\n");
        printf("Sorry, you have not built with MOZ_REFLOW_PERF=1\n");
        printf("************************************************\n");
#endif
    }
#endif
    return NS_OK;
}

void nsLayoutDebuggingTools::ForceRefresh()
{
    nsCOMPtr<nsIViewManager> vm(view_manager(mDocShell));
    if (!vm)
        return;
    nsIView* root = vm->GetRootView();
    if (root) {
        vm->UpdateView(root, NS_VMREFRESH_IMMEDIATE);
    }
}

nsresult
nsLayoutDebuggingTools::SetBoolPrefAndRefresh(const char * aPrefName,
                                              bool aNewVal)
{
    NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);

    nsIPrefService* prefService = Preferences::GetService();
    NS_ENSURE_TRUE(prefService && aPrefName, NS_OK);

    Preferences::SetBool(aPrefName, aNewVal);
    prefService->SavePrefFile(nsnull);

    ForceRefresh();

    return NS_OK;
}
