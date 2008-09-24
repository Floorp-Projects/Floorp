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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Olli Pettay <Olli.Pettay@helsinki.fi> (Original Author)
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

#include "TestHarness.h"

#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsServiceManagerUtils.h"
#include "nsStringGlue.h"
#include "nsContentCID.h"
#include "nsPIDOMEventTarget.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventListener.h"

#define REPORT_ERROR(_msg)                  \
  printf("FAIL " _msg "\n")

#define TEST_FAIL(_msg)                     \
  PR_BEGIN_MACRO                            \
    REPORT_ERROR(_msg);                     \
    return NS_ERROR_FAILURE;                \
  PR_END_MACRO

#define TEST_ENSURE_TRUE(_test, _msg)       \
  PR_BEGIN_MACRO                            \
    if (!(_test)) {                         \
      TEST_FAIL(_msg);                      \
    }                                       \
  PR_END_MACRO

static NS_DEFINE_IID(kXMLDocumentCID, NS_XMLDOCUMENT_CID);

static PRBool gDidHandleEventSuccessfully = PR_FALSE;
static PRBool gListenerIsAlive = PR_FALSE;
static nsIDOMEventTarget* gEventTarget = nsnull;

class TestEventListener : public nsIDOMEventListener
{
public:
  TestEventListener() { gListenerIsAlive = PR_TRUE; }
  virtual ~TestEventListener() { gListenerIsAlive = PR_FALSE; }
  NS_DECL_ISUPPORTS
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent)
  {
    gDidHandleEventSuccessfully = gListenerIsAlive;
    TEST_ENSURE_TRUE(gListenerIsAlive,
                     "Event listener got killed too early");
    gEventTarget->RemoveEventListener(NS_LITERAL_STRING("Foo"), this, PR_FALSE);
    gDidHandleEventSuccessfully = gListenerIsAlive;
    TEST_ENSURE_TRUE(gListenerIsAlive,
                     "Event listener got killed too early (2)");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS1(TestEventListener, nsIDOMEventListener)

nsresult
TestEventListeners()
{
  // Test that event listeners are kept alive long enough.
  {
    nsCOMPtr<nsIDocument> doc = do_CreateInstance(kXMLDocumentCID);
    TEST_ENSURE_TRUE(doc, "Couldn't create a document!");

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(doc);
    TEST_ENSURE_TRUE(target, "Document doesn't implement nsIDOMEventTarget?");
    gEventTarget = target;

    TestEventListener* listener = new TestEventListener();
    TEST_ENSURE_TRUE(listener, "Couldn't create event listener!");

    target->AddEventListener(NS_LITERAL_STRING("Foo"), listener, PR_FALSE);

    nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(doc);
    TEST_ENSURE_TRUE(docEvent,
                     "Document doesn't implement nsIDOMDocumentEvent?");

    nsCOMPtr<nsIDOMEvent> event;
    docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
    TEST_ENSURE_TRUE(event, "Couldn't create an event!");

    event->InitEvent(NS_LITERAL_STRING("Foo"), PR_TRUE, PR_TRUE);

    PRBool noPreventDefault = PR_FALSE;
    target->DispatchEvent(event, &noPreventDefault);
    TEST_ENSURE_TRUE(gDidHandleEventSuccessfully,
                     "The listener didn't handle the event successfully!");
  }
  TEST_ENSURE_TRUE(!gListenerIsAlive, "Event listener wasn't deleted!");

  printf("Testing event listener PASSED!\n");

  // Add new tests here...
  return NS_OK;
}

int main(int argc, char** argv)
{
  ScopedXPCOM xpcom("EventListeners");
  if (xpcom.failed())
    return 1;

  int retval = 0;
  if (NS_FAILED(TestEventListeners())) {
    retval = 1;
  }

  return retval;
}
