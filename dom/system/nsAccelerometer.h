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
 * The Initial Developer of the Original Code is
 * Doug Turner <dougt@dougt.org>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsAccelerometer_h
#define nsAccelerometer_h

#include "nsIAccelerometer.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsITimer.h"

#define NS_ACCELEROMETER_CID \
{ 0xecba5203, 0x77da, 0x465a, \
{ 0x86, 0x5e, 0x78, 0xb7, 0xaf, 0x10, 0xd8, 0xf7 } }

#define NS_ACCELEROMETER_CONTRACTID "@mozilla.org/accelerometer;1"

class nsIDOMWindow;

class nsAccelerometer : public nsIAccelerometerUpdate
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACCELEROMETER
  NS_DECL_NSIACCELEROMETERUPDATE

  nsAccelerometer();

  virtual ~nsAccelerometer();

  double mLastX;
  double mLastY;
  double mLastZ;

private:
  nsCOMArray<nsIAccelerationListener> mListeners;
  nsCOMArray<nsIDOMWindow> mWindowListeners;

  void StartDisconnectTimer();

  PRBool mStarted;
  PRBool mNewListener;

  nsCOMPtr<nsITimer> mTimeoutTimer;
  static void TimeoutHandler(nsITimer *aTimer, void *aClosure);

 protected:

  PRUint32 mUpdateInterval;
  PRBool   mEnabled;

  virtual void Startup()  = 0;
  virtual void Shutdown() = 0;
};

#endif
