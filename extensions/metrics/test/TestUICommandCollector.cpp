/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is the Metrics extension.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@gmail.com>
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

// Unit test for nsUICommandCollector

#include <stdio.h>

#include "TestCommon.h"
#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMText.h"
#include "nsIDOMXULCommandEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsComponentManagerUtils.h"
#include "nsUICommandCollector.h"
#include "nsMetricsService.h"

// This is a little unconventional, but it lets us register the metrics
// components statically so that they can initialize properly in the test
// environment, while still allowing us access to non-interface methods.
#include "nsMetricsModule.cpp"

static int gTotalTests = 0;
static int gPassedTests = 0;

class UICommandCollectorTest
{
 public:
  UICommandCollectorTest() {}
  ~UICommandCollectorTest() {}

  void SetUp();
  void TestGetEventTargets();
  void TestGetEventKeyId();

 private:
  nsRefPtr<nsUICommandCollector> mCollector;
  nsCOMPtr<nsIDOMDocument> mDocument;
  nsCOMPtr<nsIPrivateDOMEvent> mDOMEvent;
  nsString kXULNS;
};

void UICommandCollectorTest::SetUp()
{
  ++gTotalTests;
  mCollector = new nsUICommandCollector();
  mDocument = do_CreateInstance("@mozilla.org/xml/xml-document;1");
  kXULNS.Assign(NS_LITERAL_STRING("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"));
}

void UICommandCollectorTest::TestGetEventTargets()
{
  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("xulcommandevents"),
                        getter_AddRefs(event));

  // The private event interface lets us directly set the target
  // and original target for easier testing.
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  ASSERT_TRUE(privateEvent);

  nsCOMPtr<nsIDOMElement> buttonElement;
  mDocument->CreateElementNS(kXULNS, NS_LITERAL_STRING("button"),
                            getter_AddRefs(buttonElement));
  ASSERT_TRUE(buttonElement);
  buttonElement->SetAttribute(NS_LITERAL_STRING("id"),
                              NS_LITERAL_STRING("btn1"));

  // First test the simple case where we have only explicit content.
  // Note that originalTarget == target unless set otherwise.
  privateEvent->SetTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(buttonElement)));

  nsString targetId, targetAnonId;
  nsresult rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(targetId.Equals(NS_LITERAL_STRING("btn1")));
  ASSERT_TRUE(targetAnonId.IsEmpty());

  // If there was an anonid, it should be ignored
  buttonElement->SetAttribute(NS_LITERAL_STRING("anonid"),
                              NS_LITERAL_STRING("abc"));
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(targetId.Equals(NS_LITERAL_STRING("btn1")));
  ASSERT_TRUE(targetAnonId.IsEmpty());

  // If the target has no id, GetEventTargets should fail
  buttonElement->RemoveAttribute(NS_LITERAL_STRING("id"));
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_FAILED(rv));

  // The same should be true with no anonid
  buttonElement->RemoveAttribute(NS_LITERAL_STRING("anonid"));
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_FAILED(rv));

  // Empty attributes should work the same as no attributes
  buttonElement->SetAttribute(NS_LITERAL_STRING("id"), nsString());
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_FAILED(rv));

  // Now test some cases where the originalTarget is different
  nsCOMPtr<nsIDOMElement> anonChild;
  mDocument->CreateElementNS(kXULNS, NS_LITERAL_STRING("hbox"),
                            getter_AddRefs(anonChild));
  ASSERT_TRUE(anonChild);

  privateEvent->SetOriginalTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(anonChild)));

  // This is the typical id/anonid case for anonymous content
  buttonElement->SetAttribute(NS_LITERAL_STRING("id"),
                              NS_LITERAL_STRING("btn1"));
  anonChild->SetAttribute(NS_LITERAL_STRING("anonid"),
                          NS_LITERAL_STRING("box1"));
  targetId.SetLength(0);
  targetAnonId.SetLength(0);
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(targetId.Equals(NS_LITERAL_STRING("btn1")));
  ASSERT_TRUE(targetAnonId.Equals(NS_LITERAL_STRING("box1")));

  // If there is no id or anonid, it should fail
  anonChild->RemoveAttribute(NS_LITERAL_STRING("anonid"));
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_FAILED(rv));

  // Test the failure case where the target/originalTarget is not an element
  privateEvent->SetOriginalTarget(nsnull);  // now mirrors target again
  nsCOMPtr<nsIDOMText> textNode;
  mDocument->CreateTextNode(NS_LITERAL_STRING("blah"),
                           getter_AddRefs(textNode));
  ASSERT_TRUE(textNode);
  privateEvent->SetTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(textNode)));
  rv = mCollector->GetEventTargets(event, targetId, targetAnonId);
  ASSERT_TRUE(NS_FAILED(rv));

  ++gPassedTests;
}

