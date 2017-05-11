#ifndef nsIPresShellInlines_h
#define nsIPresShellInlines_h

#include "nsIDocument.h"

void
nsIPresShell::SetNeedLayoutFlush()
{
  mNeedLayoutFlush = true;
  if (nsIDocument* doc = mDocument->GetDisplayDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->mNeedLayoutFlush = true;
    }
  }
}

void
nsIPresShell::SetNeedStyleFlush()
{
  mNeedStyleFlush = true;
  if (nsIDocument* doc = mDocument->GetDisplayDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->mNeedStyleFlush = true;
    }
  }
}

void
nsIPresShell::EnsureStyleFlush()
{
  SetNeedStyleFlush();
  ObserveStyleFlushes();
}

void
nsIPresShell::SetNeedThrottledAnimationFlush()
{
  mNeedThrottledAnimationFlush = true;
  if (nsIDocument* doc = mDocument->GetDisplayDocument()) {
    if (nsIPresShell* shell = doc->GetShell()) {
      shell->mNeedThrottledAnimationFlush = true;
    }
  }
}

#endif // nsIPresShellInlines_h
