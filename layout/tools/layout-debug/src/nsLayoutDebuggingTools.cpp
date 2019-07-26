/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutDebuggingTools.h"

#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIContentViewer.h"

#include "nsIServiceManager.h"
#include "nsAtom.h"
#include "nsQuickSort.h"

#include "nsIContent.h"

#include "nsViewManager.h"
#include "nsFrame.h"

#include "nsLayoutCID.h"

#include "nsISelectionController.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"

using namespace mozilla;
using mozilla::dom::Document;

static already_AddRefed<nsIContentViewer> doc_viewer(nsIDocShell* aDocShell) {
  if (!aDocShell) return nullptr;
  nsCOMPtr<nsIContentViewer> result;
  aDocShell->GetContentViewer(getter_AddRefs(result));
  return result.forget();
}

static PresShell* GetPresShell(nsIDocShell* aDocShell) {
  nsCOMPtr<nsIContentViewer> cv = doc_viewer(aDocShell);
  if (!cv) return nullptr;
  return cv->GetPresShell();
}

static nsViewManager* view_manager(nsIDocShell* aDocShell) {
  PresShell* presShell = GetPresShell(aDocShell);
  if (!presShell) {
    return nullptr;
  }
  return presShell->GetViewManager();
}

#ifdef DEBUG
static already_AddRefed<Document> document(nsIDocShell* aDocShell) {
  nsCOMPtr<nsIContentViewer> cv(doc_viewer(aDocShell));
  if (!cv) return nullptr;
  RefPtr<Document> result = cv->GetDocument();
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
      mReflowCounts(false) {
  ForceRefresh();
}

nsLayoutDebuggingTools::~nsLayoutDebuggingTools() {}

NS_IMPL_ISUPPORTS(nsLayoutDebuggingTools, nsILayoutDebuggingTools)

NS_IMETHODIMP
nsLayoutDebuggingTools::Init(mozIDOMWindow* aWin) {
  if (!Preferences::GetService()) {
    return NS_ERROR_UNEXPECTED;
  }

  {
    if (!aWin) return NS_ERROR_UNEXPECTED;
    auto* window = nsPIDOMWindowInner::From(aWin);
    mDocShell = window->GetDocShell();
  }
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_UNEXPECTED);

  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetVisualDebugging(bool aVisualDebugging) {
#ifdef DEBUG
  nsFrame::ShowFrameBorders(aVisualDebugging);
  ForceRefresh();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetVisualEventDebugging(bool aVisualEventDebugging) {
#ifdef DEBUG
  nsFrame::ShowEventTargetFrameBorder(aVisualEventDebugging);
  ForceRefresh();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetReflowCounts(bool aShow) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  if (PresShell* presShell = GetPresShell(mDocShell)) {
#ifdef MOZ_REFLOW_PERF
    presShell->SetPaintFrameCount(aShow);
#else
    printf("************************************************\n");
    printf("Sorry, you have not built with MOZ_REFLOW_PERF=1\n");
    printf("************************************************\n");
#endif
  }
  return NS_OK;
}

static void DumpAWebShell(nsIDocShellTreeItem* aShellItem, FILE* out,
                          int32_t aIndent) {
  nsString name;
  nsCOMPtr<nsIDocShellTreeItem> parent;
  int32_t i, n;

  for (i = aIndent; --i >= 0;) fprintf(out, "  ");

  fprintf(out, "%p '", static_cast<void*>(aShellItem));
  aShellItem->GetName(name);
  aShellItem->GetInProcessSameTypeParent(getter_AddRefs(parent));
  fputs(NS_LossyConvertUTF16toASCII(name).get(), out);
  fprintf(out, "' parent=%p <\n", static_cast<void*>(parent));

  ++aIndent;
  aShellItem->GetInProcessChildCount(&n);
  for (i = 0; i < n; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aShellItem->GetInProcessChildAt(i, getter_AddRefs(child));
    if (child) {
      DumpAWebShell(child, out, aIndent);
    }
  }
  --aIndent;
  for (i = aIndent; --i >= 0;) fprintf(out, "  ");
  fputs(">\n", out);
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpWebShells() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpAWebShell(mDocShell, stdout, 0);
  return NS_OK;
}

static void DumpContentRecur(nsIDocShell* aDocShell, FILE* out) {
#ifdef DEBUG
  if (nullptr != aDocShell) {
    fprintf(out, "docshell=%p \n", static_cast<void*>(aDocShell));
    RefPtr<Document> doc(document(aDocShell));
    if (doc) {
      dom::Element* root = doc->GetRootElement();
      if (root) {
        root->List(out);
      }
    } else {
      fputs("no document\n", out);
    }
    // dump the frames of the sub documents
    int32_t i, n;
    aDocShell->GetInProcessChildCount(&n);
    for (i = 0; i < n; ++i) {
      nsCOMPtr<nsIDocShellTreeItem> child;
      aDocShell->GetInProcessChildAt(i, getter_AddRefs(child));
      nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
      if (child) {
        DumpContentRecur(childAsShell, out);
      }
    }
  }
#endif
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpContent() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpContentRecur(mDocShell, stdout);
  return NS_OK;
}

static void DumpFramesRecur(nsIDocShell* aDocShell, FILE* out) {
#ifdef DEBUG_FRAME_DUMP
  fprintf(out, "webshell=%p \n", static_cast<void*>(aDocShell));
  if (PresShell* presShell = GetPresShell(aDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->List(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }

  // dump the frames of the sub documents
  int32_t i, n;
  aDocShell->GetInProcessChildCount(&n);
  for (i = 0; i < n; ++i) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aDocShell->GetInProcessChildAt(i, getter_AddRefs(child));
    nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
    if (childAsShell) {
      DumpFramesRecur(childAsShell, out);
    }
  }
#endif
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpFrames() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpFramesRecur(mDocShell, stdout);
  return NS_OK;
}

static void DumpViewsRecur(nsIDocShell* aDocShell, FILE* out) {
#ifdef DEBUG
  fprintf(out, "docshell=%p \n", static_cast<void*>(aDocShell));
  RefPtr<nsViewManager> vm(view_manager(aDocShell));
  if (vm) {
    nsView* root = vm->GetRootView();
    if (root) {
      root->List(out);
    }
  } else {
    fputs("null view manager\n", out);
  }

  // dump the views of the sub documents
  int32_t i, n;
  aDocShell->GetInProcessChildCount(&n);
  for (i = 0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> child;
    aDocShell->GetInProcessChildAt(i, getter_AddRefs(child));
    nsCOMPtr<nsIDocShell> childAsShell(do_QueryInterface(child));
    if (childAsShell) {
      DumpViewsRecur(childAsShell, out);
    }
  }
#endif  // DEBUG
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpViews() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpViewsRecur(mDocShell, stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpStyleSheets() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->ListStyleSheets(out);
  } else {
    fputs("null pres shell\n", out);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsLayoutDebuggingTools::DumpMatchedRules() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG_FRAME_DUMP
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->ListWithMatchedRules(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpComputedStyles() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->ListComputedStyles(out);
  } else {
    fputs("null pres shell\n", out);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpReflowStats() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#ifdef DEBUG
  if (RefPtr<PresShell> presShell = GetPresShell(mDocShell)) {
#  ifdef MOZ_REFLOW_PERF
    presShell->DumpReflows();
#  else
    printf("************************************************\n");
    printf("Sorry, you have not built with MOZ_REFLOW_PERF=1\n");
    printf("************************************************\n");
#  endif
  }
#endif
  return NS_OK;
}

nsresult nsLayoutDebuggingTools::ForceRefresh() {
  RefPtr<nsViewManager> vm(view_manager(mDocShell));
  if (!vm) return NS_OK;
  nsView* root = vm->GetRootView();
  if (root) {
    vm->InvalidateView(root);
  }
  return NS_OK;
}

nsresult nsLayoutDebuggingTools::SetBoolPrefAndRefresh(const char* aPrefName,
                                                       bool aNewVal) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);

  nsIPrefService* prefService = Preferences::GetService();
  NS_ENSURE_TRUE(prefService && aPrefName, NS_OK);

  Preferences::SetBool(aPrefName, aNewVal);
  prefService->SavePrefFile(nullptr);

  ForceRefresh();

  return NS_OK;
}
