/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QTMLocationProvider.h"
#include "nsGeoPosition.h"

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
    mCallback = nullptr;

    return NS_OK;
}

NS_IMETHODIMP
QTMLocationProvider::SetHighAccuracy(bool)
{
  return NS_OK;
}
