/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutDebuggingTools.h"

#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIContentViewer.h"

#include "nsIServiceManager.h"
#include "nsIAtom.h"
#include "nsQuickSort.h"

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"

#include "nsIPresShell.h"
#include "nsViewManager.h"
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
        return nullptr;
    nsCOMPtr<nsIContentViewer> result;
    aDocShell->GetContentViewer(getter_AddRefs(result));
    return result.forget();
}

static already_AddRefed<nsIPresShell>
pres_shell(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIContentViewer> cv = doc_viewer(aDocShell);
    if (!cv)
        return nullptr;
    nsCOMPtr<nsIPresShell> result;
    cv->GetPresShell(getter_AddRefs(result));
    return result.forget();
}

static nsViewManager*
view_manager(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIPresShell> shell(pres_shell(aDocShell));
    if (!shell)
        return nullptr;
    return shell->GetViewManager();
}

#ifdef DEBUG
static already_AddRefed<nsIDocument>
document(nsIDocShell *aDocShell)
{
    nsCOMPtr<nsIContentViewer> cv(doc_viewer(aDocShell));
    if (!cv)
        return nullptr;
    nsCOMPtr<nsIDOMDocument> domDoc;
    cv->GetDOMDocument(getter_AddRefs(domDoc));
    if (!domDoc)
        return nullptr;
    nsCOMPtr<nsIDocument> result = do_QueryInterface(domDoc);
    return result.forget();
}
#endif

nsLayoutDebuggingTools::nsLayoutDebuggingTools()
  : mPaintFlashing(false),
    mPaintDumping(false),
    mInvalidateDumping(false),
    mEventDumping(false),
    mMotionEventDumping(false),
    mCrossingEventDumping(false),
    mReflowCounts(false)
{
    NewURILoaded();
}

nsLayoutDebuggingTools::~nsLayoutDebuggingTools()
{
}

NS_IMPL_ISUPPORTS(nsLayoutDebuggingTools, nsILayoutDebuggingTools)

NS_IMETHODIMP
nsLayoutDebuggingTools::Init(mozIDOMWindow* aWin)
{
    if (!Preferences::GetService()) {
        return NS_ERROR_UNEXPECTED;
    }

    {
        if (!aWin)
            return NS_ERROR_UNEXPECTED;
        auto* window = nsPIDOMWindowInner::From(aWin);
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
    mEditorMode = false;
    mVisualDebugging = false;
    mVisualEventDebugging = false;

    mReflowCounts = false;

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

static void DumpAWebShell(nsIDocShellTreeItem* aShellItem, FILE* out, int32_t aIndent)
{
    nsString name;
    nsCOMPtr<nsIDocShellTreeItem> parent;
    int32_t i, n;

    for (i = aIndent; --i >= 0; )
        fprintf(out, "  ");

    fprintf(out, "%p '", static_cast<void*>(aShellItem));
    aShellItem->GetName(name);
    aShellItem->GetSameTypeParent(getter_AddRefs(parent));
    fputs(NS_LossyConvertUTF16toASCII(name).get(), out);
    fprintf(out, "' parent=%p <\n", static_cast<void*>(parent));

    ++aIndent;
    aShellItem->GetChildCount(&n);
    for (i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        aShellItem->GetChildAt(i, getter_AddRefs(child));
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
    DumpAWebShell(mDocShell, stdout, 0);
    return NS_OK;
}

static
void
DumpContentRecur(nsIDocShell* aDocShell, FILE* out)
{
#ifdef DEBUG
    if (nullptr != aDocShell) {
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
        int32_t i, n;
        aDocShell->GetChildCount(&n);
        for (i = 0; i < n; ++i) {
            nsCOMPtr<nsIDocShellTreeItem> child;
            aDocShell->GetChildAt(i, getter_AddRefs(child));
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
            root->List(out);
        }
    }
    else {
        fputs("null pres shell\n", out);
    }

    // dump the frames of the sub documents
    int32_t i, n;
    aDocShell->GetChildCount(&n);
    for (i = 0; i < n; ++i) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        aDocShell->GetChildAt(i, getter_AddRefs(child));
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
    RefPtr<nsViewManager> vm(view_manager(aDocShell));
    if (vm) {
        nsView* root = vm->GetRootView();
        if (root) {
            root->List(out);
        }
    }
    else {
        fputs("null view manager\n", out);
    }

    // dump the views of the sub documents
    int32_t i, n;
    aDocShell->GetChildCount(&n);
    for (i = 0; i < n; i++) {
        nsCOMPtr<nsIDocShellTreeItem> child;
        aDocShell->GetChildAt(i, getter_AddRefs(child));
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
    RefPtr<nsViewManager> vm(view_manager(mDocShell));
    if (!vm)
        return;
    nsView* root = vm->GetRootView();
    if (root) {
        vm->InvalidateView(root);
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
    prefService->SavePrefFile(nullptr);

    ForceRefresh();

    return NS_OK;
}
