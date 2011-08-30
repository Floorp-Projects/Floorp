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

#include "nsDOMMemoryReporter.h"
#include "nsGlobalWindow.h"


nsDOMMemoryReporter::nsDOMMemoryReporter()
{
}

NS_IMPL_ISUPPORTS1(nsDOMMemoryReporter, nsIMemoryReporter)

/* static */
void
nsDOMMemoryReporter::Init()
{
  // The memory reporter manager is going to own this object.
  NS_RegisterMemoryReporter(new nsDOMMemoryReporter());
}

NS_IMETHODIMP
nsDOMMemoryReporter::GetProcess(nsACString &aProcess)
{
  // "" means the main process.
  aProcess.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryReporter::GetPath(nsACString &aMemoryPath)
{
  aMemoryPath.AssignLiteral("explicit/dom");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryReporter::GetKind(PRInt32* aKind)
{
  *aKind = KIND_HEAP;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryReporter::GetDescription(nsACString &aDescription)
{
  aDescription.AssignLiteral("Memory used by the DOM.");
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMemoryReporter::GetUnits(PRInt32* aUnits)
{
  *aUnits = UNITS_BYTES;
  return NS_OK;
}

static
PLDHashOperator
GetWindowsMemoryUsage(const PRUint64& aId, nsGlobalWindow*& aWindow,
                      void* aClosure)
{
  *(PRInt64*)aClosure += aWindow->SizeOf();
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsDOMMemoryReporter::GetAmount(PRInt64* aAmount) {
  *aAmount = 0;

  nsGlobalWindow::WindowByIdTable* windows = nsGlobalWindow::GetWindowsTable();
  NS_ENSURE_TRUE(windows, NS_OK);

  windows->Enumerate(GetWindowsMemoryUsage, aAmount);

  return NS_OK;
}

