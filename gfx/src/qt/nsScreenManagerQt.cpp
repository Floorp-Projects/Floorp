/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
 *   John C. Griggs <johng@corel.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsScreenManagerQt.h"
#include "nsScreenQt.h"

#include "qdesktopwidget.h"
#include "qapplication.h"
#include "qtlog.h"

#ifdef DEBUG
PRUint32 gSMCount = 0;
PRUint32 gSMID = 0;
#endif

nsScreenManagerQt::nsScreenManagerQt()
{
#ifdef DEBUG
    gSMCount++;
    mID = gSMID++;
    PR_LOG(gQtLogModule, QT_BASIC,
           ("nsScreenManagerQt CTOR (%p) ID: %d, Count: %d\n", this, mID, gSMCount));
#endif

    desktop = 0;
}


nsScreenManagerQt::~nsScreenManagerQt()
{
#ifdef DEBUG
    gSMCount--;
    PR_LOG(gQtLogModule, QT_BASIC,
           ("nsScreenManagerQt DTOR (%p) ID: %d, Count: %d\n", this, mID, gSMCount));
#endif
    // nothing to see here.
}

// addref, release, QI
NS_IMPL_ISUPPORTS1(nsScreenManagerQt, nsIScreenManager)

void nsScreenManagerQt::init()
{
    if (desktop)
        return;

    desktop = QApplication::desktop();
    nScreens = desktop->numScreens();
    screens = new nsCOMPtr<nsIScreen>[nScreens];

    for (int i = 0; i < nScreens; ++i)
        screens[i] = new nsScreenQt(i);
}

//
// ScreenForRect
//
// Returns the screen that contains the rectangle. If the rect overlaps
// multiple screens, it picks the screen with the greatest area of intersection.
//
// The coordinates are in pixels (not twips) and in screen coordinates.
//
NS_IMETHODIMP
nsScreenManagerQt::ScreenForRect(PRInt32 inLeft, PRInt32 inTop,
				 PRInt32 inWidth, PRInt32 inHeight,
				 nsIScreen **outScreen)
{
    if (!desktop)
        init();

    QRect r(inLeft, inTop, inWidth, inHeight);
    int best = 0;
    int area = 0;
    for (int i = 0; i < nScreens; ++i) {
        const QRect& rect = desktop->screenGeometry(i);
        QRect intersection = r&rect;
        int a = intersection.width()*intersection.height();
        if (a > area) {
            best = i;
            area = a;
        }
    }
    NS_IF_ADDREF(*outScreen = screens[best]);
    return NS_OK;
}

//
// GetPrimaryScreen
//
// The screen with the menubar/taskbar. This shouldn't be needed very
// often.
//
NS_IMETHODIMP
nsScreenManagerQt::GetPrimaryScreen(nsIScreen **aPrimaryScreen)
{
    if (!desktop)
        init();

    NS_IF_ADDREF(*aPrimaryScreen = screens[0]);
    return NS_OK;
}

//
// GetNumberOfScreens
//
// Returns how many physical screens are available.
//
NS_IMETHODIMP
nsScreenManagerQt::GetNumberOfScreens(PRUint32 *aNumberOfScreens)
{
    if (!desktop)
        init();

    *aNumberOfScreens = desktop->numScreens();
    return NS_OK;
}
