/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:tabstop=4:expandtab:shiftwidth=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLayoutDebuggingTools.h"

#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"
#include "nsIContentViewer.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsService.h"

#include "nsAtom.h"
#include "nsQuickSort.h"

#include "nsIContent.h"

#include "nsCounterManager.h"
#include "nsCSSFrameConstructor.h"
#include "nsViewManager.h"
#include "nsIFrame.h"

#include "nsLayoutCID.h"

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

nsLayoutDebuggingTools::nsLayoutDebuggingTools() { ForceRefresh(); }

nsLayoutDebuggingTools::~nsLayoutDebuggingTools() = default;

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
  nsIFrame::ShowFrameBorders(aVisualDebugging);
  ForceRefresh();
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::SetVisualEventDebugging(bool aVisualEventDebugging) {
#ifdef DEBUG
  nsIFrame::ShowEventTargetFrameBorder(aVisualEventDebugging);
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

NS_IMETHODIMP
nsLayoutDebuggingTools::SetPagedMode(bool aPagedMode) {
  nsCOMPtr<nsIPrintSettingsService> printSettingsService =
      do_GetService("@mozilla.org/gfx/printsettings-service;1");
  nsCOMPtr<nsIPrintSettings> printSettings;

  printSettingsService->GetNewPrintSettings(getter_AddRefs(printSettings));

  // Use the same setup as setupPrintMode() in reftest-content.js.
  printSettings->SetPaperWidth(5);
  printSettings->SetPaperHeight(3);

  nsIntMargin unwriteableMargin(0, 0, 0, 0);
  printSettings->SetUnwriteableMarginInTwips(unwriteableMargin);

  printSettings->SetHeaderStrLeft(u""_ns);
  printSettings->SetHeaderStrCenter(u""_ns);
  printSettings->SetHeaderStrRight(u""_ns);

  printSettings->SetFooterStrLeft(u""_ns);
  printSettings->SetFooterStrCenter(u""_ns);
  printSettings->SetFooterStrRight(u""_ns);

  printSettings->SetPrintBGColors(true);
  printSettings->SetPrintBGImages(true);

  nsCOMPtr<nsIContentViewer> contentViewer(doc_viewer(mDocShell));
  contentViewer->SetPageModeForTesting(aPagedMode, printSettings);

  ForceRefresh();
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
  }
#endif
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpContent() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpContentRecur(mDocShell, stdout);
  return NS_OK;
}

static void DumpFramesRecur(
    nsIDocShell* aDocShell, FILE* out,
    nsIFrame::ListFlags aFlags = nsIFrame::ListFlags()) {
  if (aFlags.contains(nsIFrame::ListFlag::DisplayInCSSPixels)) {
    fprintf(out, "Frame tree in CSS pixels:\n");
  } else {
    fprintf(out, "Frame tree in app units:\n");
  }

  fprintf(out, "docshell=%p \n", aDocShell);
  if (PresShell* presShell = GetPresShell(aDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->List(out, "", aFlags);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

static void DumpTextRunsRecur(nsIDocShell* aDocShell, FILE* out) {
  fprintf(out, "Text runs:\n");

  fprintf(out, "docshell=%p \n", aDocShell);
  if (PresShell* presShell = GetPresShell(aDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->ListTextRuns(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpFrames() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpFramesRecur(mDocShell, stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpFramesInCSSPixels() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpFramesRecur(mDocShell, stdout, nsIFrame::ListFlag::DisplayInCSSPixels);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpTextRuns() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpTextRunsRecur(mDocShell, stdout);
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
#endif  // DEBUG
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpViews() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  DumpViewsRecur(mDocShell, stdout);
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpCounterManager() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    presShell->FrameConstructor()->CounterManager()->Dump();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLayoutDebuggingTools::DumpStyleSheets() {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_NOT_INITIALIZED);
#if defined(DEBUG) || defined(MOZ_LAYOUT_DEBUGGER)
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
  FILE* out = stdout;
  if (PresShell* presShell = GetPresShell(mDocShell)) {
    nsIFrame* root = presShell->GetRootFrame();
    if (root) {
      root->ListWithMatchedRules(out);
    }
  } else {
    fputs("null pres shell\n", out);
  }
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
