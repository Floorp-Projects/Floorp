/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIScriptLoaderObserver.h"
#include "nsIScriptElement.h"
#include "nsStubMutationObserver.h"

/**
 * Baseclass useful for script elements (such as <xhtml:script> and
 * <svg:script>). Currently the class assumes that only the 'src'
 * attribute and the children of the class affect what script to execute.
 */

class nsScriptElement : public nsIScriptElement,
                        public nsStubMutationObserver
{
public:
  // nsIScriptLoaderObserver
  NS_DECL_NSISCRIPTLOADEROBSERVER

  // nsIMutationObserver
  NS_DECL_NSIMUTATIONOBSERVER_CHARACTERDATACHANGED
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTAPPENDED
  NS_DECL_NSIMUTATIONOBSERVER_CONTENTINSERTED

  nsScriptElement(mozilla::dom::FromParser aFromParser)
    : nsIScriptElement(aFromParser)
  {
  }

  virtual nsresult FireErrorEvent();

protected:
  // Internal methods

  /**
   * Check if this element contains any script, linked or inline
   */
  virtual bool HasScriptContent() = 0;

  virtual bool MaybeProcessScript();
};