void UICommandCollectorTest::TestGetEventKeyId()
{
  nsCOMPtr<nsIDOMDocumentEvent> docEvent = do_QueryInterface(mDocument);
  nsCOMPtr<nsIDOMEvent> event;
  docEvent->CreateEvent(NS_LITERAL_STRING("xulcommandevents"),
                        getter_AddRefs(event));

  // The private event interface lets us directly set the target
  // and original target for easier testing.
  nsCOMPtr<nsIPrivateDOMEvent> privateEvent = do_QueryInterface(event);
  ASSERT_TRUE(privateEvent);

  nsCOMPtr<nsIDOMElement> elem;
  mDocument->CreateElementNS(kXULNS, NS_LITERAL_STRING("hbox"),
                             getter_AddRefs(elem));
  ASSERT_TRUE(elem);

  privateEvent->SetTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(elem)));

  // In its initial state, the command event will have no source event.
  // GetEventKeyId should leave keyId empty.
  nsString keyId;
  mCollector->GetEventKeyId(event, keyId);
  ASSERT_TRUE(keyId.IsEmpty());

  // Now set up a source event
  nsCOMPtr<nsIDOMEvent> sourceEvent;
  docEvent->CreateEvent(NS_LITERAL_STRING("Events"),
                        getter_AddRefs(sourceEvent));
  nsCOMPtr<nsIPrivateDOMEvent> privateSource = do_QueryInterface(sourceEvent);
  ASSERT_TRUE(privateSource);

  nsCOMPtr<nsIDOMXULCommandEvent> xcEvent = do_QueryInterface(event);
  ASSERT_TRUE(xcEvent);
  nsresult rv = xcEvent->InitCommandEvent(NS_LITERAL_STRING("command"),
                                          PR_TRUE, PR_TRUE, nsnull, 0,
                                          PR_FALSE, PR_FALSE, PR_FALSE,
                                          PR_FALSE, sourceEvent);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // The source event will initially point to a non-key element
  privateSource->SetTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(elem)));

  mCollector->GetEventKeyId(event, keyId);
  ASSERT_TRUE(keyId.IsEmpty());

  // Create a key element and point the source event there
  nsCOMPtr<nsIDOMElement> keyElem;
  mDocument->CreateElementNS(kXULNS, NS_LITERAL_STRING("key"),
                             getter_AddRefs(keyElem));
  ASSERT_TRUE(keyElem);
  keyElem->SetAttribute(NS_LITERAL_STRING("id"), NS_LITERAL_STRING("keyFoo"));
  privateSource->SetTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(keyElem)));

  mCollector->GetEventKeyId(event, keyId);
  ASSERT_TRUE(keyId.Equals(NS_LITERAL_STRING("keyFoo")));

  // Make sure we don't crash if the source event target is a non-element
  nsCOMPtr<nsIDOMText> textNode;
  mDocument->CreateTextNode(NS_LITERAL_STRING("blah"),
                            getter_AddRefs(textNode));
  ASSERT_TRUE(textNode);
  privateSource->SetTarget(
      nsCOMPtr<nsIDOMEventTarget>(do_QueryInterface(textNode)));
  keyId.SetLength(0);
  mCollector->GetEventKeyId(event, keyId);
  ASSERT_TRUE(keyId.IsEmpty());

  ++gPassedTests;
}

int main(int argc, char *argv[])
{
  nsStaticModuleInfo staticComps = { "metrics", &NSGetModule };
  NS_InitXPCOM3(nsnull, nsnull, nsnull, &staticComps, 1);
  // Pre-initialize the metrics service since it's assumed to exist
  nsMetricsService::get();

  // Use a separate instantiation of the test objects for each test
  {
    UICommandCollectorTest test;
    test.SetUp();
    test.TestGetEventTargets();
  }
  {
    UICommandCollectorTest test;
    test.SetUp();
    test.TestGetEventKeyId();
  }

  NS_ShutdownXPCOM(nsnull);

  printf("%d/%d tests passed\n", gPassedTests, gTotalTests);
  return 0;
}
