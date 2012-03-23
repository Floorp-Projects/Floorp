/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Qt code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Oleg Romashin <romaxa@gmail.com>
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

#include "QTMLocationProvider.h"
#include "nsGeoPosition.h"

using namespace QtMobility;

using namespace mozilla;

NS_IMPL_ISUPPORTS1(QTMLocationProvider, nsIGeolocationProvider)

QTMLocationProvider::QTMLocationProvider()
{
    mLocation = QGeoPositionInfoSource::createDefaultSource(this);
    if (mLocation)
        connect(mLocation, SIGNAL(positionUpdated(QGeoPositionInfo)), this, SLOT(positionUpdated(QGeoPositionInfo)));
}

QTMLocationProvider::~QTMLocationProvider()
{
    delete mLocation;
}

void
QTMLocationProvider::positionUpdated(const QGeoPositionInfo &geoPosition)
{
    if (!geoPosition.isValid()) {
        NS_WARNING("Invalida geoposition received");
        return;
    }

    QGeoCoordinate coord = geoPosition.coordinate();
    double latitude = coord.latitude();
    double longitude = coord.longitude();
    double altitude = coord.altitude();
    double accuracy = geoPosition.attribute(QGeoPositionInfo::HorizontalAccuracy);
    double altitudeAccuracy = geoPosition.attribute(QGeoPositionInfo::VerticalAccuracy);
    double heading = geoPosition.attribute(QGeoPositionInfo::Direction);

    bool providesSpeed = geoPosition.hasAttribute(QGeoPositionInfo::GroundSpeed);
    double speed = geoPosition.attribute(QGeoPositionInfo::GroundSpeed);

    nsRefPtr<nsGeoPosition> p =
        new nsGeoPosition(latitude, longitude,
                          altitude, accuracy,
                          altitudeAccuracy, heading,
                          speed, geoPosition.timestamp().toTime_t());
    if (mCallback) {
        mCallback->Update(p);
    }
}

NS_IMETHODIMP
QTMLocationProvider::Startup()
{
    if (!mLocation)
        return NS_ERROR_NOT_IMPLEMENTED;

    mLocation->startUpdates();

    return NS_OK;
}

NS_IMETHODIMP
QTMLocationProvider::Watch(nsIGeolocationUpdate* aCallback)
{
    mCallback = aCallback;

    return NS_OK;
}

NS_IMETHODIMP
QTMLocationProvider::Shutdown()
{
    if (!mLocation)
        return NS_ERROR_NOT_IMPLEMENTED;

    mLocation->stopUpdates();
    mCallback = nsnull;

    return NS_OK;
}

NS_IMETHODIMP
QTMLocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}
