
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
 * The Original Code is nsGenConImageContent.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2004
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
#include "nsContentCreatorFunctions.h"
#include "nsXMLElement.h"
#include "nsImageLoadingContent.h"
#include "imgIRequest.h"

/**
 * A fake content node class so that the image frame for
 *   p:before { content: url(foo.gif); }
 * can have a content node that knows about image loading but can take
 * an imgIRequest that's already been loaded from the style system.
 */
class nsGenConImageContent : public nsXMLElement,
                             public nsImageLoadingContent
{
public:
  nsGenConImageContent(nsINodeInfo* aNodeInfo)
    : nsXMLElement(aNodeInfo)
  {
  }

  nsresult Init(imgIRequest* aImageRequest)
  {
    return aImageRequest->Clone(this, getter_AddRefs(mCurrentRequest));
  }
private:
  ~nsGenConImageContent() {}

public:
  NS_DECL_ISUPPORTS_INHERITED
};

NS_IMPL_ISUPPORTS_INHERITED2(nsGenConImageContent, nsXMLElement,
                             nsIImageLoadingContent, imgIDecoderObserver)

nsresult
NS_NewGenConImageContent(nsIContent** aResult, nsINodeInfo* aNodeInfo,
                         imgIRequest* aImageRequest)
{
  nsGenConImageContent *it = new nsGenConImageContent(aNodeInfo);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult = it);
  nsresult rv = it->Init(aImageRequest);
  if (NS_FAILED(rv))
    NS_RELEASE(*aResult);
  return rv;
}
