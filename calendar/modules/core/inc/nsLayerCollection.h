/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsLayerCollection_h___
#define nsLayerCollection_h___

#include "nscalexport.h"
#include "nsILayer.h"
#include "nsILayerCollection.h"
#include "nsIArray.h"


class NS_CALENDAR nsLayerCollection : public nsILayerCollection,
                                 nsILayer
{
  
public:
  nsLayerCollection(nsISupports* outer);
  ~nsLayerCollection();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD CreateIterator(nsIIterator ** aIterator) ;
  NS_IMETHOD AddLayer(nsILayer * aLayer);
  NS_IMETHOD RemoveLayer(nsILayer * aLayer);

  NS_IMETHOD SetShell(nsCalendarShell* aShell);
  NS_IMETHOD SetCurl(const JulianString& s);
  NS_IMETHOD GetCurl(JulianString& s);
  
  /**
   * Check to see if any layer matches the supplied curl.
   * In this case, matching means that the host and CSID 
   * values are equal.
   * @param  s  the curl
   * @return NS_OK on success
   */
  NS_IMETHOD URLMatch(const JulianString& aCurl, PRBool& aMatch);

  NS_IMETHOD SetCal(NSCalendar* aCal);
  NS_IMETHOD GetCal(NSCalendar*& aCal);
  NS_IMETHOD FetchEventsByRange(
                      DateTime* aStart, 
                      DateTime* aStop, JulianPtrArray* aL
                      );

  NS_IMETHOD StoreEvent(VEvent& addEvent);
private:
  nsIArray * mLayers ;
  nsCalendarShell * mpShell;

};

#endif //nsLayerCollection_h___
