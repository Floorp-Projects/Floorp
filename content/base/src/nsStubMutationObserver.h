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
 * The Original Code is nsStubMutationObserver.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

/*
 * nsStubMutationObserver is an implementation of the nsIMutationObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#ifndef nsStubMutationObserver_h_
#define nsStubMutationObserver_h_

#include "nsIMutationObserver.h"
#include "nsIMutationObserver2.h"

/**
 * There are two advantages to inheriting from nsStubMutationObserver
 * rather than directly from nsIMutationObserver:
 *  1. smaller compiled code size (since there's no need for the code
 *     for the empty virtual function implementations for every
 *     nsIMutationObserver implementation)
 *  2. the performance of document's loop over observers benefits from
 *     the fact that more of the functions called are the same (which
 *     can reduce instruction cache misses and perhaps improve branch
 *     prediction)
 */
class nsStubMutationObserver : public nsIMutationObserver {
  NS_DECL_NSIMUTATIONOBSERVER
};

class nsStubMutationObserver2 : public nsIMutationObserver2 {
  NS_DECL_NSIMUTATIONOBSERVER2
};

#endif /* !defined(nsStubMutationObserver_h_) */
